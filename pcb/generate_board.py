#!/usr/bin/env python3
"""Generate the SnowGauge TFmini Plus carrier board (KiCad 10, pcbnew API).

Run with KiCad's bundled python:
  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 generate_board.py
"""
import os
import pcbnew
from pcbnew import VECTOR2I, FromMM

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "SnowGauge.kicad_pcb")
FPLIB = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints"
XIAO_MOD = os.path.join(HERE, "lib", "XIAO-nRF52840-DIP.kicad_mod")

def mm(x, y):
    return VECTOR2I(FromMM(x), FromMM(y))

board = pcbnew.NewBoard(OUT)

# ---------------- nets ----------------
NET_NAMES = [
    "BATP_RAW", "VBAT", "GND", "3V3_SYS", "VBAT_SW", "5V_SENS",
    "Q1_GATE", "SENSOR_EN", "Q2_G", "VBAT_MEAS_EN", "ADC_NODE", "MEAS_LO",
    "UART_TX_MCU", "TFMINI_RX", "UART_RX", "I2C_SDA", "I2C_SCL",
]
nets = {}
for n in NET_NAMES:
    ni = pcbnew.NETINFO_ITEM(board, n)
    board.Add(ni)
    nets[n] = ni

# ---------------- footprints ----------------
def load(lib, name):
    fp = pcbnew.FootprintLoad(os.path.join(FPLIB, lib + ".pretty"), name)
    assert fp is not None, f"footprint not found: {lib}/{name}"
    return fp

def load_file(path):
    fp = pcbnew.FootprintLoad(os.path.dirname(path),
                              os.path.splitext(os.path.basename(path))[0])
    assert fp is not None, f"footprint not found: {path}"
    return fp

# ref: (loader-args, value, anchor(x,y), rot, {pad: net})
PARTS = {
    "J1": (("TerminalBlock", "TerminalBlock_MaiXu_MX126-5.0-02P_1x02_P5.00mm"),
           "BATTERY L91x4", (29, 30), 0, {"1": "BATP_RAW", "2": "GND"}),
    "D1": (("Diode_THT", "D_DO-41_SOD81_P10.16mm_Horizontal"),
           "1N5819", (49.7, 26), 180, {"1": "VBAT", "2": "BATP_RAW"}),
    "U1": (("Package_TO_SOT_THT", "TO-220-3_Vertical"),
           "NJU7223F33", (58, 26), 0, {"1": "3V3_SYS", "2": "VBAT", "3": "GND"}),
    "Q1": (("Package_TO_SOT_THT", "TO-220-3_Vertical"),
           "2SJ334", (70, 26), 0, {"1": "Q1_GATE", "2": "VBAT_SW", "3": "VBAT"}),
    "U2": (("Package_TO_SOT_THT", "TO-220-3_Vertical"),
           "NJU7223F50", (84, 26), 0, {"1": "5V_SENS", "2": "VBAT_SW", "3": "GND"}),
    "Q2": (("Package_TO_SOT_THT", "TO-251-3_Vertical"),
           "2SK4017", (69, 45), 0, {"1": "Q2_G", "2": "Q1_GATE", "3": "GND"}),
    "Q3": (("Package_TO_SOT_THT", "TO-251-3_Vertical"),
           "2SK4017", (30, 72), 0, {"1": "VBAT_MEAS_EN", "2": "MEAS_LO", "3": "GND"}),
    "R1": (("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal"),
           "1M", (25, 40), 270, {"1": "VBAT", "2": "ADC_NODE"}),
    "R2": (("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal"),
           "1M", (25, 54), 270, {"1": "ADC_NODE", "2": "MEAS_LO"}),
    "R3": (("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal"),
           "10k", (52, 48), 0, {"1": "SENSOR_EN", "2": "Q2_G"}),
    "R4": (("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal"),
           "100k", (72, 52), 0, {"1": "Q2_G", "2": "GND"}),
    "R5": (("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal"),
           "100k", (61, 36), 0, {"1": "VBAT", "2": "Q1_GATE"}),
    "R6": (("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal"),
           "100k", (36, 66), 0, {"1": "VBAT_MEAS_EN", "2": "GND"}),
    "R7": (("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal"),
           "1k", (52, 64), 0, {"1": "UART_TX_MCU", "2": "TFMINI_RX"}),
    "C1": (("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P5.00mm"),
           "0.1u", (50, 32), 0, {"1": "GND", "2": "VBAT"}),
    "C2": (("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P5.00mm"),
           "0.1u", (50, 38), 0, {"1": "3V3_SYS", "2": "GND"}),
    "C3": (("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P5.00mm"),
           "0.1u", (74, 32.5), 0, {"1": "GND", "2": "VBAT_SW"}),
    "C5": (("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P5.00mm"),
           "0.1u", (93, 30), 270, {"1": "5V_SENS", "2": "GND"}),
    "C6": (("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P5.00mm"),
           "10u", (40, 38), 0, {"1": "3V3_SYS", "2": "GND"}),
    "C4": (("Capacitor_THT", "CP_Radial_D10.0mm_P5.00mm"),
           "OS-CON 470u/16V", (84, 36), 270, {"1": "5V_SENS", "2": "GND"}),
    "J2": (("TerminalBlock", "TerminalBlock_MaiXu_MX126-5.0-04P_1x04_P5.00mm"),
           "TFmini R/B/G/W", (93, 47), 270, {"1": "5V_SENS", "2": "GND", "3": "UART_RX", "4": "TFMINI_RX"}),
    "J3": (("Connector_PinHeader_2.54mm", "PinHeader_1x04_P2.54mm_Vertical"),
           "I2C SPARE", (60, 74), 90, {"1": "I2C_SCL", "2": "I2C_SDA", "3": "GND", "4": "3V3_SYS"}),
    "A1": ("XIAO", "XIAO nRF52840 Sense", (38, 52), 90,
           {"1": "ADC_NODE", "4": "VBAT_MEAS_EN", "5": "I2C_SDA", "6": "I2C_SCL",
            "7": "UART_TX_MCU", "8": "UART_RX", "11": "SENSOR_EN",
            "12": "3V3_SYS", "13": "GND"}),
    "H1": (("MountingHole", "MountingHole_3.2mm_M3"), "M3", (24, 19.5), 0, {}),
    "H2": (("MountingHole", "MountingHole_3.2mm_M3"), "M3", (96, 19.5), 0, {}),
    "H3": (("MountingHole", "MountingHole_3.2mm_M3"), "M3", (24, 74.5), 0, {}),
    "H4": (("MountingHole", "MountingHole_3.2mm_M3"), "M3", (96, 74.5), 0, {}),
}

