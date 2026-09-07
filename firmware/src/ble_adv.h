/*
 * BLE advertising with a status payload (spec section 12.2, layer 3).
 *
 * Connectable undirected advertising at a slow interval; the device name
 * "SG-TFM-XXXX" (TFmini Plus) / "SG-TSD-XXXX" (TSD20), XXXX = last two bytes of the
 * device ID, goes in the scan response.
 * Manufacturer Data (company ID 0xFFFF = unassigned/test) carries:
 *
 *  off size field
 *    0   1  payload version (2)
 *    1   2  vbat_end_mv of the last record (0 = none)
 *    3   4  record count
 *    7   2  last distance median (cm; 0xFFFF = none / invalid)
 *    9   1  status flags (BLE_ADV_FLAG_*)
 *   10   4  epoch of the last record (0 = none)
 *   14   2  uptime (hours, saturating)
 *   16   1  consecutive boot count (1 = clean start; see diag.h)     [v2]
 *   17   1  last reset cause (enum diag_reset: 0 power/brown-out, 1 pin,
 *           2 software, 3 watchdog, 4 lockup, 5 fatal error)          [v2]
 *
 * Read with any scanner (nRF Connect) during a site visit without
 * connecting.
 */
#ifndef SNOWGAUGE_BLE_ADV_H
#define SNOWGAUGE_BLE_ADV_H

#include <stdint.h>
#include <stdbool.h>

#define BLE_ADV_COMPANY_ID     0xFFFF
#define BLE_ADV_PAYLOAD_VERSION 2

#define BLE_ADV_FLAG_TIME_SYNCED    BIT(0)
#define BLE_ADV_FLAG_TIME_ESTIMATED BIT(1)
#define BLE_ADV_FLAG_LAST_LIDAR_OK  BIT(2)
#define BLE_ADV_FLAG_FS_OK          BIT(3)
#define BLE_ADV_FLAG_MIRROR_FULL    BIT(4)
#define BLE_ADV_FLAG_LAST_MEAS_ERR  BIT(5)
#define BLE_ADV_FLAG_ABNORMAL_RESET BIT(6) /* this boot follows a watchdog / lockup / fatal reset */
#define BLE_ADV_FLAG_MEAS_HALTED    BIT(7) /* reset loop: scheduled measurements suspended */

/* Enable Bluetooth, set the name and start advertising. */
int ble_adv_init(void);

/* Rebuild the payload from storage/time state (call after each record). */
int ble_adv_update(bool last_measurement_failed);

/* Restart advertising with a new interval window (ms). 0 = stop. */
int ble_adv_set_interval(uint32_t min_ms, uint32_t max_ms);

/* True while a central is connected. */
bool ble_adv_is_connected(void);

/*
 * True when the radio side is doing what it should: advertising, or a
 * central connected, or advertising deliberately off. Watchdog condition.
 */
bool ble_adv_is_healthy(void);

/* Status for the shell. */
void ble_adv_status(void (*out)(void *ctx, const char *fmt, ...), void *ctx);

#endif /* SNOWGAUGE_BLE_ADV_H */
