/*
 * Wall-clock time for record timestamps.
 *
 * The nRF52840 has no calendar RTC, so the Zephyr emulated RTC (rtc-emul,
 * driven by the kernel tick on the 32.768 kHz crystal) is used behind the
 * `rtc` devicetree alias. That alias is also what mcumgr's os_mgmt
 * datetime command will talk to over BLE (step 4c).
 *
 * The clock is lost on reset. storage_init() re-seeds it from the newest
 * stored record, so time keeps moving forward but is marked ESTIMATED
 * until it is set from outside again.
 */
#ifndef SNOWGAUGE_TIMEKEEPING_H
#define SNOWGAUGE_TIMEKEEPING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum time_state {
	TIME_UNSET = 0,   /* never set since boot and nothing to restore from */
	TIME_ESTIMATED,   /* restored from the last record (offset unknown) */
	TIME_SYNCED,      /* set from the shell / BLE since boot */
};

int time_init(void);

/* Current UTC epoch seconds. Returns -ENODATA while TIME_UNSET. */
int time_now(uint32_t *epoch);

/* Set the clock. synced = true for an external source, false for a restore. */
int time_set(uint32_t epoch, bool synced);

enum time_state time_get_state(void);

/* "YYYY-MM-DDThh:mm:ssZ" (21 bytes incl. NUL) or "unset". */
void time_format(uint32_t epoch, char *buf, size_t len);

/* Year*100 + month (e.g. 202612) for the monthly record file name. */
uint32_t time_yyyymm(uint32_t epoch);

#endif /* SNOWGAUGE_TIMEKEEPING_H */
