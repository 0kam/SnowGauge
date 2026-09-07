/*
 * LiDAR abstraction: one interrupt-driven UART receiver + burst statistics
 * shared by the two sensor variants (spec section 3.2 / 3.3):
 *
 *   CONFIG_SNOWGAUGE_SENSOR_TFMINI  Benewake TFmini Plus (tfmini.c)
 *                                   5 V rail, 115200 8N1, 9-byte frames, cm,
 *                                   strength + chip temperature, 100 Hz
 *   CONFIG_SNOWGAUGE_SENSOR_TSD20   PONO TSD20 (tsd20.c)
 *                                   3.3 V rail, 460800 8N1, 4-byte frames, mm,
 *                                   distance only, 200 Hz, sentinel 50000
 *
 * The record format stores centimetres; a backend that reports millimetres
 * rounds to the nearest cm and keeps dist_mm for the live shell dump.
 */
#ifndef SNOWGAUGE_LIDAR_H
#define SNOWGAUGE_LIDAR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <zephyr/kernel.h>

#define LIDAR_STRENGTH_SATURATED 0xFFFFU
#define LIDAR_TEMP_NONE          INT16_MIN

struct lidar_frame {
	uint16_t dist_cm;     /* rounded distance, cm */
	uint16_t dist_mm;     /* native resolution (TFmini: cm * 10) */
	uint16_t strength;    /* 0 when the sensor has no strength output */
	int16_t temp_c_x10;   /* LIDAR_TEMP_NONE when the sensor has no temperature */
	bool valid;           /* false = sensor sentinel (no target / out of range) */
};

struct lidar_stats {
	/* frame accounting */
	uint16_t n_frames;     /* frames with a good checksum */
	uint16_t n_valid;      /* frames used for the distance statistics */
	uint16_t n_weak;       /* strength below CONFIG_SNOWGAUGE_TFMINI_MIN_STRENGTH */
	uint16_t n_saturated;  /* strength == 65535 */
	uint16_t n_invalid;    /* distance sentinel with acceptable strength */
	uint16_t n_checksum_err;
	uint32_t elapsed_ms;

	/* distance over valid frames */
	uint16_t dist_median_cm;
	uint16_t dist_min_cm;
	uint16_t dist_max_cm;
	float dist_mean_cm;
	float dist_var_cm2;

	/* auxiliary over frames with a good checksum */
	uint16_t strength_median;  /* 0 without strength output */
	int16_t temp_c_x10;        /* LIDAR_TEMP_NONE without temperature output */
};

/* Attach the interrupt-driven receiver to the sensor UART and set its baud rate. */
int lidar_init(void);

/* Discard any buffered bytes and reset the frame parser. */
void lidar_flush(void);

/*
 * Sensor-specific "start streaming" hook, called by sensor_rail_on() once the
 * rail has settled and the UART is awake. No-op for sensors that stream as
 * soon as they are powered (TFmini).
 */
int lidar_start(void);

/* Sensor-specific "stop streaming" hook (TSD20), sent before a deliberate power cycle. */
int lidar_stop(void);

/*
 * Block until n_samples good frames have been received or timeout expires,
 * then fill *stats. The sensor rail must be on. Returns the number of good
 * frames (0 if nothing was received), or a negative errno.
 */
int lidar_capture(uint16_t n_samples, k_timeout_t timeout, struct lidar_stats *stats);

/* Wait for the next good frame (raw access, e.g. the live shell dump). 0, or -EAGAIN. */
int lidar_read_frame(struct lidar_frame *frame, k_timeout_t timeout);

/* Frame-rate command; -ENOTSUP when the sensor cannot do the requested rate. */
int lidar_set_frame_rate(uint16_t hz);

/* Persist the sensor's settings in the sensor; -ENOTSUP when it has no such command. */
int lidar_save_settings(void);

const char *lidar_name(void);   /* "TFmini Plus" / "TSD20" */
bool lidar_has_strength(void);
bool lidar_has_temp(void);

/* ---- backend interface (implemented by tfmini.c / tsd20.c) ---- */

enum lidar_parse {
	LIDAR_PARSE_NONE,        /* byte consumed, no frame yet */
	LIDAR_PARSE_FRAME,       /* *frame filled from a checksum-good frame */
	LIDAR_PARSE_CKSUM_ERR,   /* a frame was dropped by its checksum */
};

struct lidar_backend {
	const char *name;
	uint32_t baud;
	bool has_strength;
	bool has_temp;
	void (*parser_reset)(void);
	enum lidar_parse (*parse_byte)(uint8_t b, struct lidar_frame *frame);
	int (*start)(void);                    /* optional */
	int (*stop)(void);                     /* optional */
	int (*set_frame_rate)(uint16_t hz);    /* optional */
	int (*save_settings)(void);            /* optional */
};

extern const struct lidar_backend lidar_backend;

/* Debug helpers (shell): raw byte access and run-time baud rate change. */
int lidar_read_bytes(uint8_t *buf, size_t len, k_timeout_t timeout);
int lidar_set_baud(uint32_t baud);

/* Blocking transmit for command frames (backends only). */
void lidar_uart_write(const uint8_t *buf, size_t len);

#endif /* SNOWGAUGE_LIDAR_H */
