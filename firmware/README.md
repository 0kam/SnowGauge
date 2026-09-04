# SnowGauge firmware

Zephyr / nRF Connect SDK application for the Seeed XIAO nRF52840 Sense on
SnowGauge PCB v1.2 (or the equivalent breadboard, `docs/breadboard_guide.html`).

- SDK: **nRF Connect SDK v3.4.0** (installed with `nrfutil sdk-manager`)
- Board target: `xiao_ble/nrf52840/sense`
- Pin map source of truth: [`pcb/README.md`](../pcb/README.md)
- User-facing docs (Japanese): [`docs/03_firmware.md`](../docs/03_firmware.md) (flashing), [`docs/04_app.md`](../docs/04_app.md) (field app), [`docs/05_data.md`](../docs/05_data.md) (data)

## Status: step 4d complete (2026-09-04) — records + BLE advertising + SMP/mcumgr + schedule/settings + calibration GATT (26 µA sleep incl. adv)

| Module | File | Purpose |
|---|---|---|
| Sensor rail | `src/sensor_rail.c` | D10 = SENSOR_EN. Parks the TFmini UART (pins Hi-Z) *before* cutting the rail (ghost-power countermeasure) |
| TFmini Plus | `src/tfmini.c` | Interrupt-driven 9-byte frame parser, N-sample burst → median / variance / quality counters, frame-rate command |
| Battery | `src/battery.c` | A0 via SAADC (gain 1/6, 40 µs acquisition for the 500 kΩ source). Valid only while the rail is on; Vbat = node × 2 |
| Tilt | `src/tilt.h`, `src/tilt_lsm6dsl.c` | `TiltSensor` abstraction; LSM6DS3TR-C implementation (accel only, 52 Hz burst, powered down between reads). Deferred init: the board's regulator delay is too short for the chip |
| Measure | `src/measure.c` | tilt → rail on → Vbat → N frames → Vbat → rail off; `sensor_lock` mutex shared with the calibration live mode |
| Power | `src/power.c` | Green LED pulse; checks that the QSPI flash runs under runtime PM (deep power-down between accesses) |
| USB PM | `src/usb_pm.c` | USB device enabled only while VBUS is present (board default keeps HFXO+USBD on: 1.8 mA) |
| Record | `src/record.c` | 40-byte record v1 encode/decode with CRC-16 (`docs/record_format.md`) |
| Storage | `src/storage.c` | LittleFS on the QSPI (`/lfs1/rec_YYYYMM.bin`) + raw append-only mirror in internal flash; restores clock/seq at boot |
| Clock | `src/timekeeping.c` | Emulated RTC behind the `rtc` alias; UNSET / ESTIMATED / SYNCED state |
| Config | `src/config.c` | Zephyr settings (`sg/sched/*`, `sg/cal/*`) exposed over the SMP settings group; scheduler (local time window + interval, tz offset) and the calibration reference d0/θ0 |
| BLE adv | `src/ble_adv.c` | Connectable advertising 1–2 s, name `SG-XXXX`, Manufacturer Data with Vbat / record count / last distance / flags (layout in `ble_adv.h`); restarts advertising after a disconnect |
| SMP | `src/smp_mgmt.c` | mcumgr hooks: os datetime get/set → app clock, fs access hook (a read of `rec_*.bin` unlocks the BLE ERASE), over BLE and over the USB shell |
| Calibration GATT | `src/cal_gatt.c` | Custom service (UUIDs in `cal_gatt.h`): live notify (dist/strength/tilt/vbat/quality, 5 min timeout, stops on disconnect), control write (live on/off, ZERO, reference from probed depth, ERASE + token), status read (d0/θ0/set epoch/live/busy/seq) on its own work queue |
| Shell | `src/shell_cmds.c` | Bench commands over USB CDC ACM (table below) |

