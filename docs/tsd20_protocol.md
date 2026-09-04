# PONO TSD20 — protocol notes for the firmware variant (step 5)

Source: TSD20 user manual (PDF hosted by Akizuki, https://akizukidenshi.com/goodsaffix/TSD20%20user%20manual.pdf), read 2026-09-04. Product page: 秋月 131304.

## Electrical

| Item | Value | SnowGauge note |
|---|---|---|
| Supply | 3.3–3.6 V DC, peak 70 mA, average 40 mA (< 0.2 W) | Board variant U2 = NJU7223F33 (3.3 V switched rail). **No reverse/overvoltage protection on the sensor** |
| Interface | UART (3.3 V logic) or I2C (addr 0x52, 400 kHz) | UART on D6/D7 as with the TFmini; direct connection |
| Connector | 6-pin 0.8 mm terminal, 20 cm tinned stranded wires | Pin 1 NC, **2 = 3.3 V**, **3 = TX (→ XIAO D7 RX)**, **4 = RX (← XIAO D6 TX via R7)**, 5 NC, **6 = GND**. Wire colours not given in the manual: identify by pin position |
| Range / accuracy | 0.05–20 m (90 % refl.), 0.05–10 m (10 % refl.); ±5 cm < 5 m, 1 % ≥ 5 m; repeatability ±10 mm; resolution 1 mm | Snow: expect the 10 % reflectivity figure |
| Rate | 200 Hz default; 100/50/20/10/1 Hz selectable | 100 frames = 0.5 s at 200 Hz |
| Optics | 905 nm, FOV 3°, spot 5 cm @1 m … 100 cm @20 m; ambient light 8 m @ 100 kLux | Class 1 |
| Temperature | −20…+60 °C | |

## UART

- **460800 8N1 default**. Baud command `5A 06 02 <baud/100 LE16> CK` (115200 = `80 04`, 460800 = `00 12`). Any other rate is rejected. nRF52840 UARTE supports 460800; keep the default unless the breadboard proves noisy.
- **Output frame, 4 bytes, little-endian:** `5C` `distL` `distH` `CK` — distance in **mm** (0–65535), **50000 = out of range / no target**.
- **Checksum:** `CK = ~(sum of bytes from the 2nd to the second-last)` → for the data frame `CK = (uint8_t)~(distL + distH)`.
- **No signal-strength or temperature field** (unlike the TFmini). Quality must come from variance, out-of-range count and frame count only.
- Commands (same checksum rule over bytes 2..n-1):
  - start ranging `5A 0A 02 02 00 F1` (reply `5A 8A 02 02 00 71`), stop `5A 0A 02 00 00 F3`
  - frequency `5A 0B 02 <div LE16> CK`, f = 10000/(div+1): 200 Hz → div 49 (`31 00`), 100 Hz → 99 (`63 00`), 10 Hz → 999 (`E7 03`)
  - serial number `5A 0D 04 0D 0D 0D BA`, software version `5A 16 02 16 16 BB`
  - The sensor streams frames as soon as it is powered (the quick-test section connects and reads immediately); whether the stop/start state is persistent across power cycles is not stated — the driver must not rely on it and should send "start ranging" after the rail settles.

## Firmware plan (step 5)

1. `Kconfig` choice `SNOWGAUGE_SENSOR` = `TFMINI` (default) | `TSD20`; `overlay-tsd20.conf` selects it and sets `uart0` `current-speed = <460800>` via a DT overlay fragment.
2. Put the sensor behind `lidar.h` (`lidar_init/flush/capture/read_frame/set_frame_rate`, `struct lidar_stats` = today's `tfmini_stats`); `tfmini.c` and new `tsd20.c` implement it. TSD20: 4-byte parser, mm → cm (round) for the v1 record, sentinel 50000 → `n_invalid`, `strength_median` = 0 and `n_weak` = 0 (no strength), `temp_c_x10` = INT16_MIN.
3. Rail: unchanged (`SENSOR_EN`); settle time may be shorter; measure boot-to-first-frame with the shell `tfmini raw` equivalent (`lidar raw`).
4. Record: set new flag bit 6 `RECORD_FLAG_SENSOR_TSD20` so CSV/analysis can tell the variants apart (update `record.h`, `docs/record_format.md`, `app.js` `RECORD_SCHEMA.flags`, `tools/decode_records.py`). BLE name prefix `ST-` for the TSD20 build (app `namePrefix` filter: accept both `SG-` and `ST-`).
5. Release as a second asset in the same tag: `snowgauge_fw_<date>_tsd20.uf2` / `_tsd20_dfu.zip`.
6. Bench: same STEP 5 check with the TSD20 on the 3.3 V rail (U2 = F33), current profile with picowatt (expect ≈ 40 mA × 0.5 s per measurement).
