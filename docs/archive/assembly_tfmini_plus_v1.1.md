# SnowGauge 組立説明書 — TFmini Plus 版(初号機)

- 版: v1.0(2026-09-02)
- 対象: 設計仕様書 v0.9 §4 のアーキテクチャに基づく TFmini Plus 構成(5V センサレール)
- 想定基板: ユニバーサル基板(スルーホール)。XIAO はピンソケット実装を推奨(交換可能にするため)
- **初心者向けビジュアル版**: [assembly_guide_visual.html](assembly_guide_visual.html)(部品写真+配線図付き。本書はエンジニア向けリファレンス)

---

## 1. 配線の全体像

```
BAT+ ──▶|── VBAT ──┬─────────────────────────┬──────────────┐
     1N5819        │                         │              │
    (逆接保護)      │ [常時系]                 │ [測定時のみ系]  │ [電圧監視]
                   │                         │              │
                   ▼                         ▼              ▼
             NJU7223F33               2SJ334 (P-ch)       R1 1MΩ
             VIN  VOUT ── 3V3_SYS     S ── D = VBAT_SW      │
              │    │        │         G ─┬─ R5 100kΩ ─ VBAT ├── ADC_NODE ── XIAO A0(D0)
             GND  0.1µF   XIAO 3V3    │  │                  │
                   +10µF              │  ▼                 R2 1MΩ
                                      │ NJU7223F50          │
                          Q2 2SK4017  │ VIN VOUT ─ 5V_SENS  ▼
                          D ──────────┘  │    │        Q3 2SK4017
                          G ─ R3 10kΩ ─ XIAO D2(SENSOR_EN)  D
                          │  R4 100kΩ ─ GND    │            G ── XIAO D3(VBAT_MEAS_EN)
                          S ─ GND              │            │    R6 100kΩ ─ GND
                                     C3 0.1µF + C4 470µF    S ─ GND
                                     (OS-CON)  │
                                               ▼
                                          TFmini Plus 赤(+5V)

UART:  XIAO D6(TX) ── R7 1kΩ ── TFmini 白(RX)
       XIAO D7(RX) ─────────── TFmini 緑(TX)
       TFmini 黒(GND) ── GND(共通)
```

## 2. 部品表(この基板で使う分)

| 記号 | 部品 | 型番 | 備考 |
|---|---|---|---|
| D1 | ショットキー | 1N5819 | 帯(カソード)が VBAT 側 |
| U1 | 3.3V LDO(常時) | NJU7223F33 | TO-220F。正面から 1=VOUT, 2=VIN, 3=GND |
| U2 | 5V LDO(遮断側) | NJU7223F50 | 同上ピン配置。TSD20 機では F33 に差し替え |
| Q1 | 高側スイッチ P-ch | 2SJ334 | 1=G, 2=D(タブ), 3=S |
| Q2, Q3 | 低側スイッチ N-ch | 2SK4017 ×2 | 1=G, 2=D, 3=S |
| R1, R2 | 1MΩ 1/4W | 分圧 | ±0.1V 精度で可(カーボン) |
| R3 | 10kΩ | Q2 ゲート直列 | |
| R4, R6 | 100kΩ | Q2/Q3 ゲートプルダウン | スリープ・リセット中の誤ONを防止 |
| R5 | 100kΩ | Q1 G-S プルアップ(→VBAT) | スリープ中のセンサレール確実OFF |
| R7 | 1kΩ | TX 直列 | 幽霊給電・短絡時の保護 |
| C1, C2, C3, C5 | 0.1µF X7R | U1/U2 の入出力直近 | 各 LDO の VIN・VOUT 両方に必須 |
| C4 | OS-CON 470µF/16V | 5V_SENS レール | 極性注意。TFmini ピーク 500mA 吸収 |
| C6 | 10µF | 3V3_SYS レール | XIAO 近傍 |
| A1 | XIAO nRF52840 Sense | — | ピンソケット実装推奨 |
| — | 電池ボックス 単3×4 | — | L91×4 |
| — | TFmini Plus | — | 付属ケーブル: 赤=+5V, 黒=GND, 緑=TX, 白=RX(**現物のデータシートで要確認**) |

## 3. ネットリスト(配線チェック用)

| ネット | 接続先(すべて列挙) |
|---|---|
| BAT+ | 電池+ → D1 アノード |
| VBAT | D1 カソード / U1-VIN / C1 / Q1-S / R5 上端 / R1 上端 |
| 3V3_SYS | U1-VOUT / C2 / C6 / XIAO 3V3 ピン |
| SENSOR_EN | XIAO D2 → R3 → Q2-G(R4 で GND へ) |
| Q1_GATE | Q1-G / R5 下端 / Q2-D |
| VBAT_SW | Q1-D / U2-VIN / C3 |
| 5V_SENS | U2-VOUT / C5 / C4+ / TFmini 赤 |
| ADC_NODE | R1 下端 / R2 上端 / XIAO A0(D0) |
| VBAT_MEAS_EN | XIAO D3 / Q3-G(R6 で GND へ) |
| MEAS_GND_SW | R2 下端 / Q3-D |
| UART_TX | XIAO D6 → R7 → TFmini 白(RX) |
| UART_RX | XIAO D7 ← TFmini 緑(TX) |
| GND | 電池− / U1-GND / U2-GND / Q2-S / Q3-S / R4 / R6 / 各コンデンサ− / XIAO GND / TFmini 黒 |

