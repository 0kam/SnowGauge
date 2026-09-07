/* Watchdog supervisor - see wdt_mon.h */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>

#include "wdt_mon.h"
#include "ble_adv.h"

LOG_MODULE_REGISTER(wdt_mon, CONFIG_LOG_DEFAULT_LEVEL);

#define SUPERVISOR_PERIOD_MS (CONFIG_SNOWGAUGE_WDT_TIMEOUT_S * 1000 / 4)

static const struct device *const wdt = DEVICE_DT_GET(DT_ALIAS(watchdog0));
static int wdt_channel = -1;
static bool started;

struct channel {
	bool armed;
	int64_t deadline_ms;
	bool reported;
};

static struct channel ch[WDT_CH_COUNT];
static const char *const ch_name[WDT_CH_COUNT] = { "main", "measure", "ble" };
static struct k_spinlock lock;
static uint32_t feeds, starved_ticks;

static bool all_alive(int64_t now)
{
	bool ok = true;
	k_spinlock_key_t key = k_spin_lock(&lock);

	for (int i = 0; i < WDT_CH_COUNT; i++) {
		if (i == WDT_CH_BLE) {
			continue;
		}
		if (ch[i].armed && now > ch[i].deadline_ms) {
			if (!ch[i].reported) {
				LOG_ERR("channel %s stalled (%lld s overdue) - watchdog no longer fed",
					ch_name[i], (now - ch[i].deadline_ms) / 1000);
				ch[i].reported = true;
			}
			ok = false;
		}
	}
	bool ble_stalled = ch[WDT_CH_BLE].armed && now > ch[WDT_CH_BLE].deadline_ms;

	k_spin_unlock(&lock, key);

	if (!ble_stalled && !ble_adv_is_healthy()) {
		ble_stalled = true;
	}
	if (ble_stalled) {
		if (!ch[WDT_CH_BLE].reported) {
			LOG_ERR("BLE advertising is not running - watchdog no longer fed");
			ch[WDT_CH_BLE].reported = true;
		}
		ok = false;
	}
	return ok;
}

static void supervise(struct k_work *work)
{
	ARG_UNUSED(work);
	int64_t now = k_uptime_get();

	if (all_alive(now)) {
		(void)wdt_feed(wdt, wdt_channel);
		feeds++;
	} else {
		starved_ticks++;
	}
	k_work_reschedule(k_work_delayable_from_work(work), K_MSEC(SUPERVISOR_PERIOD_MS));
}
static K_WORK_DELAYABLE_DEFINE(supervisor_work, supervise);

int wdt_mon_init(void)
{
	int ret;
	struct wdt_timeout_cfg cfg = {
		.window = { .min = 0, .max = CONFIG_SNOWGAUGE_WDT_TIMEOUT_S * 1000U },
		.callback = NULL,
		.flags = WDT_FLAG_RESET_SOC,
	};

	if (!device_is_ready(wdt)) {
		LOG_ERR("watchdog device not ready");
		return -ENODEV;
	}
	ret = wdt_install_timeout(wdt, &cfg);
	if (ret < 0) {
		LOG_ERR("wdt_install_timeout: %d", ret);
		return ret;
	}
	wdt_channel = ret;
	/* Keep running in sleep (the device sleeps 99.9 % of the time); pause when a debugger halts. */
	ret = wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
	if (ret) {
		LOG_ERR("wdt_setup: %d", ret);
		return ret;
	}
	started = true;
	wdt_mon_alive(WDT_CH_MAIN, WDT_MAIN_TIMEOUT_S);
	k_work_schedule(&supervisor_work, K_MSEC(SUPERVISOR_PERIOD_MS));
	LOG_INF("watchdog on: %u s timeout, supervisor every %u s", CONFIG_SNOWGAUGE_WDT_TIMEOUT_S,
		SUPERVISOR_PERIOD_MS / 1000);
	return 0;
}

void wdt_mon_alive(enum wdt_ch c, uint32_t timeout_s)
{
	if (c >= WDT_CH_COUNT) {
		return;
	}
	k_spinlock_key_t key = k_spin_lock(&lock);

	ch[c].armed = true;
	ch[c].deadline_ms = k_uptime_get() + (int64_t)timeout_s * 1000;
	ch[c].reported = false;
	k_spin_unlock(&lock, key);
}

void wdt_mon_disarm(enum wdt_ch c)
{
	if (c >= WDT_CH_COUNT) {
		return;
	}
	k_spinlock_key_t key = k_spin_lock(&lock);

	ch[c].armed = false;
	ch[c].reported = false;
	k_spin_unlock(&lock, key);
}

void wdt_mon_stall(enum wdt_ch c)
{
	if (c >= WDT_CH_COUNT) {
		return;
	}
	k_spinlock_key_t key = k_spin_lock(&lock);

	ch[c].armed = true;
	ch[c].deadline_ms = k_uptime_get() - 1;
	k_spin_unlock(&lock, key);
	LOG_WRN("channel %s marked stalled (test) - reset in <= %u s", ch_name[c],
		CONFIG_SNOWGAUGE_WDT_TIMEOUT_S + SUPERVISOR_PERIOD_MS / 1000);
}

void wdt_mon_status(void (*out)(void *ctx, const char *fmt, ...), void *ctx)
{
	int64_t now = k_uptime_get();

	out(ctx, "hw watchdog %s: timeout %u s, supervisor %u s, feeds=%u starved=%u",
	    started ? "on" : "OFF", CONFIG_SNOWGAUGE_WDT_TIMEOUT_S, SUPERVISOR_PERIOD_MS / 1000,
	    feeds, starved_ticks);
	for (int i = 0; i < WDT_CH_COUNT; i++) {
		if (i == WDT_CH_BLE) {
			out(ctx, "  %-8s %s", ch_name[i],
			    ble_adv_is_healthy() ? "healthy (adv or connected)" : "NOT healthy");
		} else if (ch[i].armed) {
			out(ctx, "  %-8s armed, %lld s left%s", ch_name[i],
			    (ch[i].deadline_ms - now) / 1000, ch[i].reported ? " (STALLED)" : "");
		} else {
			out(ctx, "  %-8s idle", ch_name[i]);
		}
	}
}