fps = {}
for ref, (src, value, pos, rot, padnets) in PARTS.items():
    fp = load_file(XIAO_MOD) if src == "XIAO" else load(*src)
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.SetPosition(mm(*pos))
    fp.SetOrientationDegrees(rot)
    board.Add(fp)
    for padnum, netname in padnets.items():
        matched = [p for p in fp.Pads() if p.GetNumber() == padnum]
        assert matched, f"{ref} pad {padnum} missing"
        for p in matched:
            p.SetNet(nets[netname])
    if ref.startswith("H"):
        for p in fp.Pads():
            p.SetLocalClearance(FromMM(0.6))
    fps[ref] = fp

def pad_xy(ref, num):
    pads = [p for p in fps[ref].Pads() if p.GetNumber() == num]
    th = [p for p in pads if p.HasHole()] or pads
    c = th[0].GetPosition()
    return (pcbnew.ToMM(c.x), pcbnew.ToMM(c.y))

# sanity-check XIAO through-hole pad geometry (15.24 mm row spacing)
p1, p7, p8, p14 = (pad_xy("A1", n) for n in ("1", "7", "8", "14"))
assert (abs(p1[0] - 30.38) < 0.05 and abs(p1[1] - 44.38) < 0.05
        and abs(p7[1] - 59.62) < 0.05 and abs(p14[0] - 45.62) < 0.05
        and abs(p8[0] - 45.62) < 0.05), \
    f"unexpected XIAO geometry: p1={p1} p7={p7} p8={p8} p14={p14}"

# ---------------- tracks ----------------
POWER_W, SIG_W = 1.2, 0.6

def track(netname, pts, layer=pcbnew.F_Cu, width=SIG_W):
    for a, b in zip(pts, pts[1:]):
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(mm(*a))
        t.SetEnd(mm(*b))
        t.SetWidth(FromMM(width))
        t.SetLayer(layer)
        t.SetNet(nets[netname])
        board.Add(t)

