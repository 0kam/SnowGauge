# SnowGauge Web Bluetooth page

Single-page app for site visits: sync time / timezone / location, download
records (CSV + raw), change the observation schedule, aim and ZERO the sensor.
Talks SMP (mcumgr) over Web Bluetooth plus the small calibration GATT service.

- **Public URL: https://0kam.github.io/SnowGauge/app/** (GitHub Pages from `docs/`).
- **Required firmware: Release `fw-2026-09-04` or newer** (SMP settings group +
  calibration service). Older firmware logs "本体 FW が設定グループ未対応".
- Version string: `APP_VERSION` in `app.js`, shown under the log panel as
  「アプリ版 YYYY-MM-DDx」. Bump it and the cache name in `sw.js` on every release.
- Usage instructions for field staff (Japanese): [`../04_app.md`](../04_app.md).
- Works in Chrome on Android and on PC (Windows/macOS/Linux). iPhone Safari has
  no Web Bluetooth; use a Web-Bluetooth browser app (e.g. Bluefy).
- Must be served over HTTPS (GitHub Pages) or from `localhost`. Installs as a
  PWA and caches itself for offline use in the field (offline-first; a new
  version is picked up on the next online load and applied on the next start).
- Local test: `python3 -m http.server 8787 --directory docs/app` then open
  http://localhost:8787/ in Chrome.

Files: `cbor.js` (CBOR), `smp.js` (generic SMP client: os / fs / settings
groups), `app.js` (SnowGauge specifics: record schema, settings schema,
calibration panel, CSV), `index.html`, `sw.js` + `manifest.webmanifest` (PWA).

Reusing for another logger: keep `cbor.js` / `smp.js`, replace the two schemas
and the calibration panel in `app.js`.
