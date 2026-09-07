/*
 * Watchdog supervisor (spec section 12.3).
 *
 * The nRF52 hardware watchdog (CONFIG_SNOWGAUGE_WDT_TIMEOUT_S, runs in
 * sleep, cannot be stopped once started) is fed by a periodic supervisor on
 * the system work queue - but only while every armed channel is alive:
 *
 *   WDT_CH_MAIN     scheduler loop; must call wdt_mon_alive() at least once
 *                   per WDT_MAIN_TIMEOUT_S (it sleeps at most one hour)
 *   WDT_CH_MEASURE  armed around a measurement cycle (rail on -> record stored)
 *   WDT_CH_BLE      checked, not fed: advertising must be running (or a
 *                   central connected) unless it was switched off on purpose
 *
 * A stuck subsystem, a stuck system work queue or a locked-up CPU all end
 * in a watchdog reset, which diag.c reports on the next boot.
 *
 * Bootloader note: the Adafruit bootloader (>= 0.6.1) feeds a running WDT
 * in its DFU wait loop, so `dfu` + serial DFU work with the WDT enabled.
 */
#ifndef SNOWGAUGE_WDT_MON_H
#define SNOWGAUGE_WDT_MON_H

#include <stdint.h>
#include <stdbool.h>

enum wdt_ch {
	WDT_CH_MAIN = 0,
	WDT_CH_MEASURE,
	WDT_CH_BLE,
	WDT_CH_COUNT,
};

#define WDT_MAIN_TIMEOUT_S    (2 * 3600)
#define WDT_MEASURE_TIMEOUT_S 60

/* Install + start the hardware watchdog and the supervisor. */
int wdt_mon_init(void);

/* Channel is alive; its deadline moves to now + timeout_s. */
void wdt_mon_alive(enum wdt_ch ch, uint32_t timeout_s);

/* Stop supervising a channel (e.g. measurement finished). */
void wdt_mon_disarm(enum wdt_ch ch);

/* Test hook: mark a channel expired so that the supervisor stops feeding. */
void wdt_mon_stall(enum wdt_ch ch);

void wdt_mon_status(void (*out)(void *ctx, const char *fmt, ...), void *ctx);

#endif /* SNOWGAUGE_WDT_MON_H */
