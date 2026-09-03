/*
 * Persistent configuration (Zephyr settings on the internal storage
 * partition, NVS backend). Readable/writable over SMP settings mgmt from
 * the Web Bluetooth page and the shell ('cfg').
 *
 * Keys (all little-endian integers, sizes as in struct app_config):
 *   sg/sched/start_min    local minutes-of-day the window opens   (u16, 0..1439)
 *   sg/sched/end_min      local minutes-of-day the window closes  (u16; == start = all day,
 *                                                                  < start = spans midnight)
 *   sg/sched/interval_min minutes between measurements            (u16; 0 = scheduler off)
 *   sg/sched/tz_min       UTC offset of the site in minutes       (i16, e.g. 540 = JST)
 *   sg/cal/d0_cm          ZERO reference slant distance           (u16; 0 = not set)
 *   sg/cal/theta0_cdeg    ZERO reference tilt (0.01 deg)          (i16)
 *   sg/cal/set_epoch      when ZERO was taken (UTC)               (u32)
 */
#ifndef SNOWGAUGE_CONFIG_H
#define SNOWGAUGE_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

struct app_config {
	uint16_t sched_start_min;
	uint16_t sched_end_min;
	uint16_t sched_interval_min;
	int16_t tz_min;
	uint16_t cal_d0_cm;
	int16_t cal_theta0_cdeg;
	uint32_t cal_set_epoch;
};

/* Init the settings subsystem and load the stored values. */
int config_init(void);

/* Current values (copy). */
void config_get(struct app_config *out);

/* Replace and persist. Wakes the scheduler. */
int config_set(const struct app_config *cfg);

/* Store ZERO calibration and persist. */
int config_set_cal(uint16_t d0_cm, int16_t theta0_cdeg, uint32_t epoch);

/*
 * Next scheduled measurement at or after now_epoch (UTC), or 0 when the
 * scheduler is off. start == end means "all day".
 */
uint32_t config_next_measurement(uint32_t now_epoch);

/* Register a callback run (from the caller's context) after any change. */
void config_set_change_cb(void (*cb)(void));

#endif /* SNOWGAUGE_CONFIG_H */
