/* Benewake TFmini Plus driver - see tfmini.h */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "tfmini.h"

LOG_MODULE_REGISTER(tfmini, CONFIG_LOG_DEFAULT_LEVEL);

#define TFMINI_HDR       0x59
#define TFMINI_FRAME_LEN 9
#define TFMINI_CMD_HDR   0x5A

#define RX_RING_SIZE 256
#define MAX_SAMPLES  CONFIG_SNOWGAUGE_TFMINI_SAMPLES

static const struct device *const uart = DEVICE_DT_GET(DT_ALIAS(tfmini_uart));

RING_BUF_DECLARE(rx_ring, RX_RING_SIZE);
static K_SEM_DEFINE(rx_sem, 0, 1);

/* frame parser state (only touched from the calling thread) */
static uint8_t frame_buf[TFMINI_FRAME_LEN];
static uint8_t frame_idx;
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

int tfmini_init(void)
{
	int ret;

	if (!device_is_ready(uart)) {
		LOG_ERR("UART not ready");
		return -ENODEV;
	}
	ret = uart_irq_callback_user_data_set(uart, uart_isr, NULL);
	if (ret) {
		LOG_ERR("irq callback set failed (%d)", ret);
		return ret;
	}
	uart_irq_rx_enable(uart);
	return 0;
}

void tfmini_flush(void)
{
	/* Re-arm RX in case the driver dropped it across a suspend/resume. */
	uart_irq_rx_enable(uart);
	ring_buf_reset(&rx_ring);
	frame_idx = 0;
	checksum_errors = 0;
	k_sem_reset(&rx_sem);
}

/* Feed one byte; returns true when a checksum-valid frame is complete. */
static bool parse_byte(uint8_t b, struct tfmini_frame *f)
{
	if (frame_idx < 2) {
		if (b == TFMINI_HDR) {
			frame_buf[frame_idx++] = b;
		} else {
			frame_idx = 0;
		}
		return false;
	}

	frame_buf[frame_idx++] = b;
	if (frame_idx < TFMINI_FRAME_LEN) {
		return false;
	}
	frame_idx = 0;

	uint8_t sum = 0;

	for (int i = 0; i < TFMINI_FRAME_LEN - 1; i++) {
		sum += frame_buf[i];
	}
	if (sum != frame_buf[TFMINI_FRAME_LEN - 1]) {
		checksum_errors++;
		return false;
	}

	f->dist_cm = frame_buf[2] | (frame_buf[3] << 8);
	f->strength = frame_buf[4] | (frame_buf[5] << 8);
	int32_t temp_raw = frame_buf[6] | (frame_buf[7] << 8);

	f->temp_c_x10 = (int16_t)((temp_raw * 10) / 8 - 2560);
	return true;
}

int tfmini_read_frame(struct tfmini_frame *frame, k_timeout_t timeout)
{
	k_timepoint_t end = sys_timepoint_calc(timeout);

	for (;;) {
		uint8_t b;

		while (ring_buf_get(&rx_ring, &b, 1) == 1) {
			if (parse_byte(b, frame)) {
				return 0;
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

int tfmini_capture(uint16_t n_samples, k_timeout_t timeout,
		   struct tfmini_stats *stats)
{
	if (stats == NULL || n_samples == 0) {
		return -EINVAL;
	}
	if (n_samples > MAX_SAMPLES) {
		n_samples = MAX_SAMPLES;
	}

	memset(stats, 0, sizeof(*stats));
	tfmini_flush();

	k_timepoint_t end = sys_timepoint_calc(timeout);
	int64_t t0 = k_uptime_get();
	int32_t temp_sum = 0;
	double dist_sum = 0.0, dist_sq_sum = 0.0;

	stats->dist_min_cm = UINT16_MAX;

	while (stats->n_frames < n_samples) {
		struct tfmini_frame f;

		if (tfmini_read_frame(&f, sys_timepoint_timeout(end)) != 0) {
			break;
		}

		str_samples[stats->n_frames] = f.strength;
		temp_sum += f.temp_c_x10;
		stats->n_frames++;

		if (f.strength == TFMINI_STRENGTH_SATURATED) {
			stats->n_saturated++;
			continue;
		}
		if (f.strength < CONFIG_SNOWGAUGE_TFMINI_MIN_STRENGTH) {
			stats->n_weak++;
			continue;
		}
		if (f.dist_cm == 0 || f.dist_cm == UINT16_MAX) {
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
		stats->temp_c_x10 = (int16_t)(temp_sum / stats->n_frames);
		stats->strength_median = median_u16(str_samples, stats->n_frames);
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

static int send_cmd(const uint8_t *cmd, size_t len)
{
	/* cmd[len-1] is the checksum slot: low byte of the sum of the others. */
	uint8_t buf[16];
	uint8_t sum = 0;

	if (len > sizeof(buf)) {
		return -EINVAL;
	}
	memcpy(buf, cmd, len);
	for (size_t i = 0; i < len - 1; i++) {
		sum += buf[i];
	}
	buf[len - 1] = sum;

	for (size_t i = 0; i < len; i++) {
		uart_poll_out(uart, buf[i]);
	}
	return 0;
}

int tfmini_set_frame_rate(uint16_t hz)
{
	const uint8_t cmd[] = { TFMINI_CMD_HDR, 0x06, 0x03,
				(uint8_t)(hz & 0xFF), (uint8_t)(hz >> 8), 0x00 };

	return send_cmd(cmd, sizeof(cmd));
}

int tfmini_save_settings(void)
{
	const uint8_t cmd[] = { TFMINI_CMD_HDR, 0x04, 0x11, 0x00 };

	return send_cmd(cmd, sizeof(cmd));
}
