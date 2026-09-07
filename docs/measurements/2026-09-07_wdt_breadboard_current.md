# Breadboard sleep current with the watchdog build — 2026-09-07

Setup: TFmini breadboard as in `2026-09-03_breadboard_current.md` (U2 = NJU7223F50, TFmini
Plus on the switched rail), XIAO fed through jumper ⑩, USB disconnected. Supply: bench PSU
6.00 V, **picowatt** (INA228, 15 mΩ, ADCRANGE=1, `--preset very-quiet`, 7.6 Sa/s) in the
supply line. Firmware: TFmini build of commit `a5ec340` (hardware WDT 120 s, supervisor work
every 30 s, BLE advertising 1–2 s, payload v2). Captured during the boot hold-off, so no
measurement cycle is included.

## Sleep + advertising, whole system (difference method)

| Item | Value | Note |
|---|---|---|
| Raw, DUT connected (60 s) | −45.4 µA (sd 57) | 15:0x, sd = advertising bursts |
| Zero, DUT lead c2 pulled (40 s) | −66.9 µA (sd 34) | zero drifted ≈ +7 µA vs. 09-07 morning (−73.6) |
| Raw, DUT reinserted (60 s) | −44.9 µA (sd 42) | |
| **Sleep + advertising + WDT supervisor** | **22 µA ± 10** | 09-04 release build on the same board: 32 µA ± 10 |

The watchdog itself runs from the LFCLK that BLE keeps on anyway, and the supervisor wakes
the CPU for a few tens of µs every 30 s, so no increase was expected; the ~10 µA "drop" is
within the method's uncertainty (zero drift between runs is the dominant error). Conclusion:
enabling the WDT costs nothing measurable.

Data: `data/2026-09-07_wdt_sleep_{raw1,zero,raw2}.csv`.
