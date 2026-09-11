/*
 * SnowGauge firmware.
 *
 * Boots with the sensor rail off and the sensor UART parked (Hi-Z), opens a
 * shell on USB CDC ACM, and optionally runs a measurement cycle every
 * CONFIG_SNOWGAUGE_AUTO_MEASURE_PERIOD_S seconds.
 */

#include <zephyr/kernel.h>
#include <nrfx.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

#include "app.h"
#include "sensor_rail.h"
#include "lidar.h"
#include "battery.h"
#include "measure.h"
#include "tilt.h"
#include "power.h"
#include "usb_pm.h"
#include "record.h"
#include "storage.h"
#include "timekeeping.h"
#include "ble_adv.h"
#include "config.h"
#include "cal_gatt.h"
#include "diag.h"
#include "wdt_mon.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

static K_SEM_DEFINE(period_changed, 0, 1);
static uint32_t auto_period_s = CONFIG_SNOWGAUGE_AUTO_MEASURE_PERIOD_S;

static void config_changed(void)
{
	k_sem_give(&period_changed);
}

void app_wake_scheduler(void)
{
	k_sem_give(&period_changed);
}

void app_set_auto_period(uint32_t seconds)
{
	auto_period_s = seconds;
	k_sem_give(&period_changed);
}

uint32_t app_get_auto_period(void)
{
	return auto_period_s;
}

static bool first_record_after_boot = true;

int app_measure_and_store(bool manual, struct measurement *m_out, struct record *r_out)
{
	struct measurement m;
	struct record r;
	uint32_t epoch = 0;
	int ret;

	wdt_mon_alive(WDT_CH_MEASURE, WDT_MEASURE_TIMEOUT_S);
	ret = measure_once(&m);
	if (m_out) {
		*m_out = m;
	}
	if (ret) {
		LOG_ERR("measurement failed (%d) - not stored", ret);
		(void)ble_adv_update(true);
		wdt_mon_disarm(WDT_CH_MEASURE);
		return ret;
	}

	record_from_measurement(&r, &m);
	if (time_now(&epoch) == 0) {
		r.epoch = epoch;
		r.flags |= (time_get_state() == TIME_SYNCED) ? RECORD_FLAG_TIME_SYNCED
							     : RECORD_FLAG_TIME_ESTIMATED;
	}
	if (manual) {
		r.flags |= RECORD_FLAG_MANUAL;
	}
	if (first_record_after_boot) {
		r.flags |= RECORD_FLAG_FIRST_AFTER_BOOT;
		first_record_after_boot = false;
	}
	r.seq = storage_next_seq();

	ret = storage_append(&r);
	if (ret) {
		LOG_ERR("record %u not stored (%d)", r.seq, ret);
	} else if (m.lidar_ret != -ENOTSUP) {
		/*
		 * Surviving the full-load rail burst and storing its record is the
		 * proof, regardless of distance validity; a low-battery skip is not.
		 */
		diag_measure_succeeded();
	}
	if (r_out) {
		*r_out = r;
	}
	(void)ble_adv_update(ret != 0);
	wdt_mon_disarm(WDT_CH_MEASURE);
	return ret;
}

static void printk_out(void *ctx, const char *fmt, ...)
{
	ARG_UNUSED(ctx);
	va_list ap;
	char line[160];

	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	LOG_INF("%s", line);
}

