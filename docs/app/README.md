# SnowGauge Web Bluetooth page

Single-page app for site visits: sync time / timezone / location, download
records (CSV + raw), change the observation schedule, aim and ZERO the sensor.
Talks SMP (mcumgr) over Web Bluetooth plus the small calibration GATT service.

- Works in Chrome on Android and on PC (Windows/macOS/Linux). iPhone Safari has
  no Web Bluetooth; use a Web-Bluetooth browser app (e.g. Bluefy).
- Must be served over HTTPS (GitHub Pages) or from `localhost`. Installs as a
  PWA and caches itself for offline use in the field.
- Local test: `python3 -m http.server 8787 --directory docs/app` then open
  http://localhost:8787/ in Chrome.

Files: `cbor.js` (CBOR), `smp.js` (generic SMP client: os / fs / settings
groups), `app.js` (SnowGauge specifics: record schema, settings schema,
calibration panel, CSV), `index.html`, `sw.js` + `manifest.webmanifest` (PWA).

Reusing for another logger: keep `cbor.js` / `smp.js`, replace the two schemas
and the calibration panel in `app.js`.
