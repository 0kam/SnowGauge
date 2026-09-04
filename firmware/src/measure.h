/* One full measurement cycle (spec section 4.3, distance part). */
#ifndef SNOWGAUGE_MEASURE_H
#define SNOWGAUGE_MEASURE_H

#include <stdint.h>
#include "tfmini.h"
#include "tilt.h"

struct measurement {
	int64_t uptime_ms;
	uint16_t vbat_mv_start;  /* right after the rail settled (sensor booting) */
	uint16_t vbat_mv_end;    /* under sensor load, just before the rail is cut */
	struct tfmini_stats lidar;
	int lidar_ret;           /* return value of tfmini_capture() */
	struct tilt_reading tilt;
	int tilt_ret;            /* return value of tilt_read() */
};

/*
 * Tilt (IMU) -> rail on -> battery -> N TFmini frames -> battery -> rail off.
 * Takes sensor_lock. Skips the TFmini capture (lidar_ret = -ENOTSUP) when
 * the battery is below CONFIG_SNOWGAUGE_VBAT_MIN_MV right after the rail
 * came up (brown-out loop protection).
 */
int measure_once(struct measurement *m);

/*
 * Owner lock for the sensor rail, TFmini UART, IMU and ADC. Held by
 * measure_once(); the calibration live mode takes it per sample so a
 * scheduled measurement and the live view never interleave.
 */
extern struct k_mutex sensor_lock;

/* Pretty-print with the shell's printf-like callback (or printk). */
void measure_print(const struct measurement *m,
		   void (*out)(void *ctx, const char *fmt, ...), void *ctx);

#endif /* SNOWGAUGE_MEASURE_H */