Phone / PC UI: the Web Bluetooth page at **https://0kam.github.io/SnowGauge/app/** (source `docs/app/`, see `docs/app/README.md`). Bench SMP from the Mac: `tools/smp_datetime.py` (pip install smpclient); shell scripting: `tools/sgshell.py` (pyserial). Sleep-current results: `../docs/measurements/2026-09-03_breadboard_current.md`. Not yet: TSD20 variant (step 5, see the development order in `../CLAUDE.md`).

## Prebuilt binaries

Firmware builds are published as GitHub Releases, tag `fw-YYYY-MM-DD`. Current:
**https://github.com/0kam/SnowGauge/releases/tag/fw-2026-09-04**

| Asset | Use |
|---|---|
| `snowgauge_fw_2026-09-04.uf2` | UF2 drag-and-drop onto the `XIAO-SENSE` drive (Windows / Linux / older macOS) |
| `snowgauge_fw_2026-09-04_dfu.zip` | Serial DFU package for `adafruit-nrfutil` (required on macOS 26); already built with `--sd-req 0xFFFE` |
| `snowgauge_fw_2026-09-04.hex` | Application hex (linked at 0x27000) for building your own package |

Step-by-step flashing instructions for non-developers: [`docs/03_firmware.md`](../docs/03_firmware.md). The app requires this release or newer (settings group + calibration service).

## Build

One-time: install nrfutil and the SDK (already done on the dev Mac; `~/.local/bin/nrfutil`).

```bash
nrfutil sdk-manager install v3.4.0
```

Build inside the SDK toolchain environment. `west` must be started from the SDK workspace (`/opt/nordic/ncs/v3.4.0`), so pass absolute paths:

```bash
cd /opt/nordic/ncs/v3.4.0 && nrfutil sdk-manager toolchain launch --ncs-version v3.4.0 -- west build -d "$REPO/firmware/build" -b xiao_ble/nrf52840/sense "$REPO/firmware"
```

Add `-p` after `west build` for a pristine rebuild; `-- -DEXTRA_CONF_FILE=overlay-auto60.conf` builds the bench variant that measures every 60 s. Output: `firmware/build/firmware/zephyr/zephyr.uf2` and `zephyr.hex` (sysbuild layout).

## Flash (stock Adafruit bootloader, no debugger needed)

The app is linked at 0x27000, after the Adafruit bootloader, so the bootloader is preserved and the board can always be re-flashed.

**UF2 (Windows / Linux / older macOS):** disconnect the battery (**never USB and battery at the same time** unless jumper ⑩ is pulled — see the breadboard guide), connect USB-C, double-tap reset, copy `firmware/build/firmware/zephyr/zephyr.uf2` onto the `XIAO-SENSE` drive.

