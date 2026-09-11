#!/usr/bin/env python3
"""Generate docs/assets/figures/coordinate_system.png.

Renders the PCB, turns it into the mounting orientation used in the
enclosure (USB to the left, sensor terminal J2 up = the top view rotated
90 deg counter-clockwise) and draws the IMU axes on it.  Re-run after any
change to pcb/SnowGauge.kicad_pcb or to the mounting orientation:

    python3 docs/assets/figures/gen_coordinate_system.py

Needs KiCad 10 (kicad-cli) and Pillow.  Definitions: docs/coordinate_system.md.
"""
import os
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
PCB = os.path.join(ROOT, "pcb", "SnowGauge.kicad_pcb")
OUT = os.path.join(HERE, "coordinate_system.png")
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
FONT = "/System/Library/Fonts/ヒラギノ角ゴシック W%d.ttc"

RENDER = ["--side", "top", "--zoom", "0.85", "--width", "1600", "--height", "1250",
          "--quality", "high"]
# Board mm -> render px and the board crop, as in gen_component_orientation.py.
PX_PER_MM = 16.06
ORIGIN_PX = (327.2 - 23.5 * 16.11, 271.6 - 19.5 * 16.01)
CROP = (250, 196, 1355, 1040)
BOARD_W = 560                 # width of the board image in the figure (px)
IMU_MM = (42.0, 44.0)         # XIAO module centre; the LSM6DS3TR-C sits on it

W = 1080                      # canvas width
BG = (255, 255, 255)
INK = (20, 20, 20)
GREY = (110, 118, 128)
BLUE = (20, 90, 200)          # tilt / optical axis
RED = (200, 40, 40)           # pitch
GREEN = (20, 130, 70)         # roll


def font(size, weight=6):
    return ImageFont.truetype(FONT % weight, size)


def board_image():
    """PCB in the mounting orientation: top view rotated 90 deg CCW."""
    tmp = os.path.join(tempfile.mkdtemp(), "top.png")
    subprocess.run([KICAD_CLI, "pcb", "render", "-o", tmp] + RENDER + [PCB], check=True)
    crop = Image.open(tmp).convert("RGB").crop(CROP)
    rot = crop.transpose(Image.ROTATE_90)
    scale = BOARD_W / rot.width
    return rot.resize((BOARD_W, round(rot.height * scale)), Image.LANCZOS), crop.size


