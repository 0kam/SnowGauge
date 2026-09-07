/* LiDAR common layer - see lidar.h */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "lidar.h"

LOG_MODULE_REGISTER(lidar, CONFIG_LOG_DEFAULT_LEVEL);

/* 460800 baud = 46 bytes/ms; 512 bytes covers ~11 ms of scheduling latency. */
#define RX_RING_SIZE 512
#define MAX_SAMPLES  CONFIG_SNOWGAUGE_LIDAR_SAMPLES

#ifdef CONFIG_SNOWGAUGE_TFMINI_MIN_STRENGTH
#define MIN_STRENGTH CONFIG_SNOWGAUGE_TFMINI_MIN_STRENGTH
#else
#define MIN_STRENGTH 0 /* sensor without strength output */
#endif

static const struct device *const uart = DEVICE_DT_GET(DT_ALIAS(lidar_uart));

RING_BUF_DECLARE(rx_ring, RX_RING_SIZE);
static K_SEM_DEFINE(rx_sem, 0, 1);

static uint16_t checksum_errors;

/* burst sample storage */
static uint16_t dist_samples[MAX_SAMPLES];
static uint16_t str_samples[MAX_SAMPLES];

static void uart_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);
	uint8_t tmp[32];

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (!uart_irq_rx_ready(dev)) {
			continue;
		}
		int n = uart_fifo_read(dev, tmp, sizeof(tmp));

		if (n > 0) {
			/* On overflow the oldest bytes are lost; the parser resyncs. */
			ring_buf_put(&rx_ring, tmp, n);
			k_sem_give(&rx_sem);
		}
	}
}

int lidar_set_baud(uint32_t baud)
{
	/*
	 * The board overlay sets the TFmini rate (115200); the TSD20 wants
	 * 460800. Runtime configuration is kept by the nRF UARTE driver across
	 * the suspend/resume cycles done by sensor_rail.c.
	 */
	struct uart_config cfg = {
		.baudrate = baud,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	};
	int ret = uart_configure(uart, &cfg);

	if (ret) {
		LOG_ERR("uart_configure(%u) failed (%d)", baud, ret);
	}
	return ret;
}

int lidar_read_bytes(uint8_t *buf, size_t len, k_timeout_t timeout)
{
	k_timepoint_t end = sys_timepoint_calc(timeout);
	size_t n = 0;

	while (n < len) {
		n += ring_buf_get(&rx_ring, buf + n, len - n);
		if (n >= len || sys_timepoint_expired(end)) {
			break;
		}
		(void)k_sem_take(&rx_sem, sys_timepoint_timeout(end));
	}
	return (int)n;
}

int lidar_init(void)
{
	int ret;

	if (!device_is_ready(uart)) {
		LOG_ERR("UART not ready");
		return -ENODEV;
	}
	ret = lidar_set_baud(lidar_backend.baud);
	if (ret) {
		return ret;
	}
	struct uart_config cfg = { .baudrate = lidar_backend.baud };

	ret = uart_irq_callback_user_data_set(uart, uart_isr, NULL);
	if (ret) {
		LOG_ERR("irq callback set failed (%d)", ret);
		return ret;
	}
	uart_irq_rx_enable(uart);
	LOG_INF("%s on %s @ %u baud", lidar_backend.name, uart->name, cfg.baudrate);
	return 0;
}

void lidar_flush(void)
{
	/* Re-arm RX in case the driver dropped it across a suspend/resume. */
	uart_irq_rx_enable(uart);
	ring_buf_reset(&rx_ring);
	lidar_backend.parser_reset();
	checksum_errors = 0;
	k_sem_reset(&rx_sem);
}

int lidar_start(void)
{
	if (lidar_backend.start == NULL) {
		return 0;
	}
	return lidar_backend.start();
}

int lidar_stop(void)
{
	if (lidar_backend.stop == NULL) {
		return 0;
	}
	return lidar_backend.stop();
}

int lidar_read_frame(struct lidar_frame *frame, k_timeout_t timeout)
{
	k_timepoint_t end = sys_timepoint_calc(timeout);

	for (;;) {
		uint8_t b;

		while (ring_buf_get(&rx_ring, &b, 1) == 1) {
			switch (lidar_backend.parse_byte(b, frame)) {
			case LIDAR_PARSE_FRAME:
				return 0;
			case LIDAR_PARSE_CKSUM_ERR:
				checksum_errors++;
				break;
			default:
				break;
			}
		}
		if (sys_timepoint_expired(end)) {
			return -EAGAIN;
		}
		(void)k_sem_take(&rx_sem, sys_timepoint_timeout(end));
	}
}

