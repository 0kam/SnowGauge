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
           "BATTERY L91x4", (31, 59), 0, {"1": "BATP_RAW", "2": "GND"}),
    "D1": (("Diode_THT", "D_DO-41_SOD81_P10.16mm_Horizontal"),
           "1N5819", (37.5, 30.3), 180, {"1": "VBAT", "2": "BATP_RAW"}),
    "U1": (("Package_TO_SOT_THT", "TO-220-3_Vertical"),
           "NJU7223F33", (27.8, 27), 0, {"1": "3V3_SYS", "2": "VBAT", "3": "GND"}),
    "Q1": (("Package_TO_SOT_THT", "TO-220-3_Vertical"),
           "2SJ334", (56, 25), 0, {"1": "Q1_GATE", "2": "VBAT_SW", "3": "VBAT"}),
    "U2": (("Package_TO_SOT_THT", "TO-220-3_Vertical"),
           "NJU7223F50", (73, 25), 180, {"1": "5V_SENS", "2": "VBAT_SW", "3": "GND"}),
    "Q2": (("Package_TO_SOT_THT", "TO-251-3_Vertical"),
           "2SK4017", (60.58, 50), 180, {"1": "Q2_G", "2": "Q1_GATE", "3": "GND"}),
    "R1": (("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal"),
           "1M", (29.5, 33), 270, {"1": "VBAT_SW", "2": "ADC_NODE"}),
    "R2": (("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal"),
           "1M", (29.5, 45), 270, {"1": "ADC_NODE", "2": "GND"}),
    "R3": (("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal"),
           "10k", (53, 44), 0, {"1": "SENSOR_EN", "2": "Q2_G"}),
    "R4": (("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal"),
           "100k", (63.6, 43.5), 270, {"1": "Q2_G", "2": "GND"}),
    "R5": (("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal"),
           "100k", (53.5, 28), 270, {"1": "VBAT", "2": "Q1_GATE"}),
    "R7": (("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal"),
           "1k", (58, 54.5), 0, {"1": "UART_TX_MCU", "2": "TFMINI_RX"}),
    "C1": (("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P5.00mm"),
           "0.1u", (23.3, 28), 270, {"1": "VBAT", "2": "GND"}),
    "C2": (("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P5.00mm"),
           "0.1u", (53, 40), 0, {"1": "3V3_SYS", "2": "GND"}),
    "C3": (("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P5.00mm"),
           "0.1u", (62, 33), 0, {"1": "VBAT_SW", "2": "GND"}),
    "C5": (("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P5.00mm"),
           "0.1u", (77.5, 26), 270, {"1": "GND", "2": "5V_SENS"}),
    "C6": (("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P5.00mm"),
           "10u", (64, 59), 0, {"1": "3V3_SYS", "2": "GND"}),
    "C4": (("Capacitor_THT", "CP_Radial_D10.0mm_P5.00mm"),
           "OS-CON 470u/16V", (71, 40), 270, {"1": "5V_SENS", "2": "GND"}),
    "J2": (("TerminalBlock", "TerminalBlock_MaiXu_MX126-5.0-04P_1x04_P5.00mm"),
           "TFmini R/B/G/W", (81, 36), 270, {"1": "5V_SENS", "2": "GND", "3": "UART_RX", "4": "TFMINI_RX"}),
    "J3": (("Connector_PinHeader_2.54mm", "PinHeader_1x04_P2.54mm_Vertical"),
           "I2C SPARE", (52, 62), 90, {"1": "I2C_SCL", "2": "I2C_SDA", "3": "GND", "4": "3V3_SYS"}),
    "A1": ("XIAO", "XIAO nRF52840 Sense", (42, 44), 90,
           {"1": "ADC_NODE", "5": "I2C_SDA", "6": "I2C_SCL",
            "7": "UART_TX_MCU", "8": "UART_RX", "11": "SENSOR_EN",
            "12": "3V3_SYS", "13": "GND"}),
    "H1": (("MountingHole", "MountingHole_3.2mm_M3"), "M3", (23.5, 19.5), 0, {}),
    "H2": (("MountingHole", "MountingHole_3.2mm_M3"), "M3", (80.5, 19.5), 0, {}),
    "H3": (("MountingHole", "MountingHole_3.2mm_M3"), "M3", (23.5, 62.5), 0, {}),
    "H4": (("MountingHole", "MountingHole_3.2mm_M3"), "M3", (80.5, 62.5), 0, {}),
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
assert (abs(p1[0] - 34.38) < 0.05 and abs(p1[1] - 36.38) < 0.05
        and abs(p7[1] - 51.62) < 0.05 and abs(p14[0] - 49.62) < 0.05
        and abs(p8[0] - 49.62) < 0.05), \
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
# battery feed on B.Cu (bottom-left is crowded on top)
track("BATP_RAW", [P("J1", "1"), (27.6, 57.5), (27.6, 30), P("D1", "2")], layer=pcbnew.B_Cu, width=0.6)
# VBAT rail along y=22
track("VBAT", [(25, 22), (61.08, 22)], width=POWER_W)
track("VBAT", [P("D1", "1"), (37.5, 22)], width=POWER_W)
track("VBAT", [P("U1", "2"), (30.34, 22)], width=POWER_W)
track("VBAT", [P("Q1", "3"), (61.08, 22)], width=POWER_W)
track("VBAT", [P("C1", "1"), (23.3, 26.5), (25, 26.5), (25, 22)], width=POWER_W)

