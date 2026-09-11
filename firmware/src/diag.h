/*
 * Boot diagnostics: why did we (re)start, how often, and was it a crash.
 *
 * - Reset cause from the SoC (hwinfo / RESETREAS), cleared after reading so
 *   the next boot sees only its own cause. RESETREAS == 0 means power-on or
 *   brown-out; the nRF52 cannot tell them apart.
 * - Fatal errors (CPU exception, stack overflow, k_panic ...) reboot the
 *   device instead of spinning (k_sys_fatal_error_handler below), leaving a
 *   reason code in GPREGRET2 for the next boot to report.
 * - Consecutive-reset counter in the settings (NVS key "sgd/boots"):
 *   GPREGRET2 turned out to be cleared by a watchdog reset (measured
 *   2026-09-07; only a soft reset retains it), so the counter lives in
 *   flash and survives every kind of reset including a power cycle. It is
 *   cleared after a measurement with valid LiDAR data is stored, or on BLE
 *   connection. Drives the scheduler hold-off and, at
 *   CONFIG_SNOWGAUGE_BOOT_MAX_RESETS, a
 *   "measurements halted" mode that keeps advertising so the site visit
 *   can still read the device. BLE connection drops the halt immediately;
 *   a successful manual measurement clears the counter but needs a reboot
 *   to drop the halt. Rebooting alone does not clear the counter.
 * - One text line per boot appended to /lfs1/boot.log (diag_log_boot()).
 *
 * GPREGRET2 = DIAG_FATAL_* reason of the last fatal error (0 = none); it is
 * written by the fatal handler right before the soft reset.
 */
#ifndef SNOWGAUGE_DIAG_H
#define SNOWGAUGE_DIAG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum diag_fatal {
	DIAG_FATAL_NONE = 0,
	DIAG_FATAL_CPU_EXCEPTION,
	DIAG_FATAL_SPURIOUS_IRQ,
	DIAG_FATAL_STACK_OVERFLOW,
	DIAG_FATAL_OOPS,
	DIAG_FATAL_PANIC,
	DIAG_FATAL_UNKNOWN,
};

/* Compact reset cause for the BLE advertisement / boot.log. */
enum diag_reset {
	DIAG_RESET_POWER = 0,   /* power-on or brown-out (RESETREAS == 0) */
	DIAG_RESET_PIN,
	DIAG_RESET_SOFTWARE,    /* sys_reboot / dfu / mcumgr reset */
	DIAG_RESET_WATCHDOG,
	DIAG_RESET_LOCKUP,
	DIAG_RESET_FATAL,       /* software reset issued by the fatal error handler */
	DIAG_RESET_OTHER,
};

struct diag_boot {
	uint32_t hw_cause;        /* raw hwinfo RESET_* flags */
	enum diag_reset reset;
	enum diag_fatal fatal;    /* reason left by the previous run, if any */
	uint8_t boot_count;       /* consecutive resets incl. this boot (1 = clean) */
	bool abnormal;            /* watchdog, lockup or fatal error */
	bool halted;              /* boot_count reached CONFIG_SNOWGAUGE_BOOT_MAX_RESETS */
};

/*
 * A phone connected over BLE: someone is standing at the logger, so the resets
 * that got us here were a human power-cycling it (a loose battery holder at
 * installation), not the brown-out loop the backoff protects against. Drops the
 * "halted" state and the boot_count multiplier, leaving the base hold-off, and
 * clears the stored counter. Returns true only if halt / hold-off changed
 * and the scheduler needs waking. Safe to call on every connection.
 */
bool diag_user_present(void);

/* Clear the stored counter after valid LiDAR data is stored; keep this boot's halt. */
void diag_measure_succeeded(void);

/* Read + clear the reset cause and the fatal reason. Call first thing in main(). */
void diag_init(void);

/*
 * Load + increment the consecutive-reset counter (settings must be
 * initialised, i.e. after config_init()). Computes hold-off / halted.
 */
int diag_count_boot(void);

const struct diag_boot *diag_boot_info(void);

/* Scheduled measurements are suspended for this boot (reset loop protection). */
bool diag_measure_halted(void);

/* Uptime (seconds) before which the scheduler must not measure. */
uint32_t diag_holdoff_until_s(void);

/* Append the boot line to /lfs1/boot.log (after storage_init + clock restore). */
int diag_log_boot(void);

const char *diag_reset_str(enum diag_reset r);
const char *diag_fatal_str(enum diag_fatal f);

/* Human readable summary for the shell. */
void diag_print(void (*out)(void *ctx, const char *fmt, ...), void *ctx);

#endif /* SNOWGAUGE_DIAG_H */
