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

#include "app.h"
#include "sensor_rail.h"
#include "tfmini.h"
#include "battery.h"
#include "measure.h"
#include "tilt.h"

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

	LOG_INF("SnowGauge FW (step 2: measurement sequence) - board " CONFIG_BOARD_TARGET);

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

	LOG_INF("ready - type 'help' in the USB shell (rail is OFF)");

	for (;;) {
		uint32_t period = auto_period_s;

		if (period == 0) {
			k_sem_take(&period_changed, K_FOREVER);
			continue;
		}

		if (k_sem_take(&period_changed, K_SECONDS(period)) == 0) {
			continue; /* period changed - re-evaluate */
		}

		struct measurement m;

		(void)measure_once(&m);
		measure_print(&m, printk_out, NULL);
	}
	return 0;
}
