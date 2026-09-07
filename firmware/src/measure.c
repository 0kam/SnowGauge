/* Measurement cycle - see measure.h */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdlib.h>

#include "measure.h"
#include "sensor_rail.h"
#include "battery.h"
#include "lidar.h"
#include "tilt.h"

LOG_MODULE_REGISTER(measure, CONFIG_LOG_DEFAULT_LEVEL);

K_MUTEX_DEFINE(sensor_lock);

int measure_once(struct measurement *m)
{
	int ret;

	memset(m, 0, sizeof(*m));
	m->uptime_ms = k_uptime_get();
	k_mutex_lock(&sensor_lock, K_FOREVER);

	/* IMU first: independent of the rail, keeps the current peaks apart. */
	m->tilt_ret = tilt_read(&m->tilt, CONFIG_SNOWGAUGE_TILT_SAMPLES);
	if (m->tilt_ret) {
		LOG_WRN("tilt read failed (%d)", m->tilt_ret);
	}

	ret = sensor_rail_on();
	if (ret) {
		LOG_ERR("rail on failed (%d)", ret);
		k_mutex_unlock(&sensor_lock);
		return ret;
	}

	(void)battery_read_mv(&m->vbat_mv_start);

	if (m->vbat_mv_start != 0 && m->vbat_mv_start < CONFIG_SNOWGAUGE_VBAT_MIN_MV) {
		/*
		 * Battery too low for the sensor rail: do not run the
		 * capture (brown-out -> reset -> measure -> brown-out loop).
		 * The record still carries Vbat so the decline is visible.
		 */
		LOG_WRN("vbat %u mV below %u mV - LiDAR capture skipped", m->vbat_mv_start,
			CONFIG_SNOWGAUGE_VBAT_MIN_MV);
		m->lidar_ret = -ENOTSUP;
	} else {
		m->lidar_ret = lidar_capture(CONFIG_SNOWGAUGE_LIDAR_SAMPLES,
					     K_MSEC(CONFIG_SNOWGAUGE_LIDAR_CAPTURE_TIMEOUT_MS),
					     &m->lidar);
		if (m->lidar_ret <= 0 && IS_ENABLED(CONFIG_SNOWGAUGE_LIDAR_RETRY)) {
			/*
			 * Nothing at all from the sensor: power-cycle it once. The
			 * TSD20 gets an explicit stop first in case its ranging state
			 * outlives the short rail-off; sensor_rail_on() sends start.
			 */
			LOG_WRN("no frames (%d) - power-cycling the sensor and retrying once",
				m->lidar_ret);
			(void)lidar_stop();
			(void)sensor_rail_off();
			k_sleep(K_MSEC(CONFIG_SNOWGAUGE_LIDAR_RETRY_OFF_MS));
			ret = sensor_rail_on();
			if (ret == 0) {
				m->retried = true;
				m->lidar_ret = lidar_capture(CONFIG_SNOWGAUGE_LIDAR_SAMPLES,
							     K_MSEC(CONFIG_SNOWGAUGE_LIDAR_CAPTURE_TIMEOUT_MS),
							     &m->lidar);
			} else {
				LOG_ERR("rail on for the retry failed (%d)", ret);
				k_mutex_unlock(&sensor_lock);
				return ret;
			}
		}
	}

	(void)battery_read_mv(&m->vbat_mv_end);

	ret = sensor_rail_off();
	if (ret) {
		LOG_ERR("rail off failed (%d)", ret);
	}

	k_mutex_unlock(&sensor_lock);
	if (m->lidar_ret <= 0) {
		LOG_WRN("no LiDAR frames received (%d)", m->lidar_ret);
	}
	return ret;
}

void measure_print(const struct measurement *m,
		   void (*out)(void *ctx, const char *fmt, ...), void *ctx)
{
	const struct lidar_stats *s = &m->lidar;

	out(ctx, "t=%lld ms  vbat=%u/%u mV (start/end)",
	    m->uptime_ms, m->vbat_mv_start, m->vbat_mv_end);
	out(ctx, "frames=%u in %u ms  valid=%u weak=%u sat=%u invalid=%u cksum_err=%u%s",
	    s->n_frames, s->elapsed_ms, s->n_valid, s->n_weak, s->n_saturated,
	    s->n_invalid, s->n_checksum_err, m->retried ? "  (retried after a power cycle)" : "");
	if (s->n_valid > 0) {
		out(ctx, "dist: median=%u cm  mean=%.1f  var=%.2f cm^2  min=%u max=%u",
		    s->dist_median_cm, (double)s->dist_mean_cm, (double)s->dist_var_cm2,
		    s->dist_min_cm, s->dist_max_cm);
	} else {
		out(ctx, "dist: no valid samples");
	}
	if (s->n_frames > 0 && lidar_has_strength()) {
		out(ctx, "strength median=%u", s->strength_median);
	}
	if (s->n_frames > 0 && lidar_has_temp()) {
		out(ctx, "chip temp=%d.%d C", s->temp_c_x10 / 10, abs(s->temp_c_x10 % 10));
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
