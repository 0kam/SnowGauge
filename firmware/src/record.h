/*
 * SnowGauge data record (spec section 4.4), version 1.
 *
 * Fixed 40-byte little-endian layout so that a plain file dump can be
 * decoded offline (tools/decode_records.py) and re-synchronised on the
 * magic word if a partial write ever slips in.
 *
 *  off size field
 *    0   2  magic 0x5347 ("SG" little-endian)
 *    2   1  version (1)
 *    3   1  flags (RECORD_FLAG_*)
 *    4   4  epoch (UTC seconds; 0 = unknown)
 *    8   4  seq (monotonic record counter, reset by ERASE)
 *   12   2  dist_median_cm
 *   14   2  dist_var_cm2 (clamped to 65535)
 *   16   2  strength (TFmini signal strength median)
 *   18   2  n_frames (checksum-good frames in the burst)
 *   20   2  n_valid (frames used for the distance statistics)
 *   22   2  n_out_of_range (sentinel + saturated + weak-signal frames)
 *   24   2  tilt (0.01 deg, angle between gravity and board normal)
 *   26   2  pitch (0.01 deg, signed)
 *   28   2  roll (0.01 deg, signed)
 *   30   2  imu_temp (0.1 degC, signed; environment proxy)
 *   32   2  lidar_temp (0.1 degC, signed; TFmini chip temperature)
 *   34   2  vbat_start_mv (right after the rail settled)
 *   36   2  vbat_end_mv (under sensor load, before the rail is cut)
 *   38   2  crc16 (Zephyr crc16_ccitt: reflected CCITT poly 0x8408, init 0xFFFF, over bytes 0..37)
 */
#ifndef SNOWGAUGE_RECORD_H
#define SNOWGAUGE_RECORD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "measure.h"

#define RECORD_SIZE      40
#define RECORD_MAGIC     0x5347U
#define RECORD_VERSION   1

/* flags */
#define RECORD_FLAG_TIME_SYNCED   BIT(0) /* clock set from outside (BLE/shell) since boot */
#define RECORD_FLAG_TIME_ESTIMATED BIT(1) /* clock restored from the last record at boot */
#define RECORD_FLAG_LIDAR_OK      BIT(2) /* at least one valid distance sample */
#define RECORD_FLAG_TILT_OK       BIT(3) /* IMU read succeeded */
#define RECORD_FLAG_MANUAL        BIT(4) /* triggered from the shell / BLE, not the schedule */
#define RECORD_FLAG_FIRST_AFTER_BOOT BIT(5)

struct record {
	uint8_t flags;
	uint32_t epoch;
	uint32_t seq;
	uint16_t dist_median_cm;
	uint16_t dist_var_cm2;
	uint16_t strength;
	uint16_t n_frames;
	uint16_t n_valid;
	uint16_t n_out_of_range;
	int16_t tilt_cdeg;
	int16_t pitch_cdeg;
	int16_t roll_cdeg;
	int16_t imu_temp_dc;
	int16_t lidar_temp_dc;
	uint16_t vbat_start_mv;
	uint16_t vbat_end_mv;
};

/* Fill *r from a finished measurement. epoch/seq/time flags are set by the caller. */
void record_from_measurement(struct record *r, const struct measurement *m);

/* Serialise to RECORD_SIZE bytes (magic, version and CRC added). */
void record_encode(const struct record *r, uint8_t buf[RECORD_SIZE]);

/* Parse RECORD_SIZE bytes; returns false if magic/version/CRC do not match. */
bool record_decode(const uint8_t buf[RECORD_SIZE], struct record *r);

/* One-line human readable form (CSV-ish) for the shell. */
void record_print(const struct record *r,
		  void (*out)(void *ctx, const char *fmt, ...), void *ctx);

/* Header line matching record_print(). */
void record_print_header(void (*out)(void *ctx, const char *fmt, ...), void *ctx);

#endif /* SNOWGAUGE_RECORD_H */
