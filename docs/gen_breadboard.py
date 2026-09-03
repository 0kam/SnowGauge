#!/usr/bin/env python3
"""Generate docs/breadboard_guide.html (SnowGauge breadboard wiring guide).

Single source of truth for the breadboard netlist is PARTS below. Every
two-terminal part and every jumper lists its exact holes; the SVG draws
each end at that hole and prints the hole name next to it. One overview
figure plus one figure per assembly step (current step in colour, earlier
steps greyed out) are produced.

Run: python3 docs/gen_breadboard.py
"""
import html as _html

# ------------------------------------------------------------------ geometry
PITCH = 24
X0 = 70                    # x of column 1
COLS = 63
ROW_Y = {'T+': 34, 'T-': 54,
         'a': 108, 'b': 128, 'c': 148, 'd': 168, 'e': 188,
         'f': 244, 'g': 264, 'h': 284, 'i': 304, 'j': 324,
         'B-': 378, 'B+': 398}
W = X0 + COLS * PITCH + 50
H = 470

COLORS = {'vbat': '#D62828', 'gnd': '#3A4148', '33': '#E07A00', '5v': '#C2185B',
          'vsw': '#8E2F3C', 'en': '#2E7D32', 'gate': '#7A5230', 'adc': '#6A4CA5',
          'tx': '#1565C0', 'rx': '#5BA8DF', 'part': '#22303A'}
RAIL_NAMES = {'T+': '上の赤レール(VBAT)', 'T-': '上の青レール(GND)',
              'B-': '下の青レール(GND)', 'B+': '下の赤レール(未使用)'}


def hole_xy(h):
    """'c10' -> (x, y); 'T-@3' -> rail T- at column 3."""
    if '@' in h:
        rail, col = h.split('@')
        return X0 + (int(col) - 1) * PITCH, ROW_Y[rail]
    return X0 + (int(h[1:]) - 1) * PITCH, ROW_Y[h[0]]


def hole_name(h):
    if '@' in h:
        rail, col = h.split('@')
        return RAIL_NAMES[rail]
    return h


