#!/usr/bin/env python3
"""Generate docs/breadboard_guide.html (SnowGauge breadboard wiring guide).

Single source of truth for the breadboard netlist is PARTS below. Every
two-terminal part and every jumper lists its exact holes; the SVG draws
each end at that hole and prints the hole name next to it. One overview
figure plus one figure per assembly step (current step in colour, earlier
steps greyed out) are produced.

Run: python3 docs/gen_breadboard.py            -> breadboard_guide.html (TFmini Plus)
     python3 docs/gen_breadboard.py --tsd20    -> breadboard_guide_tsd20.html (TSD20:
                                                  U2 = NJU7223F33, sensor wires by pin number)
Both variants share the netlist; apply_variant() swaps U2, the sensor wires,
the step-3/5 checks and the wording.
"""
import html as _html
import sys

VARIANT = 'tsd20' if '--tsd20' in sys.argv else 'tfmini'

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
          'tx': '#1565C0', 'rx': '#5BA8DF', 'part': '#22303A',
          # TSD20 lead colours as on the sensor's connector (docs/tsd20_protocol.md)
          'w_red': '#D62828', 'w_yel': '#E6B800', 'w_grn': '#2E9E44', 'w_blk': '#111111'}
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
         note='向きなし'),
    # ---- STEP 2: XIAO (USB connector to the LEFT; top row = 5V..D7, bottom row = D0..D6) ----
    dict(n=9, step=2, kind='xiao', name='XIAO nRF52840 Sense', col0=20),
    dict(n=10, step=2, kind='jmp', name='ジャンパ 橙 (3.3V)', a='a10', b='a22', color='33',
         note='U1 の OUT 列 → XIAO 3V3 ピン(e22)の列'),
    dict(n=11, step=2, kind='jmp', name='ジャンパ 黒', a='a21', b='T-@21', color='gnd',
         note='XIAO GND ピン(e21)の列 → 上の青レール'),
    # ---- STEP 3: switch + 5 V (all in the upper a-e bank) ----
    dict(n=12, step=3, kind='ic3', name='Q2 2SK4017', holes=['c28', 'c29', 'c30'],
         pins=['G', 'D', 'S'], lab='left', note='印字面を手前にして左から G / D / S'),
    dict(n=13, step=3, kind='two', name='R3 10kΩ', a='b23', b='b28', shape='res', lab='left',
         note='XIAO D10 ピン(e23)の列 ↔ Q2 の G 列'),
    dict(n=14, step=3, kind='two', name='R4 100kΩ', a='a28', b='T-@28', shape='res', lab='left',
         note='Q2 の G 列 → 上の青レール（プルダウン）'),
    dict(n=15, step=3, kind='jmp', name='ジャンパ 黒', a='a30', b='T-@30', color='gnd',
         note='Q2 の S 列 → 上の青レール'),
    dict(n=16, step=3, kind='ic3', name='Q1 2SJ334', holes=['c32', 'c33', 'c34'],
         pins=['G', 'D', 'S'], lab='right', note='印字面を手前にして左から G / D / S'),
    dict(n=17, step=3, kind='two', name='R5 100kΩ', a='b32', b='b34', shape='res', lab='left',
         note='Q1 の G–S 間（プルアップ）'),
    dict(n=18, step=3, kind='jmp', name='ジャンパ (Q1ゲート)', a='a29', b='a32', color='gate',
         note='Q2 の D 列 → Q1 の G 列'),
    dict(n=19, step=3, kind='jmp', name='ジャンパ 赤', a='a34', b='T+@34', color='vbat',
         note='Q1 の S 列 → 上の赤レール'),
    dict(n=20, step=3, kind='ic3', name='U2 NJU7223F50', holes=['c38', 'c39', 'c40'],
         pins=['OUT', 'IN', 'GND'], lab='above', note='TSD20 版はここを F33 に'),
    dict(n=21, step=3, kind='jmp', name='ジャンパ (VBAT_SW)', a='a33', b='a39', color='vsw',
         note='Q1 の D 列 → U2 の IN 列'),
    dict(n=22, step=3, kind='jmp', name='ジャンパ 黒', a='a40', b='T-@40', color='gnd',
         note='U2 の GND 列 → 上の青レール'),
    dict(n=23, step=3, kind='two', name='C3 0.1µF', a='e39', b='e40', shape='cap', lab='below'),
    dict(n=24, step=3, kind='two', name='C4 OS-CON 470µF', a='b42', b='b44', shape='ecap', lab='right',
         sub='C4', note='極性注意: 長い足(+) を b42、− を b44'),
    dict(n=24, step=3, kind='two', name='C5 0.1µF', a='c42', b='c44', shape='cap', sub='C5', lab='below'),
    dict(n=25, step=3, kind='jmp', name='ジャンパ (5V)', a='a38', b='a42', color='5v',
         note='U2 の OUT 列 → 5V ノード'),
    dict(n=26, step=3, kind='jmp', name='ジャンパ 黒', a='a44', b='T-@44', color='gnd',
         note='C4 の − 列 → 上の青レール'),
    # ---- STEP 4: battery divider ----
    dict(n=27, step=4, kind='two', name='R1 1MΩ', a='e33', b='e36', shape='res', lab='left', hl='below',
         note='Q1 の D 列(VBAT_SW) ↔ 36 列'),
    dict(n=28, step=4, kind='two', name='R2 1MΩ', a='b36', b='b41', shape='res', lab='above'),
    dict(n=29, step=4, kind='jmp', name='ジャンパ 黒', a='a41', b='T-@41', color='gnd'),
    dict(n=30, step=4, kind='jmp', name='ジャンパ 紫 (電圧測定)', a='a36', b='g20', color='adc', lb=15,
         note='分圧の中点 → XIAO A0(D0) ピン(f20)の列（溝をまたぐ）'),
    # ---- STEP 5: TFmini ----
    dict(n=31, step=5, kind='two', name='R7 1kΩ', a='g26', b='g31', shape='res', lab='below',
         note='XIAO D6(TX) ピン(f26)の列 ↔ 31 列'),
    dict(n=32, step=5, kind='ext', name='TFmini 白 (RX)', a='h31', color='tx',
         label='TFmini 白(RX)', note='R7 の先'),
    dict(n=33, step=5, kind='ext', name='TFmini 緑 (TX)', a='a26', color='rx',
         label='TFmini 緑(TX)', note='XIAO D7(RX) ピン(e26)の列'),
    dict(n=34, step=5, kind='ext', name='TFmini 赤 (+5V)', a='e42', color='5v',
         label='TFmini 赤(+5V)', note='5V ノード'),
    dict(n=35, step=5, kind='ext', name='TFmini 黒 (GND)', a='T-@50', color='gnd',
         label='TFmini 黒(GND)'),
]