def main():
    board, (cw, _ch) = board_image()
    scale = BOARD_W / board.width * 1.0

    def mm2fig(x_mm, y_mm, box):
        """Board mm -> figure px (through the crop, the CCW rotation, the scale)."""
        px = ORIGIN_PX[0] + x_mm * PX_PER_MM - CROP[0]
        py = ORIGIN_PX[1] + y_mm * PX_PER_MM - CROP[1]
        rx, ry = py, (cw - 1) - px          # ROTATE_90 (counter-clockwise)
        s = BOARD_W / (CROP[3] - CROP[1])   # rotated width / source height
        return box[0] + rx * s, box[1] + ry * s

    title_h, top_label_h = 104, 44
    board_y = title_h + top_label_h
    bottom_label_h, legend_h = 52, 210
    H = board_y + board.height + bottom_label_h + legend_h
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

    bx = (W - BOARD_W) // 2
    box = (bx, board_y, bx + BOARD_W, board_y + board.height)
    img.paste(board, (bx, board_y))
    d.rectangle([bx - 3, board_y - 3, box[2] + 2, box[3] + 2], outline=INK, width=3)

    def centered(text, y, f, fill=INK):
        d.text(((W - d.textlength(text, font=f)) / 2, y), text, font=f, fill=fill)

    centered("基板の取付姿勢と IMU 座標系（部品面から見た図）", 20, font(30))
    centered("筐体内で基板は鉛直。USB コネクタ左、センサ端子 J2 上、"
             "電池端子 J1 右、D0〜D6（TX）ピン列下", 64, font(20, 3))
    centered("上: センサ端子 J2（+Y 方向）", board_y - 38, font(22, 3))
    centered("下: D0〜D6 / TX ピン列。レーザーはこの方向（−Y = 光軸）",
             box[3] + 14, font(22, 6), BLUE)

    # Side labels, vertically centred on the board.
    mid = (board_y + box[3]) / 2
    for text, sub, x_right in (("USB", "（左）", False), ("電池 J1", "（右）", True)):
        f, fs = font(24, 3), font(20, 3)
        wmax = max(d.textlength(text, font=f), d.textlength(sub, font=fs))
        x = box[2] + 26 if x_right else bx - 26 - wmax
        d.text((x + (wmax - d.textlength(text, font=f)) / 2, mid - 26), text, font=f, fill=INK)
        d.text((x + (wmax - d.textlength(sub, font=fs)) / 2, mid + 4), sub, font=fs, fill=GREY)

    # ---- axes at the IMU ----
    ox, oy = mm2fig(*IMU_MM, box)
    arm = 120

    def arrow(dx, dy, color, width=7, head=17):
        x1, y1 = ox + dx, oy + dy
        d.line([(ox, oy), (x1, y1)], fill=color, width=width)
        if dx:
            s = 1 if dx > 0 else -1
            d.polygon([(x1 + s * head, y1), (x1, y1 - head + 3), (x1, y1 + head - 3)], fill=color)
        else:
            s = 1 if dy > 0 else -1
            d.polygon([(x1, y1 + s * head), (x1 - head + 3, y1), (x1 + head - 3, y1)], fill=color)

    def label(text, x, y, color, size=26, anchor="lt"):
        f = font(size)
        w, h = d.textlength(text, font=f), size + 8
        if "r" in anchor:
            x -= w
        if "m" in anchor:
            y -= h / 2
        d.rectangle([x - 7, y - 4, x + w + 7, y + h], fill=BG, outline=color)
        d.text((x, y - 2), text, font=f, fill=color)

    arrow(0, -arm * 0.55, GREY, width=5, head=13)          # +Y (up, not the axis)
    label("+Y", ox + 12, oy - arm * 0.55 - 6, GREY, 20)
    arrow(arm, 0, RED)                                     # +X to the right
    label("+X", ox + arm + 26, oy, RED, anchor="lm")
    arrow(0, arm, BLUE)                                    # -Y = optical axis, down
    label("−Y（光軸）", ox + 16, oy + arm - 10, BLUE)
    d.ellipse([ox - 13, oy - 13, ox + 13, oy + 13], outline=GREEN, width=5)
    d.ellipse([ox - 4, oy - 4, ox + 4, oy + 4], fill=GREEN)
    label("+Z（手前）", ox - 26, oy - 30, GREEN, anchor="rt")
    label("IMU は XIAO 上", ox - 26, oy + 22, GREY, 19, anchor="rt")

    # ---- legend ----
    rows = [
        ("傾斜 tilt", BLUE, "重力と −Y のなす角 = レーザーの真下からのずれ"
                            "（取付姿勢で 0°、平置きで約 90°）"),
        ("pitch", RED, "ずれの +X 成分（面内）。光軸が右 = 電池端子側へ振れると +"),
        ("roll", GREEN, "ずれの +Z 成分（面外）。光軸が手前 = 部品面側へ振れると +"),
        ("実測", INK, "加速度計の読み: この姿勢 (0, +1, 0) g / 平置き部品面上 (0, 0, +1) g"
                      " / USB 真下 (+1, 0, 0) g"),
    ]
    y = box[3] + bottom_label_h + 12
    for key, color, text in rows:
        d.text((36, y), key, font=font(22), fill=color)
        d.text((186, y), text, font=font(20, 3), fill=INK)
        y += 38
    d.text((36, y + 6), "図: docs/assets/figures/gen_coordinate_system.py で生成"
                        "（kicad-cli pcb render + 注記）。手で編集しない。",
           font=font(17, 3), fill=GREY)

    img.save(OUT)
    print("saved", OUT, img.size)


if __name__ == "__main__":
    main()