# ------------------------------------------------------------------ netlist
# kind: two  = two-terminal part (resistor/cap/diode), a/b = holes
#       ic3  = three-pin part, holes = [pin1, pin2, pin3], pins = names
#       jmp  = jumper wire a -> b (b may be a rail 'T-@col')
#       ext  = off-board wire (battery / TFmini) arriving at hole a
#       xiao = the XIAO module
PARTS = [
    # ---- STEP 1: power ----
    dict(n=1, step=1, kind='ext', name='電池ボックス 赤線(+)', a='c2', color='vbat',
         label='電池 赤(+)'),
    dict(n=2, step=1, kind='two', name='D1 1N5819', a='b2', b='b6', shape='diode', lab='above',
         note='帯(カソード)を b6 側に'),
    dict(n=3, step=1, kind='jmp', name='ジャンパ 赤', a='a6', b='T+@6', color='vbat'),
    dict(n=4, step=1, kind='ic3', name='U1 NJU7223F33', holes=['c10', 'c11', 'c12'],
         pins=['OUT', 'IN', 'GND'], lab='left', note='印字面を手前にして左から OUT / IN / GND'),
    dict(n=5, step=1, kind='jmp', name='ジャンパ 赤', a='a11', b='T+@11', color='vbat'),
    dict(n=6, step=1, kind='ext', name='電池ボックス 黒線(−)', a='T-@7', color='gnd',
         label='電池 黒(−)'),
    dict(n=7, step=1, kind='jmp', name='ジャンパ 黒', a='a12', b='T-@12', color='gnd'),
    dict(n=8, step=1, kind='two', name='C1 0.1µF', a='e11', b='e12', shape='cap', sub='C1', lab='below'),
    dict(n=8, step=1, kind='two', name='C2 0.1µF', a='d10', b='d12', shape='cap', sub='C2', lab='left'),
    dict(n=8, step=1, kind='two', name='C6 10µF', a='b10', b='b12', shape='cap', sub='C6', lab='right',
         note='向きなし（旧版の c10↔c12 は U1 の足と同じ穴なので修正）'),
    # ---- STEP 2: XIAO ----
    dict(n=9, step=2, kind='xiao', name='XIAO nRF52840 Sense', col0=20),
    dict(n=10, step=2, kind='jmp', name='ジャンパ 橙 (3.3V)', a='a10', b='g22', color='33', lb=15,
         note='U1 の OUT 列 → XIAO 3V3 ピンの列'),
    dict(n=11, step=2, kind='jmp', name='ジャンパ 黒', a='g21', b='B-@21', color='gnd', la=15,
         note='XIAO GND ピンの列 → 下の青レール'),
    dict(n=12, step=2, kind='jmp', name='ジャンパ 黒 (レール橋渡し)', a='T-@62', b='B-@62',
         color='gnd', note='上の青レール ↔ 下の青レール'),
    # ---- STEP 3: switch + 5 V ----
    dict(n=13, step=3, kind='ic3', name='Q2 2SK4017', holes=['h28', 'h29', 'h30'],
         pins=['G', 'D', 'S'], lab='right', note='印字面を手前にして左から G / D / S'),
    dict(n=14, step=3, kind='two', name='R3 10kΩ', a='j23', b='j28', shape='res', lab='above',
         note='XIAO D10 の列 ↔ Q2 の G 列'),
    dict(n=15, step=3, kind='two', name='R4 100kΩ', a='i28', b='B-@28', shape='res', lab='left',
         note='Q2 の G 列 → 下の青レール（プルダウン）'),
    dict(n=16, step=3, kind='jmp', name='ジャンパ 黒', a='g30', b='B-@30', color='gnd', la=-9,
         note='Q2 の S 列 → 下の青レール'),
    dict(n=17, step=3, kind='ic3', name='Q1 2SJ334', holes=['c32', 'c33', 'c34'],
         pins=['G', 'D', 'S'], lab='left', note='印字面を手前にして左から G / D / S'),
    dict(n=18, step=3, kind='two', name='R5 100kΩ', a='b32', b='b34', shape='res', lab='left',
         note='Q1 の G–S 間（プルアップ）'),
    dict(n=19, step=3, kind='jmp', name='ジャンパ (Q1ゲート)', a='g29', b='a32', color='gate', la=-9,
         note='Q2 の D 列 → Q1 の G 列'),
    dict(n=20, step=3, kind='jmp', name='ジャンパ 赤', a='a34', b='T+@34', color='vbat',
         note='Q1 の S 列 → 上の赤レール'),
    dict(n=21, step=3, kind='ic3', name='U2 NJU7223F50', holes=['c38', 'c39', 'c40'],
         pins=['OUT', 'IN', 'GND'], lab='above', note='TSD20 版はここを F33 に'),
    dict(n=22, step=3, kind='jmp', name='ジャンパ (VBAT_SW)', a='a33', b='a39', color='vsw',
         note='Q1 の D 列 → U2 の IN 列'),
    dict(n=23, step=3, kind='jmp', name='ジャンパ 黒', a='a40', b='T-@40', color='gnd',
         note='U2 の GND 列 → 上の青レール'),
    dict(n=24, step=3, kind='two', name='C3 0.1µF', a='e39', b='e40', shape='cap', lab='below'),
    dict(n=25, step=3, kind='two', name='C4 OS-CON 470µF', a='b42', b='b44', shape='ecap', lab='right',
         sub='C4', note='極性注意: 長い足(+) を b42、− を b44'),
    dict(n=25, step=3, kind='two', name='C5 0.1µF', a='c42', b='c44', shape='cap', sub='C5', lab='below'),
    dict(n=26, step=3, kind='jmp', name='ジャンパ (5V)', a='a38', b='a42', color='5v',
         note='U2 の OUT 列 → 5V ノード'),
    dict(n=27, step=3, kind='jmp', name='ジャンパ 黒', a='a44', b='T-@44', color='gnd',
         note='C4 の − 列 → 上の青レール'),
    # ---- STEP 4: battery divider ----
    dict(n=28, step=4, kind='two', name='R1 1MΩ', a='e33', b='e36', shape='res', lab='below',
         note='Q1 の D 列(VBAT_SW) ↔ 36 列（旧版の c33 は Q1 の足と同じ穴なので修正）'),
    dict(n=29, step=4, kind='two', name='R2 1MΩ', a='b36', b='b41', shape='res', lab='above'),
    dict(n=30, step=4, kind='jmp', name='ジャンパ 黒', a='a41', b='T-@41', color='gnd'),
    dict(n=31, step=4, kind='jmp', name='ジャンパ 紫 (電圧測定)', a='a36', b='a20', color='adc',
         note='分圧の中点 → XIAO A0(D0) の列'),
    # ---- STEP 5: TFmini ----
    dict(n=32, step=5, kind='two', name='R7 1kΩ', a='a26', b='a31', shape='res', lab='above',
         note='XIAO D6(TX) の列 ↔ 31 列'),
    dict(n=33, step=5, kind='ext', name='TFmini 白 (RX)', a='c31', color='tx',
         label='TFmini 白(RX)', anchor='end'),
    dict(n=34, step=5, kind='ext', name='TFmini 緑 (TX)', a='h26', color='rx',
         label='TFmini 緑(TX)', note='XIAO D7(RX) の列'),
    dict(n=35, step=5, kind='ext', name='TFmini 赤 (+5V)', a='e42', color='5v',
         label='TFmini 赤(+5V)', note='5V ノード'),
    dict(n=36, step=5, kind='ext', name='TFmini 黒 (GND)', a='T-@50', color='gnd',
         label='TFmini 黒(GND)'),
]

XIAO_TOP = ['D0/A0', 'D1', 'D2', 'D3', 'D4', 'D5', 'D6']       # row e, cols 20..26
XIAO_BOT = ['5V', 'GND', '3V3', 'D10', 'D9', 'D8', 'D7']       # row f, cols 20..26