# XIAO with its USB connector pointing LEFT (component side up):
XIAO_TOP = ['5V', 'GND', '3V3', 'D10', 'D9', 'D8', 'D7/RX']    # row e, cols 20..26
XIAO_BOT = ['D0/A0', 'D1', 'D2', 'D3', 'D4', 'D5', 'D6/TX']    # row f, cols 20..26

TEXTS = {
    'tfmini': dict(
        sensor='TFmini Plus', rail='5V', rail_legend='5V(測定時のみ)', rail_v='4.9〜5.1V', u2='NJU7223F50',
        tx_legend='UART TX→白', rx_legend='UART 緑→RX', out='breadboard_guide.html',
        other='TSD20 版', other_file='breadboard_guide_tsd20.html',
        step5_note='TFmini の線が細くて抜けやすい場合は、ピンヘッダ付きジャンパをかませるか、線先にピンを圧着してください。安定化電源で代用する場合は電流制限を <b>1A 以上</b> に（TFmini 起動時ピーク 500mA）。',
        peak_note='TFmini のピーク電流(500mA)はブレッドボードの接触抵抗に厳しいので、<b>電源系のジャンパは短く太いものを</b>。測距が不安定なら C4 の追加や配線短縮を試す',
        fw_note='テスト FW で: <span class="kbd">rail on</span> → <span class="kbd">lidar raw 500</span>（旧名 <span class="kbd">tfmini raw</span> も可）で距離フレーム受信、<span class="kbd">rail off</span> → e42 = 0V'),
    'tsd20': dict(
        sensor='TSD20', rail='3.3V(SW)', rail_legend='センサ 3.3V ノード(測定時のみ)', rail_v='3.25〜3.35V', u2='NJU7223F33',
        tx_legend='UART TX→緑(④)', rx_legend='UART 黄(③)→RX', out='breadboard_guide_tsd20.html',
        other='TFmini Plus 版', other_file='breadboard_guide.html',
        step5_note='TSD20 のリード線（6 本、20cm、先端すずめっき）は基板上のコネクタ側で左から <b>白・赤・黄・緑・水色・黒</b> = ピン <b>1=NC, 2=3.3V, 3=TX, 4=RX, 5=NC, 6=GND</b>（色は手持ち品の実物で確認、説明書に記載なし。赤=3.3V・黒=GND が並びの根拠）。TFmini との対応: 赤→赤、緑→黄、白→緑、黒→黒。線は末端加工なしの極細線なので、そのままでは挿せません。<b>被覆をむいた単線ジャンパ（またはピンヘッダの足）に 1 本ずつはんだ付けし、継ぎ目を熱収縮チューブで覆って</b>から挿します（動作確認だけならミノムシクリップでも可。ねじって巻くだけは電源線では不可）。継ぎ目の手前で線をテープで固定して引っ張りを逃がすこと。安定化電源の電流制限は <b>0.5A 以上</b>（TSD20 ピーク 70mA + XIAO）。<b>TSD20 には逆接・過電圧保護がない</b>ので、②を 5V ノードや VBAT に挿さないこと。',
        peak_note='TSD20 のピーク電流は 70mA なので配線の接触抵抗には寛容。U2 の出力（e42 ノード）が 3.3V であることを、TSD20 をつなぐ<b>前</b>に必ず確認する（F50 が残っていると 5V がかかって壊れる）',
        fw_note='TSD20 版 FW（<span class="kbd">overlay-tsd20.conf</span> ビルド、BLE 名 <span class="kbd">SG-TSD-XXXX</span>）で: <span class="kbd">rail on</span> → <span class="kbd">lidar raw 500</span>（<span class="kbd">tsd20 raw</span> も可）で <span class="kbd">dist=xxxx mm</span> のフレーム受信（460800 baud、200Hz）、<span class="kbd">lidar read</span> で cksum_err=0、<span class="kbd">rail off</span> → e42 = 0V'),
}
T = TEXTS[VARIANT]


