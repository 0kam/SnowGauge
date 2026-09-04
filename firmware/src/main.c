/*
 * SnowGauge firmware - step 1: Zephyr scaffold + TFmini UART test.
 *
 * Boots with the sensor rail off and the TFmini UART parked (Hi-Z), opens a
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
#include "tfmini.h"
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

/*
 * Reset-loop protection. GPREGRET2 survives soft resets and brown-outs (not
 * a power cycle): count boots, clear the counter once the firmware has run
 * for one hold-off window, and stretch the first scheduled measurement by
 * the count. A brown-out during the 120 mA burst therefore cannot make the
 * device measure again immediately after every reset.
 */
static uint32_t boot_count;
static int64_t holdoff_until_ms;

static void boot_counter_clear(struct k_work *work)
{
	ARG_UNUSED(work);
	NRF_POWER->GPREGRET2 = 0;
}
static K_WORK_DELAYABLE_DEFINE(boot_counter_work, boot_counter_clear);

static void boot_holdoff_init(void)
{
	boot_count = NRF_POWER->GPREGRET2 + 1;
	NRF_POWER->GPREGRET2 = MIN(boot_count, 255);
	holdoff_until_ms = (int64_t)CONFIG_SNOWGAUGE_BOOT_HOLDOFF_MIN * 60 * 1000 * MIN(boot_count, 12);
	k_work_schedule(&boot_counter_work, K_MINUTES(CONFIG_SNOWGAUGE_BOOT_HOLDOFF_MIN));
	if (boot_count > 1) {
		LOG_WRN("boot #%u within the hold-off window - first measurement in %lld min",
			boot_count, holdoff_until_ms / 60000);
	}
}

int app_measure_and_store(bool manual, struct measurement *m_out, struct record *r_out)
{
	struct measurement m;
	struct record r;
	uint32_t epoch = 0;
	int ret;

	ret = measure_once(&m);
	if (m_out) {
		*m_out = m;
	}
	if (ret) {
		LOG_ERR("measurement failed (%d) - not stored", ret);
		(void)ble_adv_update(true);
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
	}
	if (r_out) {
		*r_out = r;
	}
	(void)ble_adv_update(ret != 0);
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

	LOG_INF("SnowGauge FW (step 4d: schedule + calibration) - board " CONFIG_BOARD_TARGET);
	boot_holdoff_init();

	ret = sensor_rail_init();
	if (ret) {
		LOG_ERR("sensor_rail_init: %d", ret);
	}
	ret = tfmini_init();
	if (ret) {
		LOG_ERR("tfmini_init: %d", ret);
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

	LOG_INF("ready - type 'help' in the USB shell (rail is OFF)");

	bool first = true;

	for (;;) {
		uint32_t period = auto_period_s;

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
			/* Hold-off after a reset (see boot_holdoff_init). */
			int64_t left_ms = holdoff_until_ms - k_uptime_get();

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