STEPS = {
    1: ('電源部', ['赤レール−青レール間: <span class="kbd">電池電圧 −0.2〜0.35V</span>',
                  'c10（U1 の OUT）−青レール間: <span class="kbd">3.25〜3.35V</span>']),
    2: ('XIAO', ['XIAO の 3V3 ピン(f22)−GND ピン(f21) 間 = <span class="kbd">3.3V</span>（SnowGauge FW は LED を使わないので LED は点きません）']),
    3: ('スイッチと 5V 系', ['なにもしない時: e42−GND 間 = <span class="kbd">0V</span>',
                       'ジャンパ線で j28（Q2 の G）を 3.3V（c10）に触れさせる: e42 = <span class="kbd">4.9〜5.1V</span>、離すと 0V に戻る',
                       '同じくその時 c33（Q1 の D = VBAT_SW）= <span class="kbd">電池電圧</span>、a32（Q1 の G）= <span class="kbd">0V 近く</span>']),
    4: ('電池電圧の見張り（スイッチ済みレールから分圧）',
        ['スイッチ ON 中（上の確認と同時に）: a36−GND 間 = <span class="kbd">電池電圧のほぼ半分</span>、OFF 中は <span class="kbd">0V</span>']),
    5: ('TFmini Plus', ['テスト FW で: <span class="kbd">rail on</span> → <span class="kbd">tfmini raw 500</span> で距離フレーム受信、<span class="kbd">rail off</span> → e42 = 0V',
                        'µA 電流計を電池と直列に: スリープ <span class="kbd">90µA 以下</span>（ブレッドボードは接触・リークで数 µA 上振れすることあり）']),
}

CIRCLED = '①②③④⑤⑥⑦⑧⑨⑩⑪⑫⑬⑭⑮⑯⑰⑱⑲⑳㉑㉒㉓㉔㉕㉖㉗㉘㉙㉚㉛㉜㉝㉞㉟㊱'


def circ(n):
    return CIRCLED[n - 1]


# ------------------------------------------------------------------ SVG
def esc(s):
    return _html.escape(s, quote=True)


def board_base():
    o = []
    o.append(f'<rect x="{X0-40}" y="12" width="{W-30}" height="{H-24}" rx="10" fill="#F2EFE9" stroke="#C9C2B6"/>')
    # trench
    o.append(f'<rect x="{X0-30}" y="{ROW_Y["e"]+14}" width="{COLS*PITCH+40}" height="{ROW_Y["f"]-ROW_Y["e"]-28}" fill="#E3DED4"/>')
    # rail lines
    for r, c in (('T+', '#D62828'), ('T-', '#2B5FA8'), ('B-', '#2B5FA8'), ('B+', '#D62828')):
        y = ROW_Y[r]
        off = 9 if r in ('T+', 'B+') else -9
        o.append(f'<line x1="{X0-16}" y1="{y+off}" x2="{X0+COLS*PITCH-4}" y2="{y+off}" stroke="{c}" stroke-width="2"/>')
        o.append(f'<text x="{X0-22}" y="{y+4}" text-anchor="end" class="rowlab" fill="{c}" font-weight="700">{esc(RAIL_NAMES[r][:6])}</text>')
        o.append(f'<text x="{X0+COLS*PITCH+6}" y="{y+4}" class="rowlab" fill="{c}" font-weight="700">{esc(RAIL_NAMES[r][:6])}</text>')
    # holes
    for row, y in ROW_Y.items():
        for col in range(1, COLS + 1):
            x = X0 + (col - 1) * PITCH
            o.append(f'<circle cx="{x}" cy="{y}" r="3.2" fill="#B9B2A6"/>')
    # row letters
    for row in 'abcdefghij':
        y = ROW_Y[row]
        o.append(f'<text x="{X0-22}" y="{y+4}" text-anchor="end" class="rowlab">{row}</text>')
        o.append(f'<text x="{X0+COLS*PITCH+6}" y="{y+4}" class="rowlab">{row}</text>')
    # column numbers (every column, above row a and below row j)
    for col in range(1, COLS + 1):
        x = X0 + (col - 1) * PITCH
        big = col % 5 == 0
        for y in (ROW_Y['a'] - 24, ROW_Y['j'] + 27):
            o.append(f'<text x="{x}" y="{y}" text-anchor="middle" class="collab" '
                     f'font-weight="{700 if big else 400}" fill="{"#22303A" if big else "#8a939b"}">{col}</text>')
    return '\n'.join(o)


def badge(x, y, n, dim):
    op = 0.35 if dim else 1
    return (f'<g opacity="{op}"><circle cx="{x}" cy="{y}" r="9" fill="#33658A" stroke="#fff" stroke-width="1.5"/>'
            f'<text x="{x}" y="{y+3.5}" text-anchor="middle" fill="#fff" font-size="10" font-weight="700">{n}</text></g>')


def end_dot(x, y, name, color, dim, dy=-9, anchor='middle', dx=0):
    op = 0.35 if dim else 1
    return (f'<g opacity="{op}"><circle cx="{x}" cy="{y}" r="4.2" fill="{color}" stroke="#fff" stroke-width="1.2"/>'
            f'<text x="{x+dx}" y="{y+dy}" text-anchor="{anchor}" class="pin" '
            f'stroke="#fff" stroke-width="3" paint-order="stroke">{esc(name)}</text></g>')


