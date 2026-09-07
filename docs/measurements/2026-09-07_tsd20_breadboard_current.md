# Breadboard current measurements, TSD20 variant — 2026-09-07

Setup: same breadboard as `2026-09-03_breadboard_current.md` with **U2 = NJU7223F33 and a
PONO TSD20** on the switched 3.3 V rail (`docs/breadboard_guide_tsd20.html`), XIAO fed
through jumper ⑩, USB disconnected. Supply: bench PSU 6.00 V, **picowatt** (INA228, 15 mΩ,
ADCRANGE=1) in the supply line. Firmware: TSD20 build of commit `81eed78`
(`overlay-tsd20.conf`, N = 100 frames at 200 Hz, rail settle 300 ms, BLE advertising 1–2 s).
The scheduled measurement was provoked by setting the schedule to all day / 30 min from the
app and capturing across 13:00 (`--preset normal`, 908 Sa/s).

## Measurement burst (IMU + rail + 100 TSD20 frames), one scheduled cycle

| Item | Value | Note |
|---|---|---|
| Duration (rail on → rail off) | **0.95 s** | TFmini: 1.58 s |
| Inrush at rail switch-on | 650–670 mA for ~2 ms | C4 (470 µF) charging; 2 samples |
| Sensor boot phase | ≈ 11 mA for ~150 ms | rail on until the TSD20 starts streaming |
| Streaming plateau | **41.7 mA** (max 44.7) for ~0.75 s | manual: average 40 mA, peak 70 mA |
| Charge per cycle | **35.3 mA·s = 0.0098 mAh** | TFmini: 0.037 mAh → **≈ ¼** |
| 8 cycles/day | 0.078 mAh/day ≈ **0.9 µA average** | TFmini: 3.5 µA |

Profile (50 ms bins, baseline subtracted): rail on at t = 0 → 11 mA until +0.15 s →
42 mA from +0.15 s (sensor streaming; the firmware only starts reading at +0.30 s) →
off at +0.90 s. The rail settle time could be shortened to ~200 ms (saves ~4 mA·s, 10 %).
Data: `data/2026-09-07_tsd20_burst_900Hz.csv` (burst window only).

BLE advertising spikes seen in the same capture: every ~1 s, mean 4.6 mA above baseline
per sample, max 31 mA, as before.

## Sleep current (whole system, TSD20 build, advertising on)

Difference method as in the 09-03 notes (`--preset very-quiet`, 7.6 Sa/s).

| Item | Value | Note |
|---|---|---|
| Raw, DUT connected (60 s) | −37.7 µA (sd 43) | 12:47, sd = advertising bursts |
| Zero, DUT lead c2 pulled (40 s) | −73.6 µA (sd 25) | 13:06; zero drift vs. 09-04 (−70 µA) ≈ 3 µA |
| Raw, DUT reinserted (60 s) | −40.1 µA (sd 41) | 13:09; drift check: 2.4 µA from the first run (a reset + boot hold-off in between) |
| **Sleep + advertising, whole system, TSD20** | **36 µA ± 10** | TFmini board with the same firmware generation: 32 µA ± 10 (09-04) → no measurable difference |

Sensor-side change vs. the TFmini board is only U2 (F33 instead of F50, same family) and
the sensor itself (no power when the rail is off), so no difference is expected.