static int cmp_u16(const void *a, const void *b)
{
	uint16_t x = *(const uint16_t *)a, y = *(const uint16_t *)b;

	return (x > y) - (x < y);
}

static uint16_t median_u16(uint16_t *v, uint16_t n)
{
	if (n == 0) {
		return 0;
	}
	qsort(v, n, sizeof(v[0]), cmp_u16);
	if (n & 1) {
		return v[n / 2];
	}
	return (uint16_t)(((uint32_t)v[n / 2 - 1] + v[n / 2]) / 2);
}

int lidar_capture(uint16_t n_samples, k_timeout_t timeout, struct lidar_stats *stats)
{
	if (stats == NULL || n_samples == 0) {
		return -EINVAL;
	}
	if (n_samples > MAX_SAMPLES) {
		n_samples = MAX_SAMPLES;
	}

	memset(stats, 0, sizeof(*stats));
	lidar_flush();

	k_timepoint_t end = sys_timepoint_calc(timeout);
	int64_t t0 = k_uptime_get();
	int32_t temp_sum = 0;
	double dist_sum = 0.0, dist_sq_sum = 0.0;

	stats->dist_min_cm = UINT16_MAX;
	stats->temp_c_x10 = LIDAR_TEMP_NONE;

	while (stats->n_frames < n_samples) {
		struct lidar_frame f;

		if (lidar_read_frame(&f, sys_timepoint_timeout(end)) != 0) {
			break;
		}

		str_samples[stats->n_frames] = f.strength;
		temp_sum += f.temp_c_x10;
		stats->n_frames++;

		if (lidar_backend.has_strength) {
			if (f.strength == LIDAR_STRENGTH_SATURATED) {
				stats->n_saturated++;
				continue;
			}
			if (f.strength < MIN_STRENGTH) {
				stats->n_weak++;
				continue;
			}
		}
		if (!f.valid) {
			stats->n_invalid++;
			continue;
		}

		dist_samples[stats->n_valid++] = f.dist_cm;
		dist_sum += f.dist_cm;
		dist_sq_sum += (double)f.dist_cm * f.dist_cm;
		if (f.dist_cm < stats->dist_min_cm) {
			stats->dist_min_cm = f.dist_cm;
		}
		if (f.dist_cm > stats->dist_max_cm) {
			stats->dist_max_cm = f.dist_cm;
		}
	}

	stats->elapsed_ms = (uint32_t)(k_uptime_get() - t0);
	stats->n_checksum_err = checksum_errors;

	if (stats->n_frames > 0) {
		if (lidar_backend.has_temp) {
			stats->temp_c_x10 = (int16_t)(temp_sum / stats->n_frames);
		}
		if (lidar_backend.has_strength) {
			stats->strength_median = median_u16(str_samples, stats->n_frames);
		}
	}
	if (stats->n_valid > 0) {
		uint16_t n = stats->n_valid;

		stats->dist_mean_cm = (float)(dist_sum / n);
		stats->dist_var_cm2 = (n > 1) ?
			(float)((dist_sq_sum - dist_sum * dist_sum / n) / (n - 1)) : 0.0f;
		stats->dist_median_cm = median_u16(dist_samples, n);
	} else {
		stats->dist_min_cm = 0;
	}

	return stats->n_frames;
}

int lidar_set_frame_rate(uint16_t hz)
{
	if (lidar_backend.set_frame_rate == NULL) {
		return -ENOTSUP;
	}
	return lidar_backend.set_frame_rate(hz);
}

int lidar_save_settings(void)
{
	if (lidar_backend.save_settings == NULL) {
		return -ENOTSUP;
	}
	return lidar_backend.save_settings();
}

const char *lidar_name(void)
{
	return lidar_backend.name;
}

bool lidar_has_strength(void)
{
	return lidar_backend.has_strength;
}

bool lidar_has_temp(void)
{
	return lidar_backend.has_temp;
}

void lidar_uart_write(const uint8_t *buf, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		uart_poll_out(uart, buf[i]);
	}
}
