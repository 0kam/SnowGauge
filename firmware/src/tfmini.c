/*
 * Benewake TFmini Plus backend (UART, 115200 8N1) - see lidar.h.
 *
 * Data frame (9 bytes, default 100 Hz):
 *   0x59 0x59 DistL DistH StrL StrH TempL TempH Checksum
 *   Dist   : cm (default unit)
 *   Str    : signal strength 0..65535; < 100 unreliable, 65535 saturated
 *   Temp   : chip temperature, degC = raw / 8 - 256
 *   Checksum: low byte of the sum of bytes 0..7
 *
 * Invalid distances are reported by the sensor as 0 or 65535 depending on
 * the firmware revision; both are treated as a sentinel here.
 */

#include <zephyr/kernel.h>
#include <errno.h>
#include <string.h>

#include "lidar.h"

#define TFMINI_HDR       0x59
#define TFMINI_FRAME_LEN 9
#define TFMINI_CMD_HDR   0x5A

/* frame parser state (only touched from the calling thread) */
static uint8_t frame_buf[TFMINI_FRAME_LEN];
static uint8_t frame_idx;

static void parser_reset(void)
{
	frame_idx = 0;
}

static enum lidar_parse parse_byte(uint8_t b, struct lidar_frame *f)
{
	if (frame_idx < 2) {
		if (b == TFMINI_HDR) {
			frame_buf[frame_idx++] = b;
		} else {
			frame_idx = 0;
		}
		return LIDAR_PARSE_NONE;
	}

	frame_buf[frame_idx++] = b;
	if (frame_idx < TFMINI_FRAME_LEN) {
		return LIDAR_PARSE_NONE;
	}
	frame_idx = 0;

	uint8_t sum = 0;

	for (int i = 0; i < TFMINI_FRAME_LEN - 1; i++) {
		sum += frame_buf[i];
	}
	if (sum != frame_buf[TFMINI_FRAME_LEN - 1]) {
		return LIDAR_PARSE_CKSUM_ERR;
	}

	uint16_t dist_cm = frame_buf[2] | (frame_buf[3] << 8);
	int32_t temp_raw = frame_buf[6] | (frame_buf[7] << 8);

	f->dist_cm = dist_cm;
	f->dist_mm = (dist_cm == UINT16_MAX) ? UINT16_MAX : (uint16_t)MIN(dist_cm * 10U, 65535U);
	f->strength = frame_buf[4] | (frame_buf[5] << 8);
	f->temp_c_x10 = (int16_t)((temp_raw * 10) / 8 - 2560);
	f->valid = (dist_cm != 0 && dist_cm != UINT16_MAX);
	return LIDAR_PARSE_FRAME;
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
	lidar_uart_write(buf, len);
	return 0;
}

static int set_frame_rate(uint16_t hz)
{
	const uint8_t cmd[] = { TFMINI_CMD_HDR, 0x06, 0x03,
				(uint8_t)(hz & 0xFF), (uint8_t)(hz >> 8), 0x00 };

	return send_cmd(cmd, sizeof(cmd));
}

static int save_settings(void)
{
	const uint8_t cmd[] = { TFMINI_CMD_HDR, 0x04, 0x11, 0x00 };

	return send_cmd(cmd, sizeof(cmd));
}

const struct lidar_backend lidar_backend = {
	.name = "TFmini Plus",
	.baud = 115200,
	.has_strength = true,
	.has_temp = true,
	.parser_reset = parser_reset,
	.parse_byte = parse_byte,
	.start = NULL,
	.stop = NULL,
	.set_frame_rate = set_frame_rate,
	.save_settings = save_settings,
};