**Serial DFU (needed on macOS 26, which cannot mount the XIAO's UF2 drive):**

```bash
pip install adafruit-nrfutil
adafruit-nrfutil dfu genpkg --dev-type 0x0052 --sd-req 0xFFFE --application firmware/build/firmware/zephyr/zephyr.hex snowgauge.zip
```

Put the board in the bootloader (double-tap reset, or type `dfu` in the firmware shell), then:

```bash
adafruit-nrfutil dfu serial --package snowgauge.zip -p /dev/cu.usbmodem* -b 115200 --singlebank
```

`--sd-req 0xFFFE` (any SoftDevice) matters: with `0x00` the bootloader rejects the package and resets mid-transfer.

Release assets are produced from the same build: `zephyr.uf2` and `zephyr.hex` renamed, plus the `genpkg` zip above.

## Shell (USB CDC ACM, prompt `sg:~$ `, 115200)

Open the USB serial port (`/dev/cu.usbmodem*`, `COMx`, `/dev/ttyACM0`) with e.g. `screen`, Tera Term or `tools/sgshell.py`, press Enter for the prompt.

| Command | Purpose |
|---|---|
| `help` | list commands (`cal` and `cfg` present = step 4d firmware) |
| `rail on` / `rail off` / `rail status` | sensor rail (boots OFF; `rail off` parks the UART first) |
| `tfmini raw [ms]` | live frames for `ms` (needs `rail on`) |
| `tfmini read [n] [timeout_ms]` | n-frame burst: median / variance / strength / temperature |
| `tfmini rate <hz>` / `tfmini save` | TFmini frame rate (volatile until saved) |
| `batt` | battery voltage (pulses the rail if it is off) |
| `tilt [n]` | IMU tilt / pitch / roll from n averaged samples |
| `measure` | one full cycle, stored as a record (flag MANUAL), printed as CSV |
| `auto [s]` | bench auto-measurement period (0 = off); overrides the schedule |
| `rec count` / `rec ls` / `rec dump [n]` / `rec hex [n]` | record count + storage state / files on `/lfs1` / newest n records as CSV (0 = all) / raw hex for `tools/decode_records.py --hex` |
| `rec erase ERASE` | delete all record files + erase the mirror |
| `time [set <epoch>]` | show / set the clock (UTC seconds, `date +%s`); shows synced / estimated / unset |
| `cfg show` | schedule, tz, calibration reference, next scheduled measurement |
| `cfg sched <HH:MM> <HH:MM> <interval_min> [tz_min]` | set the schedule (start == end: all day; start > end: spans midnight; interval 0: off) |
| `cal zero [depth_cm]` | take the reference (ZERO on bare ground, or from a probed snow depth: `d0 = d + depth/cos(tilt)`) |
| `cal clear` | clear the reference |
| `cal live [off]` | live-mode state; `off` stops it (start only from the BLE page) |
| `ble [adv <min_ms> [max_ms]]` | advertising status / interval (`ble adv 0` stops) |
| `reboot` | warm reset into the application |
| `dfu` | reboot into the Adafruit bootloader (then run `adafruit-nrfutil dfu serial`) |
| `device list`, `sensor get lsm6ds3tr-c@6a` | Zephyr built-ins (debug) |

Bench test (breadboard STEP 5 / PCB bring-up): `rail status` → `rail on` (5 V node = 5 V, TFmini LED on) → `tfmini raw 500` → `tfmini read 100` → `batt` → `rail off` (5 V node = 0 V) → `measure`. Measured on the breadboard (2026-09-03, 6 V bench supply): `vbat = 5624 mV`, 100 frames / 997 ms, `cksum_err=0`, median 197 cm, strength ~6830, chip temp 65 °C. If the TFmini stays dimly alive with the rail off, the UART is not parked — check that `rail off` returned 0.

Boot behaviour worth knowing on the bench: the first *scheduled* measurement is held off for 10 min × consecutive-reset count (`CONFIG_SNOWGAUGE_BOOT_HOLDOFF_MIN`); `measure` and `auto` are not affected. Vbat < 4600 mV skips the TFmini burst.

## Shell output fields

`weak` = strength < 100 (Benewake: unreliable), `sat` = strength 65535 (over-exposed), `invalid` = distance sentinel (0 / 65535) with acceptable strength, `cksum_err` = frames dropped by checksum. `var` is the sample variance in cm² over the valid frames.

## Notes / decisions pending

- **No MCUboot / BLE DFU in v1 (decided 2026-09-02, spec v0.11)**: the XIAO ships with the Adafruit UF2 bootloader at 0x0 and installing MCUboot would overwrite it, which needs an SWD probe. The firmware is flashed over USB as UF2 / serial DFU; field updates mean opening the enclosure and plugging in USB (battery disconnected).
- Build reproducibility: the SDK version is pinned here (v3.4.0) and the build uses the SDK's own workspace. There is **no `west.yml` manifest in this repo**; add one for a self-contained workspace when the build moves to CI.
- No pairing: anyone nearby can erase (token-protected, and only after a download since boot) or change settings. PIN later if needed.
- WDT (spec §12.3) still off: the nRF52 WDT keeps running across a soft reset into the Adafruit bootloader; enable only with a long timeout and verify the `dfu` path.
