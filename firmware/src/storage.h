/*
 * Record storage (spec section 4.3 step 4 / 12.1).
 *
 *  primary : LittleFS on the 2 MB QSPI flash (P25Q16H), one file per month
 *            "/lfs1/rec_YYYYMM.bin" (or "/lfs1/rec_notime.bin" while the
 *            clock is unset), records appended back to back. These files
 *            are what mcumgr fs download fetches over BLE.
 *  mirror  : raw append-only slots in the internal flash partition
 *            `mirror_partition` (~7,800 records, > 2 years at 8/day).
 *            Survives a corrupt / dead QSPI, seeds the clock and the
 *            sequence counter after a reset, and backs the shell dump.
 *
 * The QSPI driver uses runtime PM (deep power-down between accesses), so
 * nothing here has to manage the flash power state.
 */
#ifndef SNOWGAUGE_STORAGE_H
#define SNOWGAUGE_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

#include "record.h"

/* Mount the file system, scan the mirror, restore clock + sequence. */
int storage_init(void);

/* Append to LittleFS and the mirror. Returns 0 if at least the primary succeeded. */
int storage_append(const struct record *r);

/* Records stored since the last erase (as counted at boot + appended since). */
uint32_t storage_record_count(void);

/* Sequence number the next record will get. */
uint32_t storage_next_seq(void);

/* Mirror access: idx 0 = oldest. Returns -ENOENT past the end, -EBADMSG for a bad slot. */
int storage_mirror_read(uint32_t idx, struct record *r);
uint32_t storage_mirror_count(void);
uint32_t storage_mirror_capacity(void);

/* Newest stored record (from the mirror). -ENOENT if none. */
int storage_last_record(struct record *r);

/* Whether LittleFS is mounted (primary storage usable). */
bool storage_fs_ok(void);

/* Print the record files ("name size"). */
int storage_list_files(void (*out)(void *ctx, const char *fmt, ...), void *ctx);

/* Delete all record files and erase the mirror. Sequence restarts at 0. */
int storage_erase_all(void);

#endif /* SNOWGAUGE_STORAGE_H */
