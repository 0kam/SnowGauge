# SnowGauge 文書一覧と更新マップ

## 読む順（ユーザー向け）

| # | 文書 | 対象 | 内容 |
|---|---|---|---|
| 1 | [01_parts.md](01_parts.md) / [01_parts.csv](01_parts.csv) | 部品を買う人 | 購入リスト（BOM の唯一の正） |
| 2 | [02_assembly.md](02_assembly.md) | 組み立てる人 | ブレッドボード、基板 v1.2、筐体（検討中） |
| 2' | [breadboard_guide.html](https://0kam.github.io/SnowGauge/breadboard_guide.html) | 同上 | ブレッドボード配線の図解（生成物） |
| 3 | [03_firmware.md](03_firmware.md) | 書き込む人 | Release からのダウンロードと USB 書き込み |
| 4 | [04_app.md](04_app.md) | 現地作業する人 | アプリのインストールと設置・巡回手順 |
| 5 | [05_data.md](05_data.md) | 解析する人 | CSV の読み方、品質フラグ、再計算 |

## 技術文書（開発者向け）

| 文書 | 内容 |
|---|---|
| [../SnowGauge_設計仕様書_v0.12.md](../SnowGauge_設計仕様書_v0.12.md) | 要件・設計判断・電力収支・改版履歴。全文改版制（変更のたびに版を上げてファイル名も変える） |
| [../pcb/README.md](../pcb/README.md) | **ピンマップと電気トポロジの正**。基板は `pcb/generate_board.py` から生成（手編集禁止） |
| [../firmware/README.md](../firmware/README.md) | ビルド、書き込み、ベンチ用シェルコマンド |
| [record_format.md](record_format.md) | 40 バイトレコード、フラッシュ配置、CSV 列定義 |
| [app/README.md](app/README.md) | Web Bluetooth ページの構成と他ロガーへの流用 |
| [measurements/](measurements/) | 実測記録（`YYYY-MM-DD_topic.md`） |
| [archive/](archive/) | 旧版（v1.2 以前の回路。使わないこと） |
| [../CLAUDE.md](../CLAUDE.md) | AI エージェント向けの作業引き継ぎ（現状・決定事項・次の一手） |

## 更新マップ：何を変えたら、どの文書を直すか

| 変更 | 必ず更新するもの | あわせて確認 |
|---|---|---|
| **ピン割り当て・回路トポロジ** | `pcb/generate_board.py` → 再生成・DRC・ガーバー、`pcb/README.md`、`firmware/boards/*.overlay`、`docs/gen_breadboard.py`（PARTS）→ `breadboard_guide.html` 再生成 | 02_assembly.md、01_parts.md、仕様書 §4、CLAUDE.md の Key GPIO |
| **部品・数量・購入先** | `docs/01_parts.md` と `01_parts.csv` | 仕様書 §6（要約のみ）、`pcb/README.md` の追加部品欄 |
| **レコード形式**（フィールド追加・版上げ） | `firmware/src/record.h/.c`、`docs/record_format.md`、`docs/app/app.js` の `RECORD_SCHEMA`、`tools/decode_records.py` | 05_data.md、仕様書 §4.4 |
| **CSV の列** | `docs/app/app.js` の `CSV_COLUMNS`/`exportCSV`、`docs/record_format.md` | 05_data.md |
| **設定キー**（スケジュール・基準など） | `firmware/src/config.h/.c`、`docs/app/app.js` の `SETTINGS_SCHEMA` | 04_app.md、仕様書 §4.6 |
| **BLE**（アドバタイズ内容・GATT・SMP） | `firmware/src/ble_adv.h`、`cal_gatt.h`、`docs/app/app.js`/`smp.js` | 仕様書 §12、04_app.md |
| **アプリ UI** | `docs/app/index.html`、`app.js`（`APP_VERSION` を上げる）、`sw.js`（`CACHE` 名を上げる） | 04_app.md（ボタン名を一致させる） |
| **ファームウェアのリリース** | GitHub Release（タグ `fw-YYYY-MM-DD`、.uf2 / _dfu.zip / .hex）、`docs/03_firmware.md` のリンク | `firmware/README.md`、CLAUDE.md |
| **電流・電池寿命の実測** | `docs/measurements/YYYY-MM-DD_*.md` | 仕様書 §7.1、README の主な仕様 |
| **設計判断の変更** | 仕様書（版を上げ、§14 改版履歴に 1 行） | CLAUDE.md の決定事項 |

原則: 同じ事実を 2 か所に書かない。書く場合は片方を「正」と決めてもう片方からリンクする。