def apply_variant():
    """Swap the variant-specific parts of PARTS / STEPS in place."""
    if VARIANT != 'tsd20':
        return
    for p in PARTS:
        if p['name'] == 'U2 NJU7223F50':
            p['name'] = 'U2 NJU7223F33'
            p['note'] = 'TFmini 版はここが F50。印字で確認'
        if p['name'] == 'ジャンパ (5V)':
            p['name'] = 'ジャンパ (センサ 3.3V)'
            p['note'] = 'U2 の OUT 列 → センサ 3.3V ノード'
    del PARTS[[i for i, p in enumerate(PARTS) if p['step'] == 5 and p['kind'] == 'ext'][0]:]
    PARTS.extend([
        dict(n=32, step=5, kind='ext', name='TSD20 ④ 緑 (RX)', a='h31', color='w_grn',
             label='TSD20 ④緑(RX)', note='R7 の先（XIAO D6 TX → 1kΩ → TSD20 RX）。TFmini の白と同じ穴'),
        dict(n=33, step=5, kind='ext', name='TSD20 ③ 黄 (TX)', a='a26', color='w_yel',
             label='TSD20 ③黄(TX)', note='XIAO D7(RX) ピン(e26)の列。TFmini の緑と同じ穴'),
        dict(n=34, step=5, kind='ext', name='TSD20 ② 赤 (+3.3V)', a='e42', color='w_red',
             label='TSD20 ②赤(3.3V)', note='センサ 3.3V ノード（U2=F33 の出力。5V ではない！）'),
        dict(n=35, step=5, kind='ext', name='TSD20 ⑥ 黒 (GND)', a='T-@50', color='w_blk',
             label='TSD20 ⑥黒(GND)', note='①白 と ⑤水色 は NC（未接続のまま）'),
    ])
    STEPS[3] = ('スイッチと センサ 3.3V 系', [
        'なにもしない時: e42−GND 間 = <span class="kbd">0V</span>',
        'ジャンパ線で a28（Q2 の G）を 3.3V（c10）に触れさせる: e42 = <span class="kbd">3.25〜3.35V</span>、離すと 0V に戻る（<b>4.9〜5.1V なら U2 が F50 のまま</b>。TSD20 をつなぐ前に必ず直す）',
        'a28 を c10 に触れさせている間: a32（Q1 の G）= <span class="kbd">0V 近く</span>、a33（Q1 の D = VBAT_SW）= <span class="kbd">電池電圧</span>。離している時は逆に a32 ≈ 電池電圧、a33 = 0V'])
    STEPS[5] = ('TSD20', [
        T['fw_note'],
        'µA 電流計を電池と直列に: スリープ <span class="kbd">90µA 以下</span>（ブレッドボードは接触・リークで数 µA 上振れすることあり）'])