P = pad_xy
# battery -> diode -> VBAT rail (rail along y=23)
track("BATP_RAW", [P("J1", "1"), (29, 26), P("D1", "2")], width=0.8)
track("VBAT", [(25, 23), (75.08, 23)], width=POWER_W)                     # rail
track("VBAT", [P("D1", "1"), (49.7, 23)], width=POWER_W)                  # diode tap
track("VBAT", [P("U1", "2"), (60.54, 23)], width=POWER_W)                  # U1 IN
track("VBAT", [P("Q1", "3"), (75.08, 23)], width=POWER_W)                  # Q1 S
track("VBAT", [P("C1", "2"), (55, 23)], width=POWER_W)                     # C1
track("VBAT", [P("R1", "1"), (25, 23)], width=POWER_W)                     # divider top
# 3V3: U1 OUT -> mid channel -> XIAO 3V3 (+ C2, C6 on channel)
track("3V3_SYS", [P("U1", "1"), (58, 34), (38, 34), (38, 49.46), P("A1", "12")])
track("3V3_SYS", [P("C2", "1"), (50, 34)])
track("3V3_SYS", [P("C6", "1"), (40, 34)])
# high-side switch chain
track("Q1_GATE", [P("R5", "2"), (70, 36), P("Q1", "1")])
track("Q1_GATE", [(70, 36), (70, 43), (71.29, 43), P("Q2", "2")])
track("VBAT", [P("R5", "1"), (61, 23)], width=POWER_W)
track("VBAT_SW", [P("Q1", "2"), (72.54, 29.5), (86.54, 29.5), P("U2", "2")], width=POWER_W)
track("VBAT_SW", [P("C3", "2"), (79, 29.5)], width=POWER_W)
track("SENSOR_EN", [P("A1", "11"), (52, 52), P("R3", "1")])
track("Q2_G", [P("R3", "2"), (69, 48), P("Q2", "1")])
track("Q2_G", [P("R4", "1"), (72, 48), (69, 48)])
# 5V rail to C4/C5/J2
track("5V_SENS", [P("U2", "1"), (84, 24), (91, 24), (91, 36), (89, 36), P("C4", "1")], width=POWER_W)
track("5V_SENS", [P("C5", "1"), (89, 30), (89, 36)], width=POWER_W)
track("5V_SENS", [P("C4", "1"), (89, 36), (89, 47), P("J2", "1")], width=POWER_W)
# battery monitor divider
track("ADC_NODE", [P("R1", "2"), P("R2", "1")])
track("ADC_NODE", [P("R1", "2"), (23, 50.16), (23, 42), (30.38, 42), P("A1", "1")])
track("MEAS_LO", [P("R2", "2"), (25, 69), (28, 74), (32.29, 73.5), P("Q3", "2")], layer=pcbnew.B_Cu)
track("VBAT_MEAS_EN", [P("A1", "4"), (26.8, 52), (26.8, 70), (30, 70), P("Q3", "1")])
track("VBAT_MEAS_EN", [P("R6", "1"), (26.8, 66)])
# UART
track("UART_TX_MCU", [P("A1", "7"), (30.38, 64), P("R7", "1")])
track("TFMINI_RX", [P("R7", "2"), (90, 64), (90, 62), P("J2", "4")])
track("UART_RX", [P("A1", "8"), (48.5, 59.62), (48.5, 57), P("J2", "3")])
# I2C spare header (B.Cu diagonals under the GND pour)
track("I2C_SDA", [P("A1", "5"), (33, 54.54), (40, 58.35), (48, 58.35), (52, 59), (60, 63), (60, 68), (62.54, 71), P("J3", "2")], layer=pcbnew.B_Cu)
track("I2C_SCL", [P("A1", "6"), (33, 58.5), (40, 64), (44, 68), (58, 72.5), P("J3", "1")], layer=pcbnew.B_Cu)
track("3V3_SYS", [P("A1", "12"), (50, 50), (67.62, 60), P("J3", "4")], layer=pcbnew.B_Cu)

# ---------------- board outline ----------------
X0, Y0, X1, Y1 = 20, 16, 100, 78
for a, b in [((X0, Y0), (X1, Y0)), ((X1, Y0), (X1, Y1)),
             ((X1, Y1), (X0, Y1)), ((X0, Y1), (X0, Y0))]:
    seg = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_SEGMENT)
    seg.SetStart(mm(*a))
    seg.SetEnd(mm(*b))
    seg.SetLayer(pcbnew.Edge_Cuts)
    seg.SetWidth(FromMM(0.1))
    board.Add(seg)

# ---------------- GND zone on B.Cu ----------------
zone = pcbnew.ZONE(board)
zone.SetLayer(pcbnew.B_Cu)
zone.SetNet(nets["GND"])
outline = zone.Outline()
outline.NewOutline()
for x, y in [(X0+0.6, Y0+0.6), (X1-0.6, Y0+0.6), (X1-0.6, Y1-0.6), (X0+0.6, Y1-0.6)]:
    outline.Append(FromMM(x), FromMM(y))
zone.SetLocalClearance(FromMM(0.3))
zone.SetMinThickness(FromMM(0.3))
zone.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
zone.SetThermalReliefGap(FromMM(0.5))
zone.SetThermalReliefSpokeWidth(FromMM(0.8))
board.Add(zone)

# ---------------- silkscreen labels ----------------
def silk(text, x, y, size=1.2, bold=False):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(mm(x, y))
    t.SetLayer(pcbnew.F_SilkS)
    t.SetTextSize(VECTOR2I(FromMM(size), FromMM(size)))
    t.SetTextThickness(FromMM(size * (0.2 if bold else 0.15)))
    board.Add(t)

silk("SnowGauge v1.0", 60, 18.5, 1.8, True)
silk("BAT+", 29, 36.2)
silk("BAT-", 34, 36.2)
silk("NO USB WHILE BATTERY CONNECTED!", 47, 76.6, 1.3, True)
silk("TFmini", 84.5, 44)
silk("RED", 88.5, 47, 1.0)
silk("BLK", 89.5, 52, 1.0)
silk("GRN", 89.5, 57, 1.0)
silk("WHT", 89.5, 62, 1.0)
silk("SCL SDA GND 3V3", 63.8, 70.8, 1.0)
silk("U2: F50=TFmini / F33=TSD20", 84, 21, 1.0)
silk("USB", 38, 42.8, 1.0)

pcbnew.ZONE_FILLER(board).Fill(board.Zones())
board.Save(OUT)
print("saved", OUT)

# report unconnected
conn = board.GetConnectivity()
print("unconnected items:", conn.GetUnconnectedCount(True))