def name_text(x, y, label, anchor='middle'):
    return (f'<text x="{x}" y="{y}" text-anchor="{anchor}" class="cname" '
            f'stroke="#fff" stroke-width="3" paint-order="stroke">{esc(label)}</text>')


def draw_two(p, dim):
    import math
    (x1, y1), (x2, y2) = hole_xy(p['a']), hole_xy(p['b'])
    col = COLORS['part']
    op = 0.3 if dim else 1
    o = [f'<g opacity="{op}">']
    o.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{col}" stroke-width="1.6"/>')
    mx, my = (x1 + x2) / 2, (y1 + y2) / 2
    ang = math.degrees(math.atan2(y2 - y1, x2 - x1))
    vertical = abs(ang) > 45
    L = math.hypot(x2 - x1, y2 - y1)
    shape = p.get('shape', 'res')
    bw = min(max(L - 26, 18), 50)
    tf = f'transform="translate({mx},{my}) rotate({ang})"'
    if shape == 'res':
        o.append(f'<rect x="{-bw/2}" y="-6" width="{bw}" height="12" rx="3" fill="#E8DCC8" stroke="{col}" stroke-width="1.3" {tf}/>')
    elif shape == 'diode':
        o.append(f'<rect x="{-bw/2}" y="-6" width="{bw}" height="12" rx="3" fill="#333" stroke="{col}" stroke-width="1.3" {tf}/>')
        o.append(f'<rect x="{bw/2-7}" y="-6" width="4" height="12" fill="#DDD" {tf}/>')
    elif shape == 'cap':
        o.append(f'<ellipse cx="{mx}" cy="{my}" rx="8" ry="6" fill="#E5A33C" stroke="{col}" stroke-width="1.2"/>')
    elif shape == 'ecap':
        o.append(f'<circle cx="{mx}" cy="{my}" r="12" fill="#5B4A8A" stroke="{col}" stroke-width="1.2"/>')
    lab = p.get('lab', 'above')
    label = p['name']
    if vertical and lab == 'left':
        o.append(name_text(x1 - 12, y1 + 4, label, 'end'))
        holes_above = None  # hole names go to the right of the holes
    elif vertical:
        o.append(name_text(x1 + 12, y1 + 4, label, 'start'))
        holes_above = None  # hole names go to the left of the holes
    elif lab == 'above':
        o.append(name_text(mx, my - 12, label)); holes_above = False
    elif lab == 'below':
        o.append(name_text(mx, my + 25, label)); holes_above = True
    elif lab == 'left':
        o.append(name_text(min(x1, x2) - 14, my + 4, label, 'end')); holes_above = True
    else:  # right
        o.append(name_text(max(x1, x2) + 14, my + 4, label, 'start')); holes_above = True
    o.append('</g>')
    for i, (x, y, h) in enumerate(((x1, y1, p['a']), (x2, y2, p['b']))):
        nm = hole_name(h) if '@' not in h else 'レール'
        if shape == 'ecap':
            nm += '(+)' if i == 0 else '(−)'
        if holes_above is None and lab == 'left':
            o.append(end_dot(x, y, nm, col, dim, dy=4, anchor='start', dx=8))
        elif holes_above is None:
            o.append(end_dot(x, y, nm, col, dim, dy=4, anchor='end', dx=-8))
        else:
            o.append(end_dot(x, y, nm, col, dim, dy=(-10 if holes_above else 14)))
    tw = 7.2 * len(label)
    if vertical and lab == 'left':
        o.append(badge(x1 - 12 - tw - 12, y1, p['n'], dim))
    elif vertical:
        o.append(badge(x1 + 12 + tw + 12, y1, p['n'], dim))
    elif lab == 'left':
        o.append(badge(min(x1, x2) - 14 - tw - 12, my, p['n'], dim))
    elif lab == 'right':
        o.append(badge(max(x1, x2) + 14 + tw + 12, my, p['n'], dim))
    else:
        o.append(badge_two(mx, my, lab, holes_above, p['n'], dim))
    return '\n'.join(o)


def badge_two(mx, my, lab, holes_above, n, dim):
    # put the number where nothing else of this part is drawn
    if lab == 'above':       # name above, holes below -> badge under the hole names
        return badge(mx, my + 28, n, dim)
    if lab == 'below':       # holes above, name below -> badge under the name
        return badge(mx, my + 40, n, dim)
    return badge(mx, my - 24, n, dim)  # left/right: holes above -> badge above the hole names


