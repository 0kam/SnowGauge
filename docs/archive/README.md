# docs/archive — 旧版の組立資料（使用禁止）

ここにあるファイルは **PCB v1.2 より前**（ユニバーサル基板・手配線前提、設計仕様書 v0.9 §4）の組立資料です。

- `assembly_tfmini_plus_v1.1.md` — 旧 組立説明書（エンジニア向け）
- `assembly_guide_visual_v1.1.html` — 旧 ビジュアル組立ガイド（初心者向け）

v1.2 で回路が変わったため、**現在の基板・ブレッドボードには合いません。組み立てには使わないでください。**
主な相違点:

- 旧: SENSOR_EN = D2、VBAT_MEAS_EN = D3、分圧の低側スイッチ Q3（2SK4017）とそのプルダウン R6 あり
- 現在（v1.2）: SENSOR_EN = **D10**、**D3・Q3・R6 は廃止**
- 旧: 電池電圧の分圧（R1/R2）が VBAT に常時接続
- 現在（v1.2）: 分圧は **VBAT_SW（スイッチ後のレール）** に接続。電池電圧は SENSOR_EN を ON にしてから A0 を読む（スリープ中のノードは 0 V）

現行資料:

- 部品表: [../01_parts.md](../01_parts.md)
- 組み立て: [../02_assembly.md](../02_assembly.md)
- ブレッドボード配線図: [../breadboard_guide.html](../breadboard_guide.html)（`docs/gen_breadboard.py` が生成）
- ピン配置・回路トポロジの正: [../../pcb/README.md](../../pcb/README.md)

注: 2 つの旧ガイドは互いを旧ファイル名（`assembly_guide_visual.html`、`docs/assembly_tfmini_plus.md`）で参照しています。このフォルダ内では末尾に `_v1.1` が付いたファイルがそれに当たります。
