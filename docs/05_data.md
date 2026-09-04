# 05. データの見方

アプリ（04 章）で保存した CSV を、表計算ソフトや Python で読むための説明です。
列の正確な定義は [record_format.md](record_format.md) が正本です（ここでは重複させません）。

---

## 1. ファイルの名前と中身

### CSV（ふつうはこれだけ使う）

- ファイル名: **`<本体名>_<保存日時>.csv`**、例 `SG-3F2A_2026-12-31-03-15-20.csv`
  - 本体名 = Bluetooth に出る `SG-XXXX`。
  - 保存日時はスマホがファイルを作った時刻（**UTC**）です。観測時刻ではありません。
- 1 行目が見出し、2 行目以降が 1 観測 = 1 行。UTF-8（BOM 付き）。Excel でそのまま開けます。
- 列の並びと意味: [record_format.md の「CSV export」](record_format.md#csv-export-web-bluetooth-page-docsapp) を見てください。
- 本体に入っている**全期間**の記録が毎回入ります（消去しない限り、前回と同じ行がもう一度入ります → 第 5 節の重複除去）。

### 生データ（保険）

アプリの「生データ保存」で出るファイル:

- `<本体名>_rec_YYYYMM.bin` … 本体内のファイルそのもの。**月ごと（UTC の月）**に 1 ファイル、1 記録 = 40 バイト。
- `<本体名>_rec_notime.bin` … 時計が一度も合っていない間の記録（設置時に同期を忘れた場合など）。
- `<本体名>_site.json` … サイト名・緯度経度・タイムゾーンとその変更履歴。

---

## 2. 列の読み方（要点）

| 列 | 読み方 |
|---|---|
| `device_id`, `site_name`, `lat`, `lon`, `alt_m`, `tz_offset_min` | 本体名とサイト情報。**全行に同じ値が繰り返し**入っています（複数台をつなげるため） |
| `time_utc` | 観測時刻（UTC）。`2026-12-31T03:00:00Z` |
| `time_local` | 同じ時刻を現地時刻で。`2026-12-31T12:00:00+09:00`。`tz_offset_min`（分）を足したもの |
| `seq` | 本体内の通し番号。消去すると 1 から数え直し |
| `flags` | 本体が付けた印。`\|` 区切り（第 3 節） |
| `dist_cm` | **斜距離**の中央値（cm）。センサから雪面までの、センサの向きに沿った距離。有効値がないときは空 |
| `dist_var_cm2`, `strength`, `n_frames`, `n_valid`, `n_out_of_range` | 品質（第 4 節） |
| `tilt_deg` | センサの傾き（真下方向からの角度）。`pitch_deg` / `roll_deg` はその内訳 |
| `imu_temp_c` | 基板上の温度。気温の目安（直射日光・筐体内なので参考値） |
| `lidar_temp_c` | 距離センサの**内部**温度。50〜75 °C は正常で、気温ではありません |
| `vbat_start_mv`, `vbat_end_mv` | 電池電圧（mV）、センサ通電の直後と直前。`end` の方が負荷時。4400 未満は交換時期 |
| `d0_cm`, `theta0_deg` | 基準値（設置時の ZERO または積雪深入力で本体に保存されたもの） |
| `snow_depth_cm` | **積雪深** = `(d0_cm − dist_cm) × cos(tilt_deg)`。基準値がない・距離がないときは空 |

> **注意**: `d0_cm` と `snow_depth_cm` は、**ダウンロードした時点で本体に入っていた基準値**を全行に当てはめて計算しています。途中で基準を取り直した場合、それより前の行の積雪深は正しくありません。第 6 節の方法で計算し直してください。

---

## 3. `flags` 列の意味

| 値 | 意味 |
|---|---|
| `time_synced` | 起動後に時刻合わせ済み。時刻はそのまま信用できる |
| `time_estimated` | 再起動後、まだ同期していない（第 7 節）。時刻は遅れている可能性がある |
| （どちらもなく `time_utc` が空） | 時計が一度も合っていない。順番だけ正しい |
| `lidar_ok` | 距離の有効値が 1 つ以上ある |
| `tilt_ok` | 傾斜センサが読めた |
| `manual` | 自動観測ではなく、シェルや BLE から手動で測ったもの。解析からは除くのが普通 |
| `first_after_boot` | 再起動（電池交換など）後の最初の記録。区切りの目印 |

---

## 4. 品質の列と、アプリの「要確認」

本体は 1 回の観測で距離センサを 100 回（`n_frames`）読み、そのうち使える回（`n_valid`）の中央値を `dist_cm`、ばらつき（分散）を `dist_var_cm2` にしています。`n_out_of_range` は「測れない」「信号が弱い」「飽和」と返ってきた回数です。

アプリの「要確認」欄と同じ基準で行を選り分けるなら:

| 条件 | アプリの表示 | 扱い |
|---|---|---|
| `dist_cm` が空 | 測距なし | 欠測扱い |
| `n_valid < n_frames × 0.5` | 有効率低 | 降雪・霧・汚れ。要注意 |
| `dist_var_cm2 > 4` | ばらつき大 | 降雪中・揺れ。中央値は使えることが多い |
| `n_out_of_range > n_frames × 0.5` | レンジ外多 | 欠測に近い |
| `flags` に `time_synced` がない | 時刻推定 / 時刻なし | 第 7 節 |
| 前の行との間隔が観測間隔の 1.5 倍超（`manual` 以外） | 欠測あり | 電池・再起動 |
| `vbat_end_mv < 4400` | 電池低 | 交換時期。通電直後の電圧（`vbat_start_mv`）が 4600 未満だと本体は距離測定を止める |

`strength`（信号強度）は 100 未満だとセンサの仕様上信頼できず、65535 は飽和です。雪面では通常数百〜数千です。

---

## 5. 複数台・複数回のファイルをつなげる

サイト情報の列が全行に入っているので、そのまま縦につなげられます。**同じ本体を消去せずに何度も回収すると同じ記録が重複する**ので、`device_id` + `time_utc` + `seq` で重複を落とします。

```python
import glob, pandas as pd
df = pd.concat(pd.read_csv(f) for f in glob.glob("SG-*.csv"))
df["time_local"] = pd.to_datetime(df["time_local"], utc=True).dt.tz_convert("Asia/Tokyo")
df = df.drop_duplicates(["device_id", "time_utc", "seq"]).sort_values(["device_id", "time_utc"])
df = df[~df["flags"].fillna("").str.contains("manual")]          # 手動測定を除く
```

`df.groupby("device_id")["snow_depth_cm"].plot()` などで台ごとに描けます。

---

## 6. 別の基準値で積雪深を計算し直す

CSV には生の `dist_cm` と `tilt_deg` が残っているので、基準値を変えて計算し直せます。

```
snow_depth_cm = (d0_new − dist_cm) × cos(tilt_deg [度])
```

- **無雪期の値から d0 を決め直す**: 雪のない日の `dist_cm` の中央値を `d0_new` にする（もっとも確実な方法）。
- **設置時にプローブで測った積雪深 `h` から**: そのときの行の `dist_cm` = `d`、`tilt_deg` = `t` を使って `d0_new = d + h / cos(t)`（本体の「積雪深で基準設定」と同じ式）。
- 基準を取り直した日時は `theta0_deg` の隣にはなく、アプリのパネル 3「基準設定日時」に出ます。取り直したら野帳に控えてください。

```python
import numpy as np
d0_new = 245.0
df["snow_depth_cm"] = (d0_new - df["dist_cm"]) * np.cos(np.radians(df["tilt_deg"]))
```

`tilt_deg` が設置時（`theta0_deg`）から大きく変わっている行は、ポールが傾いた可能性があります。

---

## 7. 時刻の列と `time_estimated`

- 本体の時計は **UTC** で動き、`time_utc` はその値です。`time_local` は本体に保存されたタイムゾーン（`tz_offset_min`、同期時にスマホから自動設定）で表示し直しただけです。
- 本体は電池を外すと時計を失います。次に電源が入ると、**最後の記録の時刻から**時計を再開し、次に同期されるまでの記録に `time_estimated` を付けます。この間の時刻は「間隔は正しいが、全体が遅れている」状態です（電源が切れていた時間の分だけ遅れます）。
- 直し方: `time_estimated` の区間の直後にある最初の `time_synced` の行（巡回で同期した直後の記録）を基準に、観測間隔を逆算して埋め直すのが実用的です。`first_after_boot` が再起動の位置を示します。厳密には遅れの量は分からないので、この区間は「時刻は概算」と注記して扱ってください。
- 巡回のたびにアプリで「同期」を押していれば、この区間は再起動から次の巡回までの間だけです。

---

## 8. 生データ（.bin）から CSV を作る

アプリの CSV が使えないとき（消してしまった、壊れた）、「生データ保存」の `.bin` から Python で復元できます。

```bash
python3 tools/decode_records.py SG-3F2A_rec_202612.bin SG-3F2A_rec_202701.bin > records.csv
```

- 列は `seq, epoch, time_utc, flags, dist_cm, var_cm2, strength, n_frames, n_valid, n_oor, tilt_deg, pitch_deg, roll_deg, imu_temp_c, lidar_temp_c, vbat_start_mv, vbat_end_mv`。**サイト情報・現地時刻・積雪深の列はありません**（第 6 節の式で計算、サイト情報は `site.json` を見る）。
- 壊れた記録は飛ばして、標準エラー出力に件数が出ます。
- USB シェル（03 章）で `rec hex 5` と打って出た 16 進の行を `--hex "…"` に渡すと、1 件ずつ復元することもできます。USB で全件を文字として吸い出すなら `python3 tools/sgshell.py --file dump.txt "rec dump 0"`（pyserial が必要）。

## 補足: タイムゾーン列

`time_local` と `tz_offset_min` は本体に保存されたタイムゾーン設定から作られます。アプリで一度も「同期」していない本体は既定の +09:00（540 分）です。海外や別のタイムゾーンで使うときは、現地でスマホから同期すると自動で更新されます。