def draw_ic3(p, dim):
    pts = [hole_xy(h) for h in p['holes']]
    x1, y = pts[0]
    x3 = pts[2][0]
    col = COLORS['part']
    op = 0.3 if dim else 1
    lab = p.get('lab', 'above')
    o = [f'<g opacity="{op}">']
    o.append(f'<rect x="{x1-11}" y="{y-9}" width="{x3-x1+22}" height="18" rx="4" fill="#FFFFFF" fill-opacity="0.92" stroke="{col}" stroke-width="1.6"/>')
    for (x, yy), pin in zip(pts, p['pins']):
        o.append(f'<text x="{x}" y="{yy+3.5}" text-anchor="middle" class="pin" font-weight="700">{esc(pin)}</text>')
    label = p['name']
    if lab == 'above':
        o.append(name_text((x1 + x3) / 2, y - 22, label)); holes_above = False
    elif lab == 'below':
        o.append(name_text((x1 + x3) / 2, y + 27, label)); holes_above = True
    elif lab == 'left':
        o.append(name_text(x1 - 16, y + 4, label, 'end')); holes_above = (y > ROW_Y['e'])
    else:
        o.append(name_text(x3 + 16, y + 4, label, 'start')); holes_above = (y > ROW_Y['e'])
    o.append('</g>')
    for (x, yy), h in zip(pts, p['holes']):
        o.append(end_dot(x, yy + (-9 if holes_above else 9), h, col, dim, dy=(-4 if holes_above else 9)))
    if lab == 'above':
        bx, by = x3 + 20, y - 22
    elif lab == 'below':
        bx, by = x3 + 20, y + 24
    elif lab == 'left':
        bx, by = x1 - 16 - 8 * len(label) - 12, y
    else:
        bx, by = x3 + 16 + 8 * len(label) + 12, y
    o.append(badge(bx, by, p['n'], dim))
    return '\n'.join(o)


def draw_xiao(p, dim):
    c0 = p['col0']
    xa = X0 + (c0 - 1) * PITCH
    xb = X0 + (c0 + 5) * PITCH
    ye, yf = ROW_Y['e'], ROW_Y['f']
    op = 0.3 if dim else 1
    o = [f'<g opacity="{op}">']
    o.append(f'<rect x="{xa-14}" y="{ye-14}" width="{xb-xa+28}" height="{yf-ye+28}" rx="8" fill="#1F2933" stroke="#000"/>')
    o.append(f'<text x="{(xa+xb)/2}" y="{(ye+yf)/2-2}" text-anchor="middle" fill="#fff" font-size="12" font-weight="700">XIAO nRF52840 Sense</text>')
    o.append(f'<text x="{(xa+xb)/2}" y="{(ye+yf)/2+13}" text-anchor="middle" fill="#B9C4BF" font-size="9.5">USB ← 左向き</text>')
    for i, (t, b) in enumerate(zip(XIAO_TOP, XIAO_BOT)):
        x = xa + i * PITCH
        o.append(f'<circle cx="{x}" cy="{ye}" r="4" fill="#E9C46A"/><circle cx="{x}" cy="{yf}" r="4" fill="#E9C46A"/>')
        o.append(f'<text x="{x}" y="{ye-19}" text-anchor="middle" class="pin" font-weight="700" stroke="#fff" stroke-width="3" paint-order="stroke">{esc(t)}</text>')
        o.append(f'<text x="{x}" y="{ye-8}" text-anchor="middle" class="pin" fill="#8a939b" stroke="#fff" stroke-width="3" paint-order="stroke">e{c0+i}</text>')
        o.append(f'<text x="{x}" y="{yf+26}" text-anchor="middle" class="pin" font-weight="700" stroke="#fff" stroke-width="3" paint-order="stroke">{esc(b)}</text>')
        o.append(f'<text x="{x}" y="{yf+15}" text-anchor="middle" class="pin" fill="#8a939b" stroke="#fff" stroke-width="3" paint-order="stroke">f{c0+i}</text>')
    o.append('</g>')
    o.append(badge(xb + 26, (ye + yf) / 2, p['n'], dim))
    return '\n'.join(o)


