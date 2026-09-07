/* Wall-clock time - see timekeeping.h */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "timekeeping.h"

LOG_MODULE_REGISTER(timekeeping, CONFIG_LOG_DEFAULT_LEVEL);

static const struct device *const rtc = DEVICE_DT_GET(DT_ALIAS(rtc));
static enum time_state state = TIME_UNSET;

/*
 * The clock itself is base_epoch + elapsed k_uptime: k_uptime reads the
 * RTC1 counter (32.768 kHz crystal), whereas rtc-emul re-arms a 1 s work
 * item from inside its handler and gains ~2.6 s/day. rtc-emul is still
 * set on every time_set() so that mcumgr's datetime command (step 4c)
 * has a device to talk to.
 */
static uint32_t base_epoch;
static int64_t base_uptime_ms;

/* ---- checkpoint in the settings (sgt/epoch) ---- */

#define SAVE_KEY "sgt/epoch"
static uint32_t saved_epoch;

static int sgt_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	if (strcmp(name, "epoch") == 0 && len == sizeof(saved_epoch)) {
		return read_cb(cb_arg, &saved_epoch, len) == (ssize_t)len ? 0 : -EIO;
	}
	return -ENOENT;
}
SETTINGS_STATIC_HANDLER_DEFINE(sgt, "sgt", NULL, sgt_set, NULL, NULL);

static void time_save(struct k_work *work)
{
	uint32_t now;

	if (time_now(&now) == 0) {
		int ret = settings_save_one(SAVE_KEY, &now, sizeof(now));

		if (ret) {
			LOG_WRN("clock checkpoint: %d", ret);
		} else {
			LOG_DBG("clock checkpoint %u", now);
		}
	}
	if (CONFIG_SNOWGAUGE_TIME_SAVE_MIN > 0) {
		k_work_reschedule(k_work_delayable_from_work(work),
				  K_MINUTES(CONFIG_SNOWGAUGE_TIME_SAVE_MIN));
	}
}
static K_WORK_DELAYABLE_DEFINE(time_save_work, time_save);

int time_restore_saved(void)
{
	uint32_t now = 0;
	int ret = settings_load_subtree("sgt");

	if (ret) {
		LOG_WRN("load sgt: %d", ret);
		return ret;
	}
	if (saved_epoch == 0) {
		return -ENODATA;
	}
	if (time_now(&now) == 0 && now >= saved_epoch) {
		return 0; /* the newest record is later than the checkpoint */
	}
	ret = time_set(saved_epoch, false);
	if (ret == 0) {
		LOG_INF("clock restored from the checkpoint (%u)", saved_epoch);
	}
	return ret;
}

int time_init(void)
{
	if (!device_is_ready(rtc)) {
		LOG_ERR("RTC device not ready");
		return -ENODEV;
	}
	return 0;
}

int time_now(uint32_t *epoch)
{
	if (state == TIME_UNSET) {
		return -ENODATA;
	}
	*epoch = base_epoch + (uint32_t)((k_uptime_get() - base_uptime_ms) / 1000);
	return 0;
}

int time_set(uint32_t epoch, bool synced)
{
	struct tm tm;
	struct rtc_time t = { 0 };
	time_t tt = epoch;
	int ret;

	/* Never let a restore estimate overwrite an externally synced clock. */
	if (!synced && state == TIME_SYNCED) {
		return -EALREADY;
	}
	if (epoch < TIME_EPOCH_MIN || epoch > TIME_EPOCH_MAX) {
		LOG_WRN("rejecting implausible epoch %u", epoch);
		return -EINVAL;
	}

	gmtime_r(&tt, &tm);
	t.tm_sec = tm.tm_sec;
	t.tm_min = tm.tm_min;
	t.tm_hour = tm.tm_hour;
	t.tm_mday = tm.tm_mday;
	t.tm_mon = tm.tm_mon;
	t.tm_year = tm.tm_year;
	t.tm_wday = tm.tm_wday;
	t.tm_yday = tm.tm_yday;
	t.tm_isdst = -1;
	t.tm_nsec = 0;

	base_uptime_ms = k_uptime_get();
	base_epoch = epoch;
	state = synced ? TIME_SYNCED : TIME_ESTIMATED;

	if (CONFIG_SNOWGAUGE_TIME_SAVE_MIN > 0) {
		/* Checkpoint right away after an external sync, else on the period. */
		k_work_reschedule(&time_save_work,
				  synced ? K_SECONDS(2) : K_MINUTES(CONFIG_SNOWGAUGE_TIME_SAVE_MIN));
	}

	ret = rtc_set_time(rtc, &t);
	if (ret) {
		LOG_WRN("rtc_set_time: %d", ret);
	}
	return 0;
}

static int rtc_write(uint32_t epoch)
{
	struct tm tm;
	struct rtc_time t = { 0 };
	time_t tt = epoch;

	gmtime_r(&tt, &tm);
	t.tm_sec = tm.tm_sec;
	t.tm_min = tm.tm_min;
	t.tm_hour = tm.tm_hour;
	t.tm_mday = tm.tm_mday;
	t.tm_mon = tm.tm_mon;
	t.tm_year = tm.tm_year;
	t.tm_wday = tm.tm_wday;
	t.tm_yday = tm.tm_yday;
	t.tm_isdst = -1;
	t.tm_nsec = 0;
	return rtc_set_time(rtc, &t);
}

int time_refresh_rtc(void)
{
	uint32_t now;
	int ret = time_now(&now);

	return ret ? ret : rtc_write(now);
}

enum time_state time_get_state(void)
{
	return state;
}

void time_format(uint32_t epoch, char *buf, size_t len)
{
	struct tm tm;
	time_t tt = epoch;

	if (epoch == 0) {
		snprintf(buf, len, "unset");
		return;
	}
	gmtime_r(&tt, &tm);
	snprintf(buf, len, "%04d-%02d-%02dT%02d:%02d:%02dZ",
		 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		 tm.tm_hour, tm.tm_min, tm.tm_sec);
}

uint32_t time_yyyymm(uint32_t epoch)
{
	struct tm tm;
	time_t tt = epoch;

	gmtime_r(&tt, &tm);
	return (uint32_t)(tm.tm_year + 1900) * 100U + (uint32_t)(tm.tm_mon + 1);
}
