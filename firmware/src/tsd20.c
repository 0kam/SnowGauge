/*
 * PONO TSD20 backend (UART, 460800 8N1) - see lidar.h and
 * docs/tsd20_protocol.md.
 *
 * Data frame (4 bytes, default 200 Hz):
 *   0x5C DistL DistH CK       distance in mm, CK = ~(DistL + DistH)
 *   50000 = out of range / no target (sentinel)
 * No strength or temperature output.
 *
 * Commands (0x5A header, CK = ~(sum of bytes 2 .. n-1)):
 *   start ranging  5A 0A 02 02 00 F1     stop  5A 0A 02 00 00 F3
 *   frame rate     5A 0B 02 divL divH CK  f = 10000 / (div + 1)
 * The manual does not say whether the ranging state survives a power
 * cycle, so "start ranging" is sent every time the rail comes up.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <string.h>

#include "lidar.h"

LOG_MODULE_REGISTER(tsd20, CONFIG_LOG_DEFAULT_LEVEL);

#define TSD20_HDR       0x5C
#define TSD20_FRAME_LEN 4
#define TSD20_CMD_HDR   0x5A
#define TSD20_SENTINEL_MM 50000U

static uint8_t frame_buf[TSD20_FRAME_LEN];
static uint8_t frame_idx;

static void parser_reset(void)
{
	frame_idx = 0;
}

static bool frame_ok(const uint8_t *b)
{
	return b[3] == (uint8_t)~(b[1] + b[2]);
}

/*
 * A 1-byte header makes false sync likely (any DistL == 0x5C). On a
 * checksum failure the bytes after the bad header are re-fed, so a real
 * frame that started inside the bad one is not lost.
 */
static enum lidar_parse parse_byte(uint8_t b, struct lidar_frame *f)
{
	if (frame_idx == 0) {
		if (b == TSD20_HDR) {
			frame_buf[frame_idx++] = b;
		}
		return LIDAR_PARSE_NONE;
	}

	frame_buf[frame_idx++] = b;
	if (frame_idx < TSD20_FRAME_LEN) {
		return LIDAR_PARSE_NONE;
	}
	frame_idx = 0;

	if (!frame_ok(frame_buf)) {
		uint8_t rest[TSD20_FRAME_LEN - 1];

		memcpy(rest, &frame_buf[1], sizeof(rest));
		for (size_t i = 0; i < sizeof(rest); i++) {
			/* cannot complete a frame: at most 3 bytes are re-fed */
			(void)parse_byte(rest[i], f);
		}
		return LIDAR_PARSE_CKSUM_ERR;
	}

	uint16_t mm = frame_buf[1] | (frame_buf[2] << 8);

	f->dist_mm = mm;
	f->dist_cm = (uint16_t)((mm + 5U) / 10U);
	f->strength = 0;
	f->temp_c_x10 = LIDAR_TEMP_NONE;
	f->valid = (mm != TSD20_SENTINEL_MM && mm != 0);
	return LIDAR_PARSE_FRAME;
}

static int send_cmd(const uint8_t *cmd, size_t len)
{
	/* cmd[len-1] is the checksum slot: ~(sum of bytes 1 .. len-2). */
	uint8_t buf[16];
	uint8_t sum = 0;

	if (len > sizeof(buf) || len < 3) {
		return -EINVAL;
	}
	memcpy(buf, cmd, len);
	for (size_t i = 1; i < len - 1; i++) {
		sum += buf[i];
	}
	buf[len - 1] = (uint8_t)~sum;
	lidar_uart_write(buf, len);
	return 0;
}

static int start(void)
{
	const uint8_t cmd[] = { TSD20_CMD_HDR, 0x0A, 0x02, 0x02, 0x00, 0x00 };

	LOG_DBG("start ranging");
	return send_cmd(cmd, sizeof(cmd));
}

static int stop(void)
{
	const uint8_t cmd[] = { TSD20_CMD_HDR, 0x0A, 0x02, 0x00, 0x00, 0x00 };

	LOG_DBG("stop ranging");
	return send_cmd(cmd, sizeof(cmd));
}

static int set_frame_rate(uint16_t hz)
{
	if (hz == 0 || hz > 10000) {
		return -EINVAL;
	}

	uint16_t div = (uint16_t)(10000U / hz - 1U);
	const uint8_t cmd[] = { TSD20_CMD_HDR, 0x0B, 0x02,
				(uint8_t)(div & 0xFF), (uint8_t)(div >> 8), 0x00 };

	return send_cmd(cmd, sizeof(cmd));
}

const struct lidar_backend lidar_backend = {
	.name = "TSD20",
	.baud = 460800,
	.has_strength = false,
	.has_temp = false,
	.parser_reset = parser_reset,
	.parse_byte = parse_byte,
	.start = start,
	.stop = stop,
	.set_frame_rate = set_frame_rate,
	.save_settings = NULL,
};