## 4. XIAO ピンアサイン(FW と共有する正)

| XIAO ピン | 信号 | 方向 | 役割 |
|---|---|---|---|
| 3V3 | 3V3_SYS | 電源入力 | 外部 LDO から給電(オンボードレギュレータは不使用) |
| GND | GND | — | 共通グラウンド |
| D0 / A0 | ADC_NODE | AIN | 電池電圧 = 読み値 ×2(分圧 1/2) |
| D2 | SENSOR_EN | OUT | High でセンサレール ON |
| D3 | VBAT_MEAS_EN | OUT | High で分圧通電(測定時のみ) |
| D6 | UART_TX | OUT | → TFmini RX(遮断前に Hi-Z/Low 化) |
| D7 | UART_RX | IN | ← TFmini TX |

D1, D4, D5, D8–D10 は空き(D4/D5 は将来の外付け I2C センサ用に温存。基板にパッドを残すこと)。

## 5. 組立手順

**順序の原則: 電源部を先に完成させて単体検証 → その後に XIAO・LiDAR を接続。** 逆接・過電圧事故から高価な部品を守ります。

### Step 1: 電源部(常時系)

1. 背の低い部品から実装: R 類 → D1 → C1/C2/C6 → U1
2. 0.1µF は **LDO のピンの直近**(数 mm 以内)に配置
3. 電池ボックスを接続し、テスターで確認:
   - [ ] VBAT ≈ 電池電圧 − 0.2〜0.35V(ショットキー降下)
   - [ ] 3V3_SYS = 3.25〜3.35V

### Step 2: 高側スイッチ+5V 系

1. Q1, Q2, R3, R4, R5 → U2, C3, C5 → C4(**極性注意**: OS-CON の長脚が+)
2. 検証(XIAO なしで手動テスト):
   - [ ] 無操作時(SENSOR_EN 開放): 5V_SENS = 0V(R4 のプルダウンで OFF が正常)
   - [ ] SENSOR_EN 相当の Q2 ゲートをジャンパ線で 3V3_SYS に接続: 5V_SENS = 4.9〜5.1V
   - [ ] ジャンパを外す: 5V_SENS が 0V に戻る

### Step 3: 電圧監視部

1. R1, R2, R6, Q3 を実装
2. 検証:
   - [ ] Q3 ゲート開放時: ADC_NODE ≈ VBAT(電流は流れないため分圧されず浮く。異常ではない)
   - [ ] Q3 ゲートを 3V3_SYS に接続: ADC_NODE = VBAT ÷ 2

### Step 4: XIAO 実装

1. ピンソケットを半田付けし、XIAO を装着
2. **重要: USB を接続するときは必ず電池を外す**(3V3 ピンへの外部給電と USB 給電の衝突を避ける)
3. 電池のみで XIAO の LED が起動することを確認(出荷時 FW で可)

### Step 5: TFmini Plus 接続

1. 付属コネクタ(JST GH 系)経由で接続。ケーブル色は現物で要確認
2. UART 線(R7 経由の TX、RX)を接続
3. FW のテストスケッチ(開発側で用意)で:
   - [ ] SENSOR_EN ON → TFmini 通電 → 距離フレーム受信
   - [ ] SENSOR_EN OFF → 5V_SENS = 0V、TFmini 消費 0

### Step 6: 最終確認(スリープ電流)

- [ ] µA レンジの電流計を電池と直列に入れ、スリープ時 **90µA 以下**(目標 ~50µA)を確認
- 超過時の容疑者: QSPI フラッシュの deep power-down 漏れ / IMU パワーダウン漏れ / TX の Hi-Z 化漏れ(幽霊給電)/ Q2・Q3 の誤 ON

## 6. 注意事項まとめ

- **USB と電池の同時接続禁止**(Step 4 参照)
- OS-CON・LDO の**極性/ピン配置を実装前に現物のデータシートで再確認**(TO-220 の 1-2-3 は品種で異なる)
- TFmini Plus の UART は 3.3V LVTTL(データシート明記)なので直結可。R7 1kΩ は幽霊給電対策の保険
- 半田ごては TO-220F(フルモールド)の放熱タブに無理な熱をかけない
- 屋外投入前に、筐体内で数日間の連続動作試験(レコード蓄積+スリープ電流)を行う

---

**【重要・2026-09-02追記】PCB版(pcb/)では SENSOR_EN が D2 → D10 に変更されました。**FWおよびPCBでの正は [pcb/README.md](../pcb/README.md) のピンマップです。本書§4のピンアサインは手配線(ブレッドボード)時のもの。