track("VBAT", [P("R5", "1"), (53.5, 22)], width=POWER_W)
# 3V3: U1 OUT -> x=46 vertical -> pad12; C6 taps the vertical, C2 taps near pad12
track("3V3_SYS", [P("U1", "1"), (27.8, 28.6), (33, 28.6), (33, 33.1), (46, 33.1), (46, 41.46), P("A1", "12")])
track("3V3_SYS", [P("C6", "1"), (59.62, 59), (59.62, 62)])
track("3V3_SYS", [P("C2", "1"), (53, 41.46), (49.62, 41.46)])
# high-side switch chain
track("Q1_GATE", [P("Q1", "1"), (56, 35.62), (56, 42), (58.29, 42), P("Q2", "2")])
track("Q1_GATE", [P("R5", "2"), (56, 35.62)])
track("VBAT_SW", [P("Q1", "2"), (58.54, 28.5), (70.46, 28.5), P("U2", "2")], width=POWER_W)
track("VBAT_SW", [P("C3", "1"), (62, 28.5)], width=POWER_W)
track("SENSOR_EN", [P("A1", "11"), P("R3", "1")])
track("Q2_G", [P("R3", "2"), (60.62, 46.5), (60.58, 46.5), P("Q2", "1")])
track("Q2_G", [P("R4", "1"), (63.6, 42), (60.62, 42), P("R3", "2")])
# 5V: U2 OUT (flipped U2) -> right side -> C4 / C5 / J2
track("5V_SENS", [P("U2", "1"), (73, 31), (77, 31), (77, 40), P("C4", "1")], width=POWER_W)
track("5V_SENS", [P("C5", "2"), (77, 31)], width=POWER_W)
track("5V_SENS", [(77, 36), P("J2", "1")], width=POWER_W)
# battery monitor divider (left edge)
track("VBAT_SW", [P("C3", "1"), (58, 33.6), (31, 33.6), P("R1", "1")], layer=pcbnew.B_Cu)
track("ADC_NODE", [P("R1", "2"), P("R2", "1")])
track("ADC_NODE", [P("R1", "2"), (27.8, 40.62), (27.8, 35), (34.38, 35), P("A1", "1")])
# UART
track("UART_TX_MCU", [P("A1", "7"), (34.38, 53.5), (56.5, 53.5), (56.5, 54.5), P("R7", "1")])
track("TFMINI_RX", [P("R7", "2"), (78, 54.5), (78, 51), P("J2", "4")])
track("UART_RX", [P("A1", "8"), (61.5, 51.62), (61.5, 52.8), (74, 52.8), (74, 46.8), (79, 46.8), (79, 46), P("J2", "3")])
# I2C spare header (B.Cu under the GND pour)
track("I2C_SCL", [P("A1", "6"), (37, 50.5), (43, 57), (46, 59.2), (52, 61), P("J3", "1")], layer=pcbnew.B_Cu)
track("I2C_SDA", [P("A1", "5"), (41, 49), (47.5, 53.4), (51, 55.5), (54.54, 60), P("J3", "2")], layer=pcbnew.B_Cu)
track("3V3_SYS", [P("A1", "12"), (51.2, 43), (51.2, 52.4), (60.5, 52.4), (60.5, 56.5), (59.62, 58), P("J3", "4")], layer=pcbnew.B_Cu)

# ---------------- board outline ----------------
X0, Y0, X1, Y1 = 20, 16, 84, 66
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

silk("SnowGauge v1.2", 50, 18.3, 1.6, True)
silk("BAT+", 31, 56.2, 1.0)
silk("BAT-", 36, 56.2, 1.0)
silk("NO USB WHILE BATTERY CONNECTED!", 50, 64.4, 1.2, True)
silk("TFmini J2: RED BLK GRN WHT", 68, 35.9, 0.9)
silk("SCL SDA GND 3V3", 55.8, 59.7, 0.9)
silk("U2: F50=TFmini / F33=TSD20", 64, 29.4, 0.85)
silk("USB", 42, 35.2, 1.0)

pcbnew.ZONE_FILLER(board).Fill(board.Zones())
board.Save(OUT)
print("saved", OUT)

# report unconnected
conn = board.GetConnectivity()
print("unconnected items:", conn.GetUnconnectedCount(True))