STEPS = {
    1: ('電源部', ['赤レール−青レール間: <span class="kbd">電池電圧 −0.2〜0.35V</span>',
                  'c10（U1 の OUT）−青レール間: <span class="kbd">3.25〜3.35V</span>']),
    2: ('XIAO', ['XIAO の 3V3 ピン(e22)−GND ピン(e21) 間 = <span class="kbd">3.3V</span>（SnowGauge FW は LED を使わないので LED は点きません）']),
    3: ('スイッチと 5V 系', ['なにもしない時: e42−GND 間 = <span class="kbd">0V</span>',
                       'ジャンパ線で a28（Q2 の G）を 3.3V（c10）に触れさせる: e42 = <span class="kbd">4.9〜5.1V</span>、離すと 0V に戻る',
                       'a28 を c10 に触れさせている間: a32（Q1 の G）= <span class="kbd">0V 近く</span>、a33（Q1 の D = VBAT_SW）= <span class="kbd">電池電圧</span>。離している時は逆に a32 ≈ 電池電圧、a33 = 0V']),
    4: ('電池電圧の見張り（スイッチ済みレールから分圧）',
        ['STEP 3 と同じく a28 を c10 に触れさせている間: a36−GND 間 = <span class="kbd">電池電圧のほぼ半分</span>。離している時は <span class="kbd">0V</span>']),
    5: ('TFmini Plus', [T['fw_note'],
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
    if p.get('hl') and holes_above is not None:
        holes_above = (p['hl'] == 'above')
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
            rows.append((num, esc(p['name']), a, b, '列 20〜26 に溝をまたいで挿す。<b>USB コネクタが左（列 20 側）</b>。部品面を上にして見ると上列が 5V…D7、下列が D0…D6'))
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

def legend():
    items = [('vbat', 'VBAT'), ('gnd', 'GND'), ('33', '3.3V'), ('vsw', 'VBAT_SW(スイッチ後)'), ('5v', T['rail_legend']),
             ('gate', 'Q1ゲート'), ('adc', '電圧測定')]
    if VARIANT == 'tsd20':
        items += [('w_red', 'TSD20 赤 = 3.3V'), ('w_yel', 'TSD20 黄 = TX→XIAO D7'),
                  ('w_grn', 'TSD20 緑 = RX←XIAO D6'), ('w_blk', 'TSD20 黒 = GND')]
    else:
        items += [('tx', T['tx_legend']), ('rx', T['rx_legend'])]
    return ''.join(f'<span><i style="background:{COLORS[c]}"></i>{t}</span>' for c, t in items)


def build_html():
    h = []
    h.append('<title>SnowGauge ブレッドボード配線図</title>')
    h.append('<link rel="preconnect" href="https://fonts.googleapis.com">')
    h.append('<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Zen+Kaku+Gothic+New:wght@400;500;700&family=IBM+Plex+Mono:wght@400;600&display=swap">')
    h.append(f'<style>{CSS}</style>')
    h.append('<div class="wrap">')
    h.append(f'''<header>
  <div class="eyebrow">SnowGauge — breadboard prototype / {T['sensor']}</div>
  <h1>SnowGauge ブレッドボード配線図</h1>
  <p class="lede">PCB が届くまでの開発・ファームウェア検証用に、はんだ付けなしで SnowGauge（<b>{T['sensor']} 版</b>、U2 = {T['u2']}）を組む配線図です。{T['other']}は <a href="{T['other_file']}">{T['other_file']}</a>（違いは U2 とセンサ配線だけ、§5 参照）。830 穴ブレッドボード 1 枚とジャンパワイヤで組めます。回路は <b>PCB v1.2 と同一トポロジ</b>（電池監視の分圧をスイッチ済みレールに接続。Q3・R6 は廃止）なので、ここで書いたファームウェアはそのまま PCB で動きます。</p>
</header>''')
    h.append('''<section>
<h2><span class="no">1</span>座標の読み方</h2>
<p class="sub">列番号（1〜63）×行（a〜e / f〜j）で穴を指定します。<b>同じ列の a〜e、f〜j はそれぞれ内部でつながっています</b>（中央の溝をまたぐと別グループ）。上下の赤・青の長いレールは横一列すべてつながっています。</p>
<ul>
<li>上の<b style="color:#D62828">赤レール = VBAT</b>（ダイオード通過後の電池+）／ 上の<b style="color:#2B5FA8">青レール = GND</b></li>
<li>下の赤・青レールは使いません（すべて上のレールに戻します）</li>
<li>例: <span class="kbd">c10</span> = 10 列目の c 行。部品の両端は図中の●の脇に穴名を書いてあります。レールへ行く線は同じ列のレール穴に挿してあれば十分です（レール上の位置は自由）</li>
</ul>
</section>''')
    h.append('<section>\n<h2><span class="no">2</span>全体配線図</h2>')
    h.append('<p class="sub">丸数字は下のチェックリストの番号。各部品・線の両端に穴名を表示しています（ステップごとの拡大図は §3）。</p>')
    h.append(f'<div class="board">{render_svg(None)}<div class="legend">{legend()}</div></div>')
    h.append('</section>')
    h.append('<section>\n<h2><span class="no">3</span>組立チェックリスト</h2>')
    h.append(f'<p class="sub">上から順に。<b>各ステップ末尾のテスター確認に合格してから次へ。</b>XIAO と {T['sensor']} を挿すのは電源確認のあとです。図はそのステップの部品を色付き、前のステップまでを薄く表示しています。</p>')
    for k in sorted(STEPS):
        title, checks = STEPS[k]
        h.append(f'<div class="step">\n<h3><span class="sno">STEP {k}</span>{title}</h3>')
        h.append(f'<div class="board">{render_svg(k)}</div>')
        h.append(table_rows(k))
        h.append('<ul class="check">' + ''.join(f'<li>{c}</li>' for c in checks) + '</ul>')
        if k == 2:
            h.append('<div class="warn"><b>最重要:</b> パソコンと USB でつなぐ時は<b>必ず電池を抜く</b>。USB を挿したまま電池（や安定化電源）でセンサ側を動かしたい時は、<b>⑩（橙、a10→a22）だけ抜けば両立できます</b>（XIAO は USB 給電、センサレールと分圧は電池給電。GND は共通のまま）。向きを間違えて挿すと壊れるので、⑨は挿す前に指差し確認。</div>')
        if k == 4:
            h.append('<p style="font-size:13.5px;margin-top:10px">FW メモ: 電池電圧はセンサレール ON 中に A0 を読む（読み値 ×2）。旧設計の D3・Q3・R6 は廃止。</p>')
        if k == 5:
            h.append(f'<p style="font-size:13.5px;margin-top:10px">{T['step5_note']}</p>')
        h.append('</div>')
    h.append('</section>')
    h.append(f'''<section>
<h2><span class="no">4</span>注意メモ</h2>
<ul>
<li><b>電池を入れたまま USB をつながない</b>（STEP 2 参照。例外は⑩を抜いた時のみ）</li>
<li>向きがある部品: D1（帯）、C4（長い足が+）、U1/U2/Q1/Q2（1-2-3 の並び、印字面を手前に）。F33 と F50 は印字で読み分け</li>
<li>{T['peak_note']}</li>
<li>スリープ電流の最終評価は PCB で行う（ブレッドボードはリークが乗るため参考値）</li>
<li><b>抵抗の代替値</b>（手持ちで組む場合）: R3 は <b>150Ω〜2.2kΩ</b>（R4 との分圧で Q2 のゲート電圧が 2.7V 以上になること。R3=R4=10k は NG）、R4・R5 は <b>10kΩ</b> 可、R1/R2 は <b>同じ値なら 5k〜1M のどれでも</b>（FW は「読み値 ×2」）、R7 は <b>1k〜5k</b>。分圧は VBAT_SW 側なのでスリープ電流には影響しない</li>
</ul>
</section>''')
    h.append('''<section>
<h2><span class="no">5</span>TFmini Plus 版 ⇄ TSD20 版の組み替え</h2>
<p class="sub">同じ基板・同じ配線で、変わるのは <b>U2（センサ用 LDO）とセンサの 4 本の線だけ</b>。ファームウェアも別ビルド（TFmini 版 = 既定、TSD20 版 = <span class="kbd">overlay-tsd20.conf</span>、BLE 名は <span class="kbd">SG-TFM-</span> / <span class="kbd">SG-TSD-</span>）。</p>
<ol style="font-size:14px;line-height:1.7">
<li><b>電源を切る</b>（電池・安定化電源・USB すべて外す）。センサの線を 4 本とも抜く</li>
<li><b>U2 を差し替える</b>: c38–c40 の NJU7223F50 を抜き、同じ向き（印字面を手前に OUT–IN–GND）で NJU7223F33 を挿す（逆方向は F33 → F50）。C3/C4/C5 と㉑〜㉖のジャンパはそのまま</li>
<li>電源を入れ、<b>STEP 3 の確認</b>: a28 を c10 に触れさせて e42 = <b>3.25〜3.35V（TSD20 版）</b> / <b>4.9〜5.1V（TFmini 版）</b>。<b>この確認を飛ばして TSD20 に 5V をかけると壊れます</b>（逆接・過電圧保護なし）</li>
<li>センサをつなぐ（電源を切ってから）。TSD20 は線の色ではなく<b>ピン番号</b>で: ② 赤 3.3V → e42、③ 黄 TX → a26（XIAO D7 RX の列）、④ 緑 RX → h31（R7 の先）、⑥ 黒 GND → 青レール。① 白・⑤ 水色 は NC。TFmini は 赤 → e42、緑 → a26、白 → h31、黒 → 青レール</li>
<li>該当バリアントの FW を書き込み、STEP 5 の確認（<span class="kbd">rail on</span> → <span class="kbd">lidar raw 500</span>）。TSD20 版はフレームが <span class="kbd">dist=xxxx mm</span>（460800 baud）で出る</li>
</ol>
<p style="font-size:13.5px">USB と電池/安定化電源を同時につなぐ時のルール（⑩を抜く）はどちらの版も同じ。</p>
</section>''')
    h.append(f'<footer>SnowGauge 設計仕様書 v0.13 / PCB v1.2 トポロジ準拠（分圧は VBAT_SW から・Q3/R6/D3 廃止）。ピン配置の正は pcb/README.md。この HTML（{T["sensor"]} 版）は docs/gen_breadboard.py{" --tsd20" if VARIANT == "tsd20" else ""} が生成します（手編集しない）。</footer>')
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
    apply_variant()
    check_conflicts()
    import os
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), T['out'])
    with open(out, 'w', encoding='utf-8') as f:
        head, body = build_html().split('<div class="wrap">', 1)
        f.write('<!doctype html>\n<html lang="ja"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">\n')
        f.write(head + '</head>\n<body>\n<div class="wrap">' + body + '\n</body></html>\n')
    print('wrote', out)