int main(void)
{
	int ret;

	LOG_INF("SnowGauge FW (step 5: %s variant) - board " CONFIG_BOARD_TARGET, lidar_name());
	diag_init();

	ret = sensor_rail_init();
	if (ret) {
		LOG_ERR("sensor_rail_init: %d", ret);
	}
	ret = lidar_init();
	if (ret) {
		LOG_ERR("lidar_init: %d", ret);
	}
	ret = battery_init();
	if (ret) {
		LOG_ERR("battery_init: %d", ret);
	}
	ret = tilt_init();
	if (ret) {
		LOG_ERR("tilt_init: %d", ret);
	}
	ret = power_init();
	if (ret) {
		LOG_ERR("power_init: %d", ret);
	}
	ret = time_init();
	if (ret) {
		LOG_ERR("time_init: %d", ret);
	}
	ret = storage_init();
	if (ret) {
		LOG_ERR("storage_init: %d", ret);
	}
	ret = config_init();
	if (ret) {
		LOG_ERR("config_init: %d", ret);
	}
	(void)diag_count_boot(); /* needs the settings subsystem (config_init) */
	(void)time_restore_saved();
	(void)diag_log_boot();
	config_set_change_cb(config_changed);
	ret = ble_adv_init();
	if (ret) {
		LOG_ERR("ble_adv_init: %d", ret);
	}
	ret = cal_gatt_init();
	if (ret) {
		LOG_ERR("cal_gatt_init: %d", ret);
	}
	ret = usb_pm_init();
	if (ret) {
		LOG_ERR("usb_pm_init: %d", ret);
	}
	ret = wdt_mon_init();
	if (ret) {
		LOG_ERR("wdt_mon_init: %d", ret);
	}

	LOG_INF("ready - type 'help' in the USB shell (rail is OFF)");

	bool first = true;

	for (;;) {
		uint32_t period = auto_period_s;

		wdt_mon_alive(WDT_CH_MAIN, WDT_MAIN_TIMEOUT_S);
		if (diag_measure_halted()) {
			/* Reset loop: stay reachable over BLE, do not measure. */
			k_sem_take(&period_changed, K_MINUTES(10));
			first = true;
			continue;
		}
		if (period != 0) {
			/* Bench mode: fixed period from the shell / Kconfig. */
			if (!first && k_sem_take(&period_changed, K_SECONDS(period)) == 0) {
				first = true;
				continue;
			}
			first = false;
		} else {
			/* Field mode: schedule window from the settings, needs the clock. */
			uint32_t now, next;
			struct app_config c;

			config_get(&c);
			if (c.sched_interval_min == 0) {
				k_sem_take(&period_changed, K_MINUTES(10));
				first = true;
				continue;
			}
			/* Hold-off after a reset (see diag_init). */
			int64_t left_ms = (int64_t)diag_holdoff_until_s() * 1000 - k_uptime_get();

			if (left_ms > 0) {
				if (k_sem_take(&period_changed, K_MSEC(MIN(left_ms, 3600000))) == 0) {
					first = true;
				}
				continue;
			}
			if (time_now(&now) != 0) {
				/*
				 * Clock never set (sync forgotten at installation): still
				 * measure every interval from boot so nothing is lost; the
				 * records land in rec_notime.bin, ordered by seq.
				 */
				if (!first && k_sem_take(&period_changed, K_MINUTES(c.sched_interval_min)) == 0) {
					first = true;
					continue;
				}
				first = false;
				LOG_WRN("clock unset - measuring on the interval from boot");
			} else if ((next = config_next_measurement(now)) == 0) {
				k_sem_take(&period_changed, K_MINUTES(10));
				first = true;
				continue;
			} else if (next > now) {
				uint32_t wait_s = MIN(next - now, 3600U);

				if (k_sem_take(&period_changed, K_SECONDS(wait_s)) == 0 ||
				    wait_s < next - now) {
					continue; /* config changed, or re-evaluate hourly */
				}
			}
		}

		struct measurement m;
		struct record r;

		if (cal_live_is_on()) {
			LOG_WRN("scheduled measurement skipped: live mode active");
			k_sleep(K_SECONDS(60));
			continue;
		}
		led_pulse(50);
		if (app_measure_and_store(false, &m, &r) == 0) {
			LOG_INF("record %u stored (%u total)", r.seq, storage_record_count());
		}
		measure_print(&m, printk_out, NULL);
	}
	return 0;
}
