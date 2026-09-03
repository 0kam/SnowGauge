# SnowGauge record format (v1) and storage layout

Source of truth: `firmware/src/record.h` (layout), `firmware/src/storage.h`
(where records live). Decoder: `tools/decode_records.py`.

## One record = 40 bytes, little-endian

| off | size | field | unit / meaning |
|---:|---:|---|---|
| 0 | 2 | magic | `0x5347` (`"SG"` as bytes) |
| 2 | 1 | version | 1 |
| 3 | 1 | flags | see below |
| 4 | 4 | epoch | UTC seconds; 0 = clock was unset |
| 8 | 4 | seq | record counter since the last ERASE |
| 12 | 2 | dist_median_cm | median of valid TFmini samples; 65535 = none valid |
| 14 | 2 | dist_var_cm2 | variance (clamped to 65535); 65535 = none valid |
| 16 | 2 | strength | TFmini signal-strength median |
| 18 | 2 | n_frames | checksum-good frames in the burst (N=100 nominal) |
| 20 | 2 | n_valid | frames used for the distance statistics |
| 22 | 2 | n_out_of_range | sentinel (0/65535 cm) + saturated + weak-signal frames |
| 24 | 2 | tilt | 0.01°, angle between gravity and the board normal; -32768 = IMU failed |
| 26 | 2 | pitch | 0.01°, signed |
| 28 | 2 | roll | 0.01°, signed |
| 30 | 2 | imu_temp | 0.1 °C, signed (environment proxy) |
| 32 | 2 | lidar_temp | 0.1 °C, signed; TFmini *chip* temperature (50–75 °C is normal) |
| 34 | 2 | vbat_start_mv | battery right after the sensor rail settled |
| 36 | 2 | vbat_end_mv | battery under sensor load, just before the rail is cut |
| 38 | 2 | crc16 | Zephyr `crc16_ccitt()`: reflected CCITT (poly 0x8408, i.e. 0x1021 bit-reversed), init 0xFFFF, no final XOR, over bytes 0–37 |

Flags (bit set = true):

| bit | name | meaning |
|---:|---|---|
| 0 | TIME_SYNCED | clock had been set from outside (shell / BLE) since boot |
| 1 | TIME_ESTIMATED | clock was restored from the newest record at boot; timestamps are *monotonic but offset* until the next sync |
| 2 | LIDAR_OK | at least one valid distance sample |
| 3 | TILT_OK | IMU read succeeded |
| 4 | MANUAL | triggered from the shell / BLE, not the schedule |
| 5 | FIRST_AFTER_BOOT | first record after a reset (a reset marker for the analysis) |

Neither time flag set and epoch = 0: the clock had never been set and there
was no previous record to restore from.

Snow depth is *not* stored; it is computed offline from the ZERO reference
(spec §4.5): `depth = (d0 - dist_median) * cos(tilt)`.

## Where records live

1. **Primary: LittleFS on the 2 MB QSPI flash**, mounted at `/lfs1` (the path nRF Connect Device Manager hard-codes).
   One file per month, records appended back to back:
   `/lfs1/rec_YYYYMM.bin` (UTC month), or `/lfs1/rec_notime.bin` while the
   clock is unset. These files are what the BLE file download (mcumgr fs,
   step 4c) fetches. 8 records/day = ~10 kB per month.
2. **Mirror: internal flash partition `mirror_partition`** (0x9F000–0xEC000,
   308 kB = 7,884 records ≈ 2.7 years at 8/day). Raw append-only 40-byte
   slots, written right after the LittleFS append. Used to restore the
   clock estimate and the sequence counter after a reset, for the shell
   `rec dump`, and as the fallback if the QSPI file system is unusable.
   When it is full, records keep going to LittleFS only.

Internal flash layout (overrides the board's UF2 default; the bootloader
regions are untouched):

| address | size | use |
|---|---:|---|
| 0x00000 | 156 kB | Adafruit bootloader / SoftDevice area |
| 0x27000 | 480 kB | application |
| 0x9F000 | 308 kB | `mirror_partition` |
| 0xEC000 | 32 kB | `storage_partition` (settings; calibration data later) |
| 0xF4000 | 48 kB | bootloader UF2 area |

## Clock

The nRF52840 has no calendar RTC. Zephyr's emulated RTC (`rtc-emul`,
ticking from the 32.768 kHz crystal) sits behind the `rtc` alias; it is what
the shell `time` command and, later, the mcumgr `datetime` command set.
The clock is lost on reset: at boot the firmware re-seeds it from the newest
record and marks records TIME_ESTIMATED until the next external sync.

## Shell

```
measure              one cycle, stored (flag MANUAL); prints the record as CSV
rec count            record count, next seq, file system / mirror state, newest record
rec ls               files on /lfs1 with sizes
rec dump [n]         newest n records from the mirror as CSV (0 = all)
rec erase ERASE      delete all record files + erase the mirror
time                 show the clock and its state
time set <epoch>     set the clock (UTC seconds, e.g. `date +%s`)
```

## Offline decoding

```bash
python3 tools/decode_records.py rec_202612.bin rec_202701.bin > records.csv
```

Bad or torn records are skipped with a message on stderr; the decoder
re-synchronises on the next magic word.
