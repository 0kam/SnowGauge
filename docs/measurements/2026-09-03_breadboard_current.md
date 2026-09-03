# Breadboard current measurements — 2026-09-03

Setup: breadboard (docs/breadboard_guide.html, resistor substitutes R3=150Ω, R4=R5=10k,
R1=R2=5k, R7=2.2k), XIAO nRF52840 Sense fed through jumper ⑩ (3V3 pin), TFmini Plus on
the switched 5 V rail. Supply: bench PSU 6.00 V in place of the L91×4 pack, measured with
**picowatt** (INA228, 15 mΩ shunt, ADCRANGE=1, `very-quiet` preset = 4120 µs × 16 avg,
~7.6 Sa/s). Firmware: step 3 prep build (`3e31596` + VBUS-gated USB), auto measurement
every 60 s, N=100 TFmini frames.

## Method notes (picowatt at µA level)

- 1 µV across 15 mΩ = 67 µA, so terminal thermo-EMF / contact potentials matter. The
  INA228 zero moved by tens of µA whenever wiring was touched. Reliable procedure:
  **difference measurement** — pull only the DUT lead (breadboard c2) without touching the
  picowatt terminals, record the zero (40 s), reinsert, record (60 s), subtract.
- Per-sample noise ≈ 18–20 µA (sd) at `very-quiet`; 60 s averages have ~1 µA standard
  error; zero drift over minutes ≈ ±10 µA → quote results as ±10 µA.
- A broken supply(−)–Pico GND link shows up as a 50 Hz half-wave on VBUS (mean ≈ 18 V for
  a 6 V supply) and 250 µA-class current noise. Check `--preset normal` VBUS sd first.

## Results

| Item | Value | Note |
|---|---|---|
| Sleep current, whole system, **USB enabled at boot (board default)** | **1.89 mA** | udc_nrf keeps HFXO + USBD on regardless of VBUS |
| Sleep current, whole system, **USB gated by VBUS** (`usb_pm.c`) | **17 µA ± 10** | raw −53.3 µA vs zero −70.0 µA; 10 s blocks within ±3 µA |
| Sensor side only (XIAO on USB, ⑩ pulled) | ≈ 45 µA (uncertain zero) | U1 quiescent + D1 leakage; superseded by the whole-system figure |
| Measurement burst (IMU + 100 TFmini frames) | 1.58 s, peak 115–117 mA, **133–138 mA·s = 0.037 mAh** | 8/day → 0.30 mAh/day ≈ 3.5 µA average |
| TFmini alone (accidentally on VBAT rail) | 117.9 mA continuous | wiring error found this way |

Budget check (spec §7.1 assumed 50 µA typ / 90 µA worst sleep + 15–35 µA measurements):
measured ≈ 17 + 3.5 ≈ **21 µA** before BLE advertising. BLE adv (1–2 s interval) is
expected to add ~10–20 µA. Final numbers to be re-measured on PCB v1.2.

## Re-measurement with FW step 4a (record storage), 2026-09-03 afternoon

Firmware `1dccdfa` (LittleFS mounted on the QSPI, QSPI under runtime PM / deep
power-down, rtc-emul 1 Hz work item, mirror partition), shell-only build (no auto
measurement), PSU 6.00 V, USB disconnected, ⑩ in. Same difference method.

| Item | Value | Note |
|---|---|---|
| Raw, DUT connected (60 s) | −53.9 µA (sd 18) | |
| Zero, DUT lead c2 pulled (40 s) | −71.7 µA (sd 24) | |
| **Sleep current, whole system, step 4a** | **18 µA ± 10** | unchanged vs. step 3 (17 µA) → LittleFS/runtime-PM QSPI and the 1 Hz rtc-emul tick cost nothing measurable |
