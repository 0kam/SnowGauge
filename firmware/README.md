# SnowGauge firmware

Zephyr / nRF Connect SDK application for the Seeed XIAO nRF52840 Sense on
SnowGauge PCB v1.2 (or the equivalent breadboard, `docs/breadboard_guide.html`).

- SDK: **nRF Connect SDK v3.4.0** (installed with `nrfutil sdk-manager`)
- Board target: `xiao_ble/nrf52840/sense`
- Pin map source of truth: [`pcb/README.md`](../pcb/README.md)

## Status: step 2 — measurement sequence (verified on the breadboard 2026-09-03)

| Module | File | Purpose |
|---|---|---|
| Sensor rail | `src/sensor_rail.c` | D10 = SENSOR_EN. Parks the TFmini UART (pins Hi-Z) *before* cutting the rail (ghost-power countermeasure) |
| TFmini Plus | `src/tfmini.c` | Interrupt-driven 9-byte frame parser, N-sample burst → median / variance / quality counters, frame-rate command |
| Battery | `src/battery.c` | A0 via SAADC (gain 1/6, 40 µs acquisition for the 500 kΩ source). Valid only while the rail is on; Vbat = node × 2 |
| Tilt | `src/tilt.h`, `src/tilt_lsm6dsl.c` | `TiltSensor` abstraction; LSM6DS3TR-C implementation (accel only, 52 Hz burst, powered down between reads). Deferred init: the board's regulator delay is too short for the chip |
| Measure | `src/measure.c` | tilt → rail on → Vbat → N frames → Vbat → rail off |
| Shell | `src/shell_cmds.c` | Bench commands over USB CDC ACM |

Not yet: LittleFS records, BLE, sleep-current tuning (see the development order in `../CLAUDE.md`).

## Build

One-time: install nrfutil and the SDK (already done on the dev Mac; `~/.local/bin/nrfutil`).

```bash
nrfutil sdk-manager install v3.4.0
```

Build inside the SDK toolchain environment. `west` must be started from the SDK workspace (`/opt/nordic/ncs/v3.4.0`), so pass absolute paths:

```bash
cd /opt/nordic/ncs/v3.4.0 && nrfutil sdk-manager toolchain launch --ncs-version v3.4.0 -- west build -d "$REPO/firmware/build" -b xiao_ble/nrf52840/sense "$REPO/firmware"
```

Add `-p` after `west build` for a pristine rebuild. Output: `firmware/build/firmware/zephyr/zephyr.uf2` (sysbuild layout).

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

## Bench test (breadboard STEP 5 / PCB bring-up)

Open the USB serial port (`/dev/cu.usbmodem*`, any baud rate) with e.g. `screen` or the nRF Connect Serial Terminal, then:

```
rail status           # boots with the rail OFF
rail on               # SENSOR_EN high; 5V node should read 5 V, TFmini LED on
tfmini raw 500        # live frames for 0.5 s
tfmini read 100       # 100-frame burst: median / variance / strength / temperature
batt                  # battery voltage (pulses the rail if it is off)
rail off              # UART Hi-Z first, then SENSOR_EN low; 5V node should read 0 V
measure               # the whole cycle in one command
tilt [n]              # IMU tilt / pitch / roll from n averaged samples
dfu                   # reboot into the bootloader (then run adafruit-nrfutil dfu serial)
device list           # Zephyr device states (debug); 'sensor get lsm6ds3tr-c@6a' also works
auto 30               # repeat every 30 s (auto 0 to stop)
```

Expected: with the rail off the 5 V node is 0 V and the TFmini draws nothing. Measured on the breadboard (2026-09-03, 6 V bench supply): `vbat = 5624 mV`, 100 frames / 997 ms, `cksum_err=0`, median 197 cm, strength ~6830, chip temp 65 °C. If the TFmini stays dimly alive with the rail off, the UART is not parked — check that `rail off` returned 0.

## Shell output fields

`weak` = strength < 100 (Benewake: unreliable), `sat` = strength 65535 (over-exposed), `invalid` = distance sentinel (0 / 65535) with acceptable strength, `cksum_err` = frames dropped by checksum. `var` is the sample variance in cm² over the valid frames.

## Notes / decisions pending

- **No MCUboot / BLE DFU in v1 (decided 2026-09-02, spec v0.11)**: the XIAO ships with the Adafruit UF2 bootloader at 0x0 and installing MCUboot would overwrite it, which needs an SWD probe. The firmware is flashed over USB as UF2; field updates mean opening the enclosure and plugging in USB (battery disconnected).
- Build reproducibility: the SDK version is pinned here (v3.4.0). A `west.yml` manifest for a self-contained workspace can be added when the build moves to CI.
