#!/usr/bin/env python3
"""Generate docs/assets/figures/component_orientation.png.

Renders the PCB top view with kicad-cli and adds the arrows/labels that show
which way the marked (printed) face of U1/U2/Q1/Q2 points.  Re-run after any
change to pcb/SnowGauge.kicad_pcb:

    python3 docs/assets/figures/gen_component_orientation.py

Needs KiCad 10 (kicad-cli) and Pillow.
"""
import os
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
PCB = os.path.join(ROOT, "pcb", "SnowGauge.kicad_pcb")
OUT = os.path.join(HERE, "component_orientation.png")
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
FONT = "/System/Library/Fonts/ヒラギノ角ゴシック W%d.ttc"

RENDER = ["--side", "top", "--zoom", "0.85", "--width", "1600", "--height", "1250",
          "--quality", "high"]
# Board mm -> render px, measured on the M3 mounting holes of that render.
PX_PER_MM = 16.06
ORIGIN_PX = (327.2 - 23.5 * 16.11, 271.6 - 19.5 * 16.01)  # board (0,0) in px
CROP = (250, 196, 1355, 1040)
TOP_H, BOTTOM_H = 62, 132
PINK, WHITE = (255, 45, 120), (255, 255, 255)

# Parts whose marked face matters: (ref, board x/y of pad 1 row, footprint edge
# offsets in mm along y: tab edge, marked-face edge).  The tab is the narrow
# silk band; the reference designator is silkscreened on that side.
PARTS = [
    ("U1", 27.55, 27.0, -3.26, 1.36),
    ("Q1", 56.0, 25.0, -3.26, 1.36),
    ("U2", 73.0, 25.0, 3.26, -1.36),   # rotated 180 deg
    ("Q2", 60.58, 50.0, 1.27, -1.03),  # TO-251, rotated 180 deg
]


def mm2px(x, y):
    return (ORIGIN_PX[0] + x * PX_PER_MM, ORIGIN_PX[1] + y * PX_PER_MM)


def main():
    tmp = os.path.join(tempfile.mkdtemp(), "top.png")
    subprocess.run([KICAD_CLI, "pcb", "render", "-o", tmp] + RENDER + [PCB], check=True)
    src = Image.open(tmp).convert("RGB")

    crop = src.crop(CROP)
    w, h = crop.size
    img = Image.new("RGB", (w, TOP_H + h + BOTTOM_H), (24, 26, 30))
    img.paste(crop, (0, TOP_H))
    d = ImageDraw.Draw(img)

    def font(size, weight=6):
        return ImageFont.truetype(FONT % weight, size)

    def arrow(x_mm, y_mm, tab_off, face_off):
        """Arrow from the tab edge, through the marked face, pointing outward."""
        x = mm2px(x_mm, 0)[0] - CROP[0]
        y0 = mm2px(0, y_mm + tab_off)[1] - CROP[1] + TOP_H
        y1 = mm2px(0, y_mm + face_off)[1] - CROP[1] + TOP_H + (34 if face_off > 0 else -34)
        sign = 1 if y1 > y0 else -1
        head_len, head_w = 26, 15
        for color, width, grow in ((WHITE, 16, 10), (PINK, 8, 0)):
            d.line([(x, y0), (x, y1 - sign * (head_len - 4))], fill=color, width=width)
            d.polygon([(x, y1 + sign * grow),
                       (x - head_w - grow, y1 - sign * head_len),
                       (x + head_w + grow, y1 - sign * head_len)], fill=color)

    for _ref, x_mm, y_mm, tab_off, face_off in PARTS:
        arrow(x_mm, y_mm, tab_off, face_off)

    d.text((14, 16), "PCB v1.2 レギュレータと MOSFET の向き"
                     "（部品面から見た図）",
           font=font(30), fill=WHITE)

    lines = [
        ("ピンクの矢印 = 型番の印字面（黒い面）"
         "が向く向き。U1 / Q1 は下側（XIAO 側）、"
         "U2 / Q2 は上側。", 22, (255, 190, 210)),
        ("矢印と反対側の銀色の帯 = 金属タブ"
         "（背面）。シルクの記号 U1 / Q1 / U2 / Q2 は"
         "タブ側に印刷されている。", 22, (200, 210, 220)),
        ("図: docs/assets/figures/gen_component_orientation.py で生成"
         "（kicad-cli pcb render + 注記）。手で編集しない。",
         19, (140, 150, 160)),
    ]
    y = TOP_H + h + 12
    for text, size, color in lines:
        f = font(size, 3)
        while d.textlength(text, font=f) > w - 28 and size > 14:
            size -= 1
            f = font(size, 3)
        d.text((14, y), text, font=f, fill=color)
        y += size + 16

    img.save(OUT)
    print("saved", OUT)


if __name__ == "__main__":
    main()
