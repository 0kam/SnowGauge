# SnowGauge PCB v1.0 (TFmini Plus / TSD20 common board)

- Board: 80 x 62 mm, 2-layer, all through-hole. Bottom layer is a GND pour.
- Generated programmatically: [generate_board.py](generate_board.py) → `SnowGauge.kicad_pcb` (KiCad 10). DRC: 0 errors, 0 unconnected.
- One board serves both variants: **U2 = NJU7223F50 for TFmini Plus / NJU7223F33 for TSD20** (same pinout).

## Ordering (JLCPCB, ~1 week to Japan)

1. Go to jlcpcb.com → "Add gerber file" → upload `SnowGauge_v1.0_gerbers.zip`
2. Settings: 2 layers, 80x62mm (auto-detected), qty 5 (or 10), 1.6mm, HASL, green — all defaults are fine
3. Shipping: **DHL or OCS Express** (this is what makes it arrive in ~3-4 days; economy mail takes weeks)

## Firmware pin map (matches this board — the single source of truth)

| XIAO pin | Net | Function |
|---|---|---|
| A0 (D0) | ADC_NODE | Battery voltage (reads Vbat/2 when enabled) |
| D3 | VBAT_MEAS_EN | High = enable battery divider (Q3) |
| D4 / D5 | I2C_SDA / I2C_SCL | Spare I2C header J3 (future external IMU) |
| D6 (TX) | UART_TX_MCU | → 1k (R7) → TFmini RX (white) |
| D7 (RX) | UART_RX | ← TFmini TX (green) |
| **D10** | SENSOR_EN | High = sensor rail ON (Q2→Q1). *Changed from D2 in the hand-wiring plan.* |
| 3V3 | 3V3_SYS | Powered from U1 (NJU7223F33) |

## Connectors

- J1 (screw terminal 2P, 5.0/5.08mm pitch): battery + / −
- J2 (screw terminal 4P): TFmini Plus — RED +5V / BLK GND / GRN TX / WHT RX
- J3 (pin header 1x4): SCL / SDA / GND / 3V3 spare I2C
- A1: 2x low-profile 1x7 female headers (2.54mm) — socket for XIAO nRF52840 Sense

## Extra BOM lines (in addition to spec §6)

| Part | Qty | Note |
|---|---|---|
| Screw terminal block 2P, 5.0mm pitch | 1 | e.g. Akizuki TB111 series or equivalent |
| Screw terminal block 4P, 5.0mm pitch | 1 | (2P x2 also fine) |
| Female pin socket 1x7, 2.54mm | 2 | XIAO socket |
| Pin header 1x4, 2.54mm | 1 | J3 (optional) |
| M3 screws/standoffs | 4 | mounting holes at corners |

## Safety notes silkscreened on the board

- "NO USB WHILE BATTERY CONNECTED!" — disconnect battery before plugging USB into the XIAO
- C4 (OS-CON) polarity marked; D1 cathode marked "K"
