/*
 * Calibration GATT service (spec section 12.2, layer 2).
 *
 * Service  53470001-6E63-4D61-8B0C-7A8E9F0B1C2D
 *  live    53470002-...  notify, 12 bytes LE every ~250 ms while enabled:
 *                        dist_cm u16 (0xFFFF none), strength u16, tilt_cdeg i16,
 *                        vbat_mv u16, n_valid u8, n_frames u8, var_cm2 u16
 *  control 53470003-...  write: 0x00 live off, 0x01 live on,
 *                        0x10 ZERO (no snow: d0 = measured distance),
 *                        0x11 + u16 depth_cm: reference from a probed snow depth
 *                             (d0 = d + depth / cos(tilt)),
 *                        0x20 'E' 'R' 'A' 'S' 'E' erase all records
 *  status  53470004-...  read, 12 bytes LE: d0_cm u16, theta0_cdeg i16,
 *                        set_epoch u32, live u8, last_cmd_result i8,
 *                        busy u8 (a command is running), cmd_seq u8
 *                        (incremented per accepted command; poll until busy
 *                        == 0 and cmd_seq matches, then read last_cmd_result)
 *
 * Writes while busy are rejected (ATT "procedure already in progress").
 * ERASE is refused (-EACCES) unless the record files were downloaded since
 * boot (fs mgmt read of rec_*.bin) or there are no records.
 *
 * Live mode keeps the sensor rail on and burns ~120 mA: the phone page
 * turns it off, and it also times out after CAL_LIVE_TIMEOUT_S.
 */
#ifndef SNOWGAUGE_CAL_GATT_H
#define SNOWGAUGE_CAL_GATT_H

#include <stdint.h>
#include <stdbool.h>

#define CAL_LIVE_TIMEOUT_S 300

int cal_gatt_init(void);
bool cal_live_is_on(void);
void cal_live_stop(void);

/* fs mgmt hook: a record file was read since boot (unlocks ERASE). */
void cal_note_records_downloaded(void);

/*
 * Take the reference from one full measurement (also stores a record).
 * depth_cm = current snow depth measured with a probe (0 = bare ground / ZERO).
 */
int cal_zero(uint16_t depth_cm, uint16_t *d0_cm, int16_t *theta0_cdeg);

#endif /* SNOWGAUGE_CAL_GATT_H */
