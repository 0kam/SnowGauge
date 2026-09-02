/*
 * Benewake TFmini Plus driver (UART, 115200 8N1).
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
#ifndef SNOWGAUGE_TFMINI_H
#define SNOWGAUGE_TFMINI_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>

#define TFMINI_STRENGTH_SATURATED 0xFFFFU

struct tfmini_frame {
	uint16_t dist_cm;
	uint16_t strength;
	int16_t temp_c_x10;
};

struct tfmini_stats {
	/* frame accounting */
	uint16_t n_frames;     /* frames with a good checksum */
	uint16_t n_valid;      /* frames used for the distance statistics */
	uint16_t n_weak;       /* strength below CONFIG_SNOWGAUGE_TFMINI_MIN_STRENGTH */
	uint16_t n_saturated;  /* strength == 65535 */
	uint16_t n_invalid;    /* distance sentinel (0 / 65535) with acceptable strength */
	uint16_t n_checksum_err;
	uint32_t elapsed_ms;

	/* distance over valid frames */
	uint16_t dist_median_cm;
	uint16_t dist_min_cm;
	uint16_t dist_max_cm;
	float dist_mean_cm;
	float dist_var_cm2;

	/* auxiliary over frames with a good checksum */
	uint16_t strength_median;
	int16_t temp_c_x10;
};

/* Attach the interrupt-driven receiver to the TFmini UART. Call once at boot. */
int tfmini_init(void);

/* Discard any buffered bytes and reset the frame parser. */
void tfmini_flush(void);

/*
 * Block until n_samples good frames have been received or timeout expires,
 * then fill *stats. The sensor rail must be on. Returns the number of good
 * frames (0 if nothing was received), or a negative errno.
 */
int tfmini_capture(uint16_t n_samples, k_timeout_t timeout,
		   struct tfmini_stats *stats);

/*
 * Wait for the next good frame (raw access, e.g. for the live shell dump).
 * Returns 0, -EAGAIN on timeout.
 */
int tfmini_read_frame(struct tfmini_frame *frame, k_timeout_t timeout);

/* Send a configuration command frame (0x5A header). */
int tfmini_set_frame_rate(uint16_t hz);
int tfmini_save_settings(void);

#endif /* SNOWGAUGE_TFMINI_H */