def draw_jmp(p, dim):
    (x1, y1), (x2, y2) = hole_xy(p['a']), hole_xy(p['b'])
    col = COLORS[p['color']]
    op = 0.3 if dim else 1
    o = [f'<g opacity="{op}">']
    if x1 == x2 or y1 == y2:
        d = f'M{x1},{y1} L{x2},{y2}'
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
    else:
        # gentle arc so crossing wires stay distinguishable
        bulge = -28 if (y1 + y2) / 2 < ROW_Y['e'] else 28
        cx, cy = (x1 + x2) / 2, (y1 + y2) / 2 + bulge
        d = f'M{x1},{y1} Q{cx},{cy} {x2},{y2}'
        mx, my = (x1 + 2 * cx + x2) / 4, (y1 + 2 * cy + y2) / 4
    o.append(f'<path d="{d}" fill="none" stroke="#fff" stroke-width="6" stroke-linecap="round"/>')
    o.append(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="3" stroke-linecap="round"/>')
    o.append('</g>')
    for (x, y, h, key) in ((x1, y1, p['a'], 'la'), (x2, y2, p['b'], 'lb')):
        nm = h if '@' not in h else 'レール'
        dy = p.get(key, -9 if y < ROW_Y['e'] + 20 else 15)
        o.append(end_dot(x, y, nm, col, dim, dy=dy))
    o.append(badge(mx, my, p['n'], dim))
    return '\n'.join(o)


def draw_ext(p, dim):
    x, y = hole_xy(p['a'])
    col = COLORS[p['color']]
    op = 0.3 if dim else 1
    up = y < ROW_Y['e'] + 20
    ty = 14 if up else H - 14
    o = [f'<g opacity="{op}">']
    o.append(f'<path d="M{x},{y} L{x},{ty+ (6 if up else -6)}" fill="none" stroke="#fff" stroke-width="6" stroke-linecap="round"/>')
    o.append(f'<path d="M{x},{y} L{x},{ty+ (6 if up else -6)}" fill="none" stroke="{col}" stroke-width="3" stroke-linecap="round"/>')
    anc = p.get('anchor', 'start')
    lx = x - 7 if anc == 'end' else x + 7
    o.append(f'<text x="{lx}" y="{ty+4}" text-anchor="{anc}" class="cname" fill="{col}" stroke="#fff" stroke-width="3" paint-order="stroke">{esc(p["label"])}</text>')
    o.append('</g>')
    nm = p['a'] if '@' not in p['a'] else 'レール'
    o.append(end_dot(x, y, nm, col, dim, dy=(15 if up else -9)))
    o.append(badge(x + (14 if p.get('anchor') == 'end' else -14), ty + (10 if up else -10), p['n'], dim))
    return '\n'.join(o)


DRAW = {'two': draw_two, 'ic3': draw_ic3, 'xiao': draw_xiao, 'jmp': draw_jmp, 'ext': draw_ext}


def render_svg(step=None):
    """step=None: everything in colour. step=k: steps<k dimmed, k in colour, >k hidden."""
    o = [f'<svg viewBox="0 0 {W} {H}" xmlns="http://www.w3.org/2000/svg" font-family="\'Zen Kaku Gothic New\',sans-serif">',
         '<style>.cname{font-weight:700;font-size:11.5px;fill:#22303A}'
         '.pin{font-family:"IBM Plex Mono",monospace;font-size:9.5px;fill:#22303A}'
         '.rowlab{font-family:"IBM Plex Mono",monospace;font-size:10px;fill:#8a939b}'
         '.collab{font-family:"IBM Plex Mono",monospace;font-size:9px}</style>',
         board_base()]
    order = ['xiao', 'ic3', 'two', 'jmp', 'ext']
    for kind in order:
        for p in PARTS:
            if p['kind'] != kind:
                continue
            if step is not None and p['step'] > step:
                continue
            dim = step is not None and p['step'] < step
            o.append(DRAW[kind](p, dim))
    o.append('</svg>')
    return '\n'.join(o)


# ------------------------------------------------------------------ tables
def chip(color):
    return f'<i class="chip" style="background:{COLORS[color]}"></i>' if color else ''


def table_rows(step):
    rows = []
    for p in PARTS:
        if p['step'] != step:
            continue
        num = circ(p['n'])
        note = esc(p.get('note', ''))
        k = p['kind']
        if k == 'two':
            a, b = p['a'], p['b']
            rows.append((num, esc(p['name']), hole_name(a), hole_name(b), note))
        elif k == 'jmp':
            rows.append((num, chip(p['color']) + esc(p['name']), hole_name(p['a']), hole_name(p['b']), note))
        elif k == 'ext':
            rows.append((num, chip(p['color']) + esc(p['name']), hole_name(p['a']), '（外部）', note))
        elif k == 'ic3':
            a = ' / '.join(f'{h}={pin}' for h, pin in zip(p['holes'], p['pins']))
            rows.append((num, esc(p['name']), a, '', note))
        elif k == 'xiao':
            c0 = p['col0']
            a = f'e{c0}〜e{c0+6}: ' + ' '.join(XIAO_TOP)
            b = f'f{c0}〜f{c0+6}: ' + ' '.join(XIAO_BOT)
            rows.append((num, esc(p['name']), a, b, '列 20〜26 に溝をまたいで挿す。USB コネクタが左（列 20 側）'))
    out = ['<div class="tablebox"><table>',
           '<tr><th style="width:46px">番号</th><th style="width:170px">部品/線</th><th>端 A（穴）</th><th>端 B（穴）</th><th>備考</th></tr>']
    for num, name, a, b, note in rows:
        out.append(f'<tr><td class="n">{num}</td><td>{name}</td><td class="h">{a}</td><td class="h">{b}</td><td class="note">{note}</td></tr>')
    out.append('</table></div>')
    return '\n'.join(out)


CSS = r"""
:root{
  --paper:#F4F6F5; --card:#FFFFFF; --ink:#22303A; --muted:#5C6B76;
  --accent:#33658A; --accent-soft:#E3ECF2; --line:#D9E0DD; --warn-bg:#FBF0E8; --warn-bd:#C96F2E;
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--paper);color:var(--ink);font-family:"Zen Kaku Gothic New",-apple-system,"Hiragino Sans",sans-serif;line-height:1.75;font-size:15.5px}
.wrap{max-width:1380px;margin:0 auto;padding:0 24px 96px}
header{padding:52px 0 26px;border-bottom:3px solid var(--accent)}
.eyebrow{font-family:"IBM Plex Mono",monospace;font-size:12px;letter-spacing:.14em;color:var(--accent);text-transform:uppercase}
h1{font-size:32px;font-weight:700;text-wrap:balance;margin-top:6px}
.lede{color:var(--muted);margin-top:10px;max-width:44em}
h2{font-size:22px;font-weight:700;margin:56px 0 6px;padding-top:18px}
h2 .no{font-family:"IBM Plex Mono",monospace;color:var(--accent);margin-right:10px;font-size:18px}
h3{font-size:16.5px;font-weight:700;margin:24px 0 8px}
.sub{color:var(--muted);margin-bottom:16px;max-width:46em}
section{border-top:1px solid var(--line)}
p{max-width:46em}
.board{background:var(--card);border:1px solid var(--line);border-radius:8px;overflow-x:auto;margin-top:18px}
.board svg{display:block;min-width:1640px}
.legend{display:flex;flex-wrap:wrap;gap:8px 18px;padding:12px 18px;border-top:1px solid var(--line);font-size:13px}
.legend span{display:inline-flex;align-items:center;gap:7px}
.legend i{display:inline-block;width:22px;height:4px;border-radius:2px}
table{border-collapse:collapse;width:100%;margin-top:12px;background:var(--card);font-size:14px}
th,td{border:1px solid var(--line);padding:7px 10px;text-align:left;vertical-align:top}
th{background:var(--accent-soft);font-size:13px}
td.n{font-family:"IBM Plex Mono",monospace;font-weight:600;white-space:nowrap}
td.h{font-family:"IBM Plex Mono",monospace;white-space:nowrap;font-weight:600}
td.note{font-size:13px;color:var(--muted)}
.chip{display:inline-block;width:12px;height:12px;border-radius:3px;margin-right:6px;vertical-align:-1px}
.tablebox{overflow-x:auto}
.step{background:var(--card);border:1px solid var(--line);border-left:4px solid var(--accent);border-radius:8px;padding:16px 20px;margin-top:16px}
.step h3{margin-top:0;display:flex;align-items:baseline;gap:10px}
.step h3 .sno{font-family:"IBM Plex Mono",monospace;color:var(--accent);font-size:13.5px}
.step .board{margin-top:6px;margin-bottom:8px}
ul,ol{padding-left:1.4em;max-width:46em}
li{margin:4px 0}
.check{list-style:none;padding-left:0}
.check li{padding-left:30px;position:relative}
.check li::before{content:"";position:absolute;left:2px;top:6px;width:15px;height:15px;border:2px solid var(--accent);border-radius:3px;background:#fff}
.warn{background:var(--warn-bg);border:1px solid var(--warn-bd);border-left-width:4px;border-radius:8px;padding:13px 17px;margin:16px 0;max-width:52em}
.warn b{color:#9A4E12}
.kbd{background:var(--accent-soft);border-radius:4px;padding:1px 7px;font-family:"IBM Plex Mono",monospace;font-size:13px;white-space:nowrap}
footer{margin-top:64px;border-top:1px solid var(--line);padding-top:14px;font-size:12.5px;color:var(--muted)}
@media print{.board{overflow:visible}}
"""

LEGEND = ''.join(f'<span><i style="background:{COLORS[c]}"></i>{t}</span>' for c, t in [
    ('vbat', 'VBAT'), ('gnd', 'GND'), ('33', '3.3V'), ('vsw', 'VBAT_SW(スイッチ後)'), ('5v', '5V(測定時のみ)'),
    ('gate', 'Q1ゲート'), ('adc', '電圧測定'), ('tx', 'UART TX→白'), ('rx', 'UART 緑→RX')])


def build_html():
    h = []
    h.append('<title>SnowGauge ブレッドボード配線図</title>')
    h.append('<link rel="preconnect" href="https://fonts.googleapis.com">')
    h.append('<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Zen+Kaku+Gothic+New:wght@400;500;700&family=IBM+Plex+Mono:wght@400;600&display=swap">')
    h.append(f'<style>{CSS}</style>')
    h.append('<div class="wrap">')
    h.append('''<header>
  <div class="eyebrow">SnowGauge — breadboard prototype / TFmini Plus</div>
  <h1>SnowGauge ブレッドボード配線図</h1>
  <p class="lede">PCB が届くまでの開発・ファームウェア検証用に、はんだ付けなしで SnowGauge を組む配線図です。830 穴ブレッドボード 1 枚とジャンパワイヤで組めます。回路は <b>PCB v1.2 と同一トポロジ</b>（電池監視の分圧をスイッチ済みレールに接続。Q3・R6 は廃止）なので、ここで書いたファームウェアはそのまま PCB で動きます。</p>
</header>''')
    h.append('''<section>
<h2><span class="no">1</span>座標の読み方</h2>
<p class="sub">列番号（1〜63）×行（a〜e / f〜j）で穴を指定します。<b>同じ列の a〜e、f〜j はそれぞれ内部でつながっています</b>（中央の溝をまたぐと別グループ）。上下の赤・青の長いレールは横一列すべてつながっています。</p>
<ul>
<li>上の<b style="color:#D62828">赤レール = VBAT</b>（ダイオード通過後の電池+）／ 上の<b style="color:#2B5FA8">青レール = GND</b></li>
<li>下の青レールも GND（⑫のブリッジ線で上とつなぐ）。下の赤レールは使いません</li>
<li>例: <span class="kbd">c10</span> = 10 列目の c 行。部品の両端は図中の●の脇に穴名を書いてあります。レールへ行く線は同じ列のレール穴に挿してあれば十分です（レール上の位置は自由）</li>
</ul>
</section>''')
    h.append('<section>\n<h2><span class="no">2</span>全体配線図</h2>')
    h.append('<p class="sub">丸数字は下のチェックリストの番号。各部品・線の両端に穴名を表示しています（ステップごとの拡大図は §3）。</p>')
    h.append(f'<div class="board">{render_svg(None)}<div class="legend">{LEGEND}</div></div>')
    h.append('</section>')
    h.append('<section>\n<h2><span class="no">3</span>組立チェックリスト</h2>')
    h.append('<p class="sub">上から順に。<b>各ステップ末尾のテスター確認に合格してから次へ。</b>XIAO と TFmini を挿すのは電源確認のあとです。図はそのステップの部品を色付き、前のステップまでを薄く表示しています。</p>')
    for k in sorted(STEPS):
        title, checks = STEPS[k]
        h.append(f'<div class="step">\n<h3><span class="sno">STEP {k}</span>{title}</h3>')
        h.append(f'<div class="board">{render_svg(k)}</div>')
        h.append(table_rows(k))
        h.append('<ul class="check">' + ''.join(f'<li>{c}</li>' for c in checks) + '</ul>')
        if k == 2:
            h.append('<div class="warn"><b>最重要:</b> パソコンと USB でつなぐ時は<b>必ず電池を抜く</b>。USB を挿したまま電池（や安定化電源）でセンサ側を動かしたい時は、<b>⑩（橙、a10→g22）だけ抜けば両立できます</b>（XIAO は USB 給電、センサレールと分圧は電池給電。GND は共通のまま）。向きを間違えて挿すと壊れるので、⑨は挿す前に指差し確認。</div>')
        if k == 4:
            h.append('<p style="font-size:13.5px;margin-top:10px">FW メモ: 電池電圧はセンサレール ON 中に A0 を読む（読み値 ×2）。旧設計の D3・Q3・R6 は廃止。</p>')
        if k == 5:
            h.append('<p style="font-size:13.5px;margin-top:10px">TFmini の線が細くて抜けやすい場合は、ピンヘッダ付きジャンパをかませるか、線先にピンを圧着してください。安定化電源で代用する場合は電流制限を <b>1A 以上</b> に（TFmini 起動時ピーク 500mA）。</p>')
        h.append('</div>')
    h.append('</section>')
    h.append('''<section>
<h2><span class="no">4</span>注意メモ</h2>
<ul>
<li><b>電池を入れたまま USB をつながない</b>（STEP 2 参照。例外は⑩を抜いた時のみ）</li>
<li>向きがある部品: D1（帯）、C4（長い足が+）、U1/U2/Q1/Q2（1-2-3 の並び、印字面を手前に）。F33 と F50 は印字で読み分け</li>
<li>TFmini のピーク電流(500mA)はブレッドボードの接触抵抗に厳しいので、<b>電源系のジャンパは短く太いものを</b>。測距が不安定なら C4 の追加や配線短縮を試す</li>
<li>スリープ電流の最終評価は PCB で行う（ブレッドボードはリークが乗るため参考値）</li>
</ul>
</section>''')
    h.append('<footer>SnowGauge 設計仕様書 v0.11 / PCB v1.2 トポロジ準拠（分圧は VBAT_SW から・Q3/R6/D3 廃止）。ピン配置の正は pcb/README.md。TSD20 版は U2 を NJU7223F33 に差し替えるだけで同一配線。この HTML は docs/gen_breadboard.py が生成します（手編集しない）。</footer>')
    h.append('</div>')
    return '\n'.join(h)


def check_conflicts():
    used = {}
    for p in PARTS:
        hs = []
        if p['kind'] in ('two', 'jmp'):
            hs = [p['a'], p['b']]
        elif p['kind'] == 'ext':
            hs = [p['a']]
        elif p['kind'] == 'ic3':
            hs = p['holes']
        elif p['kind'] == 'xiao':
            hs = [f'e{p["col0"]+i}' for i in range(7)] + [f'f{p["col0"]+i}' for i in range(7)]
        for hh in hs:
            if '@' in hh:
                continue
            if hh in used:
                raise SystemExit(f'hole conflict: {hh} used by {used[hh]} and {p["name"]}')
            used[hh] = p['name']


if __name__ == '__main__':
    check_conflicts()
    import os
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'breadboard_guide.html')
    with open(out, 'w', encoding='utf-8') as f:
        head, body = build_html().split('<div class="wrap">', 1)
        f.write('<!doctype html>\n<html lang="ja"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">\n')
        f.write(head + '</head>\n<body>\n<div class="wrap">' + body + '\n</body></html>\n')
    print('wrote', out)
