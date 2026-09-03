/*
 * SnowGauge firmware - step 1: Zephyr scaffold + TFmini UART test.
 *
 * Boots with the sensor rail off and the TFmini UART parked (Hi-Z), opens a
 * shell on USB CDC ACM, and optionally runs a measurement cycle every
 * CONFIG_SNOWGAUGE_AUTO_MEASURE_PERIOD_S seconds.
 */

#include <zephyr/kernel.h>
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

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

static K_SEM_DEFINE(period_changed, 0, 1);
static uint32_t auto_period_s = CONFIG_SNOWGAUGE_AUTO_MEASURE_PERIOD_S;

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

	ret = measure_once(&m);
	if (m_out) {
		*m_out = m;
	}
	if (ret) {
		LOG_ERR("measurement failed (%d) - not stored", ret);
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

	LOG_INF("SnowGauge FW (step 4a: record storage) - board " CONFIG_BOARD_TARGET);

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
	ret = usb_pm_init();
	if (ret) {
		LOG_ERR("usb_pm_init: %d", ret);
	}

	LOG_INF("ready - type 'help' in the USB shell (rail is OFF)");

	bool first = true;

	for (;;) {
		uint32_t period = auto_period_s;

		if (period == 0) {
			k_sem_take(&period_changed, K_FOREVER);
			first = true;
			continue;
		}

		/* Measure right away when automatic mode starts, then every period. */
		if (!first && k_sem_take(&period_changed, K_SECONDS(period)) == 0) {
			first = true;
			continue; /* period changed - re-evaluate */
		}
		first = false;

		struct measurement m;
		struct record r;

		led_pulse(50);
		if (app_measure_and_store(false, &m, &r) == 0) {
			LOG_INF("record %u stored (%u total)", r.seq, storage_record_count());
		}
		measure_print(&m, printk_out, NULL);
	}
	return 0;
}
