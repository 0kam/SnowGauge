/* Measurement cycle - see measure.h */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdlib.h>

#include "measure.h"
#include "sensor_rail.h"
#include "battery.h"
#include "tfmini.h"
#include "tilt.h"

LOG_MODULE_REGISTER(measure, CONFIG_LOG_DEFAULT_LEVEL);

int measure_once(struct measurement *m)
{
	int ret;

	memset(m, 0, sizeof(*m));
	m->uptime_ms = k_uptime_get();

	/* IMU first: independent of the rail, keeps the current peaks apart. */
	m->tilt_ret = tilt_read(&m->tilt, CONFIG_SNOWGAUGE_TILT_SAMPLES);
	if (m->tilt_ret) {
		LOG_WRN("tilt read failed (%d)", m->tilt_ret);
	}

	ret = sensor_rail_on();
	if (ret) {
		LOG_ERR("rail on failed (%d)", ret);
		return ret;
	}

	(void)battery_read_mv(&m->vbat_mv_start);

	m->lidar_ret = tfmini_capture(CONFIG_SNOWGAUGE_TFMINI_SAMPLES,
				      K_MSEC(CONFIG_SNOWGAUGE_TFMINI_CAPTURE_TIMEOUT_MS),
				      &m->lidar);

	(void)battery_read_mv(&m->vbat_mv_end);

	ret = sensor_rail_off();
	if (ret) {
		LOG_ERR("rail off failed (%d)", ret);
	}

	if (m->lidar_ret <= 0) {
		LOG_WRN("no TFmini frames received");
	}
	return ret;
}

void measure_print(const struct measurement *m,
		   void (*out)(void *ctx, const char *fmt, ...), void *ctx)
{
	const struct tfmini_stats *s = &m->lidar;

	out(ctx, "t=%lld ms  vbat=%u/%u mV (start/end)",
	    m->uptime_ms, m->vbat_mv_start, m->vbat_mv_end);
	out(ctx, "frames=%u in %u ms  valid=%u weak=%u sat=%u invalid=%u cksum_err=%u",
	    s->n_frames, s->elapsed_ms, s->n_valid, s->n_weak, s->n_saturated,
	    s->n_invalid, s->n_checksum_err);
	if (s->n_valid > 0) {
		out(ctx, "dist: median=%u cm  mean=%.1f  var=%.2f cm^2  min=%u max=%u",
		    s->dist_median_cm, (double)s->dist_mean_cm, (double)s->dist_var_cm2,
		    s->dist_min_cm, s->dist_max_cm);
	} else {
		out(ctx, "dist: no valid samples");
	}
	if (s->n_frames > 0) {
		out(ctx, "strength median=%u  chip temp=%d.%d C",
		    s->strength_median, s->temp_c_x10 / 10, abs(s->temp_c_x10 % 10));
	}
	if (m->tilt_ret == 0) {
		out(ctx, "tilt=%.2f deg (pitch=%.2f roll=%.2f)  a=(%d,%d,%d) mg  imu temp=%.1f C  n=%u",
		    (double)m->tilt.tilt_deg, (double)m->tilt.pitch_deg, (double)m->tilt.roll_deg,
		    m->tilt.ax_mg, m->tilt.ay_mg, m->tilt.az_mg, (double)m->tilt.temp_c,
		    m->tilt.n_samples);
	} else if (m->tilt_ret != 0 && m->tilt.n_samples == 0 && m->uptime_ms != 0) {
		out(ctx, "tilt: read failed (%d)", m->tilt_ret);
	}
}
