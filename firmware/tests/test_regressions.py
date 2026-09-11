#!/usr/bin/env python3
"""Run actual firmware functions with host stubs; no board or Zephyr SDK needed."""

from pathlib import Path
import re
import subprocess
import tempfile

SRC = Path(__file__).resolve().parents[1] / "src"


def function(file, name):
    # Firmware function closing braces are at column zero.
    return re.search(
        rf"^(?:static )?\w+ {name}\(.*?^\}}",
        (SRC / file).read_text(), re.M | re.S,
    )[0]


diag = (SRC / "diag.c").read_text()
config = (SRC / "config.c").read_text()
assert "boot_counter_work" not in diag, "Elapsed time must not clear the counter"
assert "static atomic_t holdoff_until_s;" in diag

preamble = r'''
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include "diag.h"
#include "config.h"
#include "record.h"
#define CONFIG_SNOWGAUGE_BOOT_HOLDOFF_MIN 10
#define CONFIG_SNOWGAUGE_BOOT_MAX_RESETS 12
#define CONFIG_SNOWGAUGE_SCHED_DEFAULT_INTERVAL_MIN 90
#define CONFIG_SNOWGAUGE_SCHED_DEFAULT_TZ_MIN 540
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define LOG_INF(...) ((void)0)
#define LOG_WRN(...) ((void)0)
#define LOG_ERR(...) ((void)0)
#define snprintk snprintf
#define K_MUTEX_DEFINE(n) int n
#define K_FOREVER 0
#define k_mutex_lock(p, timeout) ((void)(p))
#define k_mutex_unlock(p) ((void)(p))
#define ARG_UNUSED(x) ((void)(x))
#define atomic_t _Atomic(int32_t)
#define atomic_set(p, v) atomic_store(p, v)
#define atomic_get(p) atomic_load(p)
#define WDT_CH_MEASURE 0
#define WDT_MEASURE_TIMEOUT_S 0
#define wdt_mon_alive(...) ((void)0)
#define wdt_mon_disarm(...) ((void)0)
#define TIME_SYNCED 1
#define BOOTS_KEY "sgd/boots"
static struct diag_boot info;
static atomic_t holdoff_until_s;
static uint8_t stored_boots, flash_boots;
static int save_error, boot_writes, fail_key = -1, key_writes, wakes;
static int sg_export(int (*cb)(const char *, const void *, size_t));
static int settings_load_subtree(const char *name) {
    assert(strcmp(name, "sgd") == 0);
    stored_boots = flash_boots;
    return 0;
}
static int settings_save_one(const char *name, const void *value, size_t size) {
    if (strcmp(name, BOOTS_KEY) == 0) {
        assert(size == 1);
        boot_writes++;
        if (save_error) return save_error;
        flash_boots = *(const uint8_t *)value;
        return 0;
    }
    return key_writes++ == fail_key ? -ENOSPC : 0;
}
static int settings_save_subtree(const char *name) {
    assert(strcmp(name, "sg") == 0);
    return sg_export(settings_save_one);
}
static void app_wake_scheduler(void) { wakes++; }
struct k_work { int unused; };
static bool first_record_after_boot = true;
static struct measurement sample;
static int measure_error, store_error;
int measure_once(struct measurement *m) { *m = sample; return measure_error; }
void record_from_measurement(struct record *r, const struct measurement *m) {
    (void)m; memset(r, 0, sizeof(*r));
}
static int time_now(uint32_t *epoch) { *epoch = 0; return -ENODATA; }
static int time_get_state(void) { return TIME_SYNCED; }
static uint32_t storage_next_seq(void) { return 1; }
static int storage_append(const struct record *r) { (void)r; return store_error; }
static int ble_adv_update(bool failed) { (void)failed; return 0; }
'''

code = preamble + config[config.index("static struct app_config cfg"):config.index("static const struct key_map *find_key")]
for file, names in {
    "diag.c": ["save_boots", "diag_measure_succeeded", "diag_count_boot",
               "diag_user_present", "diag_measure_halted", "diag_holdoff_until_s"],
    "ble_adv.c": ["user_present_fn"],
    "main.c": ["app_measure_and_store"],
    "config.c": ["validate", "sg_export", "config_set"],
}.items():
    code += "\n" + "\n".join(function(file, name) for name in names)

