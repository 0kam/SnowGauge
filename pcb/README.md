# SnowGauge PCB v1.2 (TFmini Plus / TSD20 common board)

- Board: **64 x 50 mm**, 2-layer, all through-hole. Bottom layer is a GND pour.
- Mounting: 4x M3 (3.2mm) holes at the corners, **57 x 43 mm** center spacing (3.5mm from every edge; hole centers at (3.5, 3.5) / (60.5, 3.5) / (3.5, 46.5) / (60.5, 46.5) from the board's top-left corner).
- Fits the previous prototype enclosure (Takachi BCAP091208G, inner 66 x 96 mm) with the 50mm side across the width.
- Battery terminal J1 exits at the bottom edge; TFmini terminal J2 exits at the right edge.
- Generated programmatically: [generate_board.py](generate_board.py) → `SnowGauge.kicad_pcb` (KiCad 10). DRC: 0 errors, 0 unconnected.
- One board serves both variants: **U2 = NJU7223F50 for TFmini Plus / NJU7223F33 for TSD20** (same pinout).

## Ordering (JLCPCB, ~1 week to Japan)

1. Go to jlcpcb.com → "Add gerber file" → upload `SnowGauge_v1.2_gerbers.zip`
2. Settings: 2 layers, 64x50mm (auto-detected), qty 5 (or 10), 1.6mm, HASL, green — all defaults are fine
3. Shipping: **DHL or OCS Express** (this is what makes it arrive in ~3-4 days; economy mail takes weeks)

## Firmware pin map (matches this board — the single source of truth)

| XIAO pin | Net | Function |
|---|---|---|
| A0 (D0) | ADC_NODE | Battery voltage = reading x2. Divider hangs on VBAT_SW, so **enable SENSOR_EN, then read A0** (v1.2: Q3/R6/D3 deleted — node is 0V during sleep, never above VDD) |
| D4 / D5 | I2C_SDA / I2C_SCL | Spare I2C header J3 (future external IMU) |
| D6 (TX) | UART_TX_MCU | → 1k (R7) → TFmini RX (white) |
| D7 (RX) | UART_RX | ← TFmini TX (green) |
| **D10** | SENSOR_EN | High = sensor rail ON (Q2→Q1). *Changed from D2 in the hand-wiring plan.* |
| 3V3 | 3V3_SYS | Powered from U1 (NJU7223F33) |

## Connectors

- J1 (screw terminal 2P, 5.08mm): battery + / − ("+"/"-" on silk)
- J2 (screw terminal 2P, 5.08mm): TFmini RED +5V / BLK GND
- J4 (screw terminal 2P, 5.08mm): TFmini GRN TX / WHT RX
- J3 (pin header 1x4): SCL / SDA / GND / 3V3 spare I2C
- A1: 2x low-profile 1x7 female headers (2.54mm) — socket for XIAO nRF52840 Sense

## Extra BOM lines (in addition to spec §6)

| Part | Qty | Note |
|---|---|---|
| Screw terminal block 2P, **5.08mm pitch** (TB111-2 / MKDS-1,5 type) | 3 | J1 battery, J2 TFmini power, J4 TFmini UART |
| Female pin socket 1x7, 2.54mm | 2 | XIAO socket. Holes are 0.89mm: fine for round machined tails (~0.5mm) and typical stamped tails; avoid 0.64mm square-tail types |
| 10uF radial MLCC, **5mm lead pitch** | 1 | C6. A 2.5mm-pitch part needs its leads splayed |
| Pin header 1x4, 2.54mm | 1 | J3 (optional) |
| M3 screws/standoffs | 4 | mounting holes at corners |

## Safety notes silkscreened on the board

- "NO USB WHILE BATTERY CONNECTED!" — disconnect battery before plugging USB into the XIAO
- C4 (OS-CON) polarity marked; D1 cathode marked "K"

## v1.2 changes (post-review)

- **Battery divider now fed from VBAT_SW** (the switched rail) instead of VBAT: the ADC node sits at 0V during sleep instead of floating to ~7V through the nRF52840's clamp diode (abs-max violation found in review). Q3, R6 and the D3 GPIO are deleted. Firmware measures battery by enabling SENSOR_EN and reading A0 (x2).
- **USB corridor cleared**: U1 and C1 moved to the top-left, D1 lowered — a USB-C cable now reaches the socketed XIAO for flashing.
- TFmini wire-order legend moved where it stays visible after C4 is mounted.
- Terminals changed to **3x 5.08mm 2P blocks** (J1 battery / J2 TFmini power / J4 TFmini UART) per builder preference — standard Akizuki TB111-class parts drop in.
