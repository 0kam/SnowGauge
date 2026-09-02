/* One full measurement cycle (spec section 4.3, distance part). */
#ifndef SNOWGAUGE_MEASURE_H
#define SNOWGAUGE_MEASURE_H

#include <stdint.h>
#include "tfmini.h"

struct measurement {
	int64_t uptime_ms;
	uint16_t vbat_mv_start;  /* right after the rail settled (sensor booting) */
	uint16_t vbat_mv_end;    /* under sensor load, just before the rail is cut */
	struct tfmini_stats lidar;
	int lidar_ret;           /* return value of tfmini_capture() */
};

/* Rail on -> battery -> N TFmini frames -> battery -> rail off. */
int measure_once(struct measurement *m);

/* Pretty-print with the shell's printf-like callback (or printk). */
void measure_print(const struct measurement *m,
		   void (*out)(void *ctx, const char *fmt, ...), void *ctx);

#endif /* SNOWGAUGE_MEASURE_H */