code += r'''
int main(void) {
    /* Brownouts before a completed burst retain NVS across 12+ boots. */
    for (int boot = 1; boot <= 13; boot++) {
        assert(diag_count_boot() == 0);
        assert(flash_boots == boot);
        assert(diag_holdoff_until_s() == 600U * MIN(boot, 12));
        assert(diag_measure_halted() == (boot >= 12));
    }
    user_present_fn(NULL);
    assert(!diag_measure_halted() && diag_holdoff_until_s() == 600);
    assert(flash_boots == 0 && wakes == 1);
    user_present_fn(NULL);
    assert(wakes == 1);

    /* First boot BLE must clear NVS=1 without disturbing a pending slot. */
    assert(diag_count_boot() == 0 && flash_boots == 1);
    user_present_fn(NULL);
    assert(flash_boots == 0 && wakes == 1);
    assert(diag_count_boot() == 0);
    save_error = -EIO;
    user_present_fn(NULL);
    assert(flash_boots == 1 && wakes == 1);
    save_error = 0;
    user_present_fn(NULL);
    assert(flash_boots == 0 && wakes == 1);

    /* A stored low-battery skip does not prove the cells survived a burst. */
    flash_boots = 11;
    assert(diag_count_boot() == 0 && diag_measure_halted());
    sample.lidar.n_valid = 0;
    sample.lidar_ret = -ENOTSUP;
    assert(app_measure_and_store(true, NULL, NULL) == 0 && flash_boots == 12);

    /* An attempted burst with zero valid frames clears NVS once stored. */
    sample.lidar_ret = 0;
    assert(app_measure_and_store(true, NULL, NULL) == 0 && flash_boots == 0);
    assert(diag_count_boot() == 0 && flash_boots == 1);
    sample.lidar_ret = 5;
    assert(app_measure_and_store(true, NULL, NULL) == 0 && flash_boots == 0);
    assert(diag_count_boot() == 0 && flash_boots == 1);
    sample.lidar_ret = -EIO;
    assert(app_measure_and_store(true, NULL, NULL) == 0 && flash_boots == 0);

    /* Measurement-cycle, storage and counter-clear I/O errors retain NVS. */
    flash_boots = 11;
    assert(diag_count_boot() == 0 && diag_measure_halted());
    sample.lidar_ret = 5;
    sample.lidar.n_valid = 5;
    measure_error = -EIO;
    assert(app_measure_and_store(true, NULL, NULL) == -EIO && flash_boots == 12);
    measure_error = 0;
    store_error = -ENOSPC;
    assert(app_measure_and_store(true, NULL, NULL) == -ENOSPC && flash_boots == 12);
    store_error = 0;
    save_error = -EIO;
    assert(app_measure_and_store(true, NULL, NULL) == 0 && flash_boots == 12);
    save_error = 0;
    assert(app_measure_and_store(true, NULL, NULL) == 0 && flash_boots == 0);
    assert(diag_measure_halted()); /* Manual success still needs a reboot. */
    int writes = boot_writes;
    assert(app_measure_and_store(true, NULL, NULL) == 0 && boot_writes == writes);
    assert(diag_count_boot() == 0 && !diag_measure_halted() && flash_boots == 1);
    assert(app_measure_and_store(false, NULL, NULL) == 0 && flash_boots == 0);

    /* Every failed export key propagates its errno through config_set(). */
    struct app_config valid = cfg;
    change_cb = app_wake_scheduler;
    for (int key = 0; key < (int)ARRAY_SIZE(keys); key++) {
        key_writes = 0;
        fail_key = key;
        assert(config_set(&valid) == -ENOSPC && key_writes == key + 1);
    }
    fail_key = -1;
    key_writes = 0;
    assert(config_set(&valid) == 0 && key_writes == ARRAY_SIZE(keys));
    /* Rejected shell config keeps RAM, NVS and the scheduler untouched. */
    for (int field = 0; field < 5; field++) {
        struct app_config invalid = valid;
        if (field == 0) invalid.sched_start_min = 1440;
        if (field == 1) invalid.sched_end_min = 1440;
        if (field == 2) invalid.sched_interval_min = 1441;
        if (field == 3) invalid.tz_min = 841;
        if (field == 4) invalid.tz_min = -841;
        int wake_count = wakes;
        key_writes = 0;
        assert(config_set(&invalid) == -EINVAL);
        assert(memcmp(&cfg, &valid, sizeof(cfg)) == 0);
        assert(key_writes == 0 && wakes == wake_count);
    }
    puts("PASS: reset backoff, measurement success, BLE wakeups, settings errors");
}
'''

with tempfile.TemporaryDirectory(prefix="snowgauge-test-") as tmp:
    tmp = Path(tmp)
    (tmp / "zephyr").mkdir()
    (tmp / "zephyr/kernel.h").write_text(
        "#pragma once\ntypedef int k_timeout_t;\n#define BIT(n) (1U << (n))\n"
    )
    (tmp / "test.c").write_text(code)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I", str(tmp), "-I", str(SRC), str(tmp / "test.c"),
                    "-o", str(tmp / "test")], check=True)
    subprocess.run([str(tmp / "test")], check=True)
