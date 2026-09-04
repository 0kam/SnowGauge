/* SnowGauge Web Bluetooth page: project-specific part.
 * Generic pieces (reuse for other loggers): cbor.js, smp.js, the panels below
 * driven by RECORD_SCHEMA / SETTINGS_SCHEMA. Project-specific: the two
 * schemas, the calibration panel (custom GATT) and the derived snow depth.
 */
'use strict';

/* ---------- project schemas ---------- */

const DEVICE_NAME_PREFIX = 'SG-';
const FS_DIR = '/lfs1/';
const SITE_FILE = FS_DIR + 'site.json';

/* 40-byte record v1, see firmware/src/record.h / docs/record_format.md */
const RECORD_SCHEMA = {
  size: 40, magic: 0x5347, version: 1,
  fields: [
    ['epoch', 'u32', 4], ['seq', 'u32', 8],
    ['dist_cm', 'u16', 12, { none: 0xffff }], ['dist_var_cm2', 'u16', 14, { none: 0xffff }],
    ['strength', 'u16', 16], ['n_frames', 'u16', 18], ['n_valid', 'u16', 20], ['n_out_of_range', 'u16', 22],
    ['tilt_deg', 'i16', 24, { scale: 100 }], ['pitch_deg', 'i16', 26, { scale: 100 }], ['roll_deg', 'i16', 28, { scale: 100 }],
    ['imu_temp_c', 'i16', 30, { scale: 10 }], ['lidar_temp_c', 'i16', 32, { scale: 10 }],
    ['vbat_start_mv', 'u16', 34], ['vbat_end_mv', 'u16', 36],
  ],
  flags: { 0: 'time_synced', 1: 'time_estimated', 2: 'lidar_ok', 3: 'tilt_ok', 4: 'manual', 5: 'first_after_boot' },
};

/* Settings exposed over SMP settings mgmt (firmware/src/config.h). */
/* kind: 'hhmm' = 30-min picker, 'choice' = fixed list, 'auto' = set by the page (no UI),
 * 'display' = read-only text. Big pickers only: nothing is typed on the phone. */
const INTERVAL_CHOICES = [[0, '自動観測しない'], [30, '30 分'], [60, '1 時間'], [90, '1 時間 30 分'], [120, '2 時間'], [180, '3 時間'], [240, '4 時間'], [360, '6 時間'], [720, '12 時間'], [1440, '24 時間']];
const SETTINGS_SCHEMA = [
  { key: 'sg/sched/start_min', type: 'u16', kind: 'hhmm', label: '観測開始' },
  { key: 'sg/sched/end_min', type: 'u16', kind: 'hhmm', label: '観測終了', help: '開始と同じなら終日。開始より前なら夜をまたぐ（例 17:00 → 05:00）' },
  { key: 'sg/sched/interval_min', type: 'u16', kind: 'choice', choices: INTERVAL_CHOICES, label: '観測間隔' },
  { key: 'sg/sched/tz_min', type: 'i16', kind: 'auto', label: 'タイムゾーン' },
  { key: 'sg/cal/d0_cm', type: 'u16', kind: 'display', label: '基準斜距離 d0', unit: ' cm' },
  { key: 'sg/cal/theta0_cdeg', type: 'i16', kind: 'display', label: '基準傾斜 θ0', scale: 100, unit: '°' },
  { key: 'sg/cal/set_epoch', type: 'u32', kind: 'display', label: '基準設定日時', epoch: true },
];

/* Calibration GATT (firmware/src/cal_gatt.h) */
const CAL_SVC = '53470001-6e63-4d61-8b0c-7a8e9f0b1c2d';
const CAL_LIVE = '53470002-6e63-4d61-8b0c-7a8e9f0b1c2d';
const CAL_CTRL = '53470003-6e63-4d61-8b0c-7a8e9f0b1c2d';
const CAL_STATUS = '53470004-6e63-4d61-8b0c-7a8e9f0b1c2d';

/* CSV columns (docs/record_format.md) */
const CSV_COLUMNS = ['device_id', 'site_name', 'lat', 'lon', 'alt_m', 'tz_offset_min', 'time_utc', 'time_local', 'seq', 'flags',
  'dist_cm', 'dist_var_cm2', 'strength', 'n_frames', 'n_valid', 'n_out_of_range', 'tilt_deg', 'pitch_deg', 'roll_deg',
  'imu_temp_c', 'lidar_temp_c', 'vbat_start_mv', 'vbat_end_mv', 'd0_cm', 'theta0_deg', 'snow_depth_cm'];

/* ---------- generic helpers ---------- */

const $ = id => document.getElementById(id);
const logEl = () => $('log');
function log(msg, cls = '') {
  const line = document.createElement('div');
  line.textContent = new Date().toLocaleTimeString() + '  ' + msg;
  if (cls) line.className = cls;
  logEl().prepend(line);
  while (logEl().children.length > 200) logEl().lastChild.remove();
}
function crc16ccitt(bytes, seed = 0xffff) { // Zephyr crc16_ccitt: reflected, poly 0x8408
  let crc = seed;
  for (const b of bytes) { crc ^= b; for (let i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ 0x8408 : crc >> 1; }
  return crc;
}
function decodeRecords(bytes, schema = RECORD_SCHEMA) {
  const out = []; const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let i = 0, bad = 0;
  while (i + schema.size <= bytes.length) {
    const ok = dv.getUint16(i, true) === schema.magic && bytes[i + 2] === schema.version &&
      dv.getUint16(i + schema.size - 2, true) === crc16ccitt(bytes.subarray(i, i + schema.size - 2));
    if (!ok) { bad++; let j = i + 1; while (j + 1 < bytes.length && !(bytes[j] === 0x47 && bytes[j + 1] === 0x53)) j++; if (j + 1 >= bytes.length) break; i = j; continue; } // magic 0x5347 LE = 'G','S'
    const r = { flags_raw: bytes[i + 3], flags: [] };
    for (const [b, n] of Object.entries(schema.flags)) if (r.flags_raw & (1 << b)) r.flags.push(n);
    for (const [name, type, off, opt] of schema.fields) {
      let v = type === 'u32' ? dv.getUint32(i + off, true) : type === 'u16' ? dv.getUint16(i + off, true) : dv.getInt16(i + off, true);
      if (opt && opt.none !== undefined && v === opt.none) v = null;
      else if (type === 'i16' && v === -32768) v = null;
      else if (opt && opt.scale) v = v / opt.scale;
      r[name] = v;
    }
    out.push(r); i += schema.size;
  }
  return { records: out, bad };
}
function isoUTC(epoch) { return epoch ? new Date(epoch * 1000).toISOString().slice(0, 19) + 'Z' : ''; }
function isoLocal(epoch, tzMin) {
  if (!epoch) return '';
  const d = new Date((epoch + tzMin * 60) * 1000);
  const s = d.toISOString().slice(0, 19);
  const sign = tzMin < 0 ? '-' : '+', a = Math.abs(tzMin);
  return s + sign + String(Math.floor(a / 60)).padStart(2, '0') + ':' + String(a % 60).padStart(2, '0');
}
function hhmm(min) { return String(Math.floor(min / 60)).padStart(2, '0') + ':' + String(min % 60).padStart(2, '0'); }
function parseHHMM(s) { const m = /^(\d{1,2}):(\d{2})$/.exec(s.trim()); if (!m) return null; const v = +m[1] * 60 + +m[2]; return v < 1440 ? v : null; }
function saveBlob(name, blob) { const a = document.createElement('a'); a.href = URL.createObjectURL(blob); a.download = name; a.click(); setTimeout(() => URL.revokeObjectURL(a.href), 5000); }
/* Button feedback: disable + "処理中…" while the async action runs, toast at the end. */
function toast(msg, cls = '') {
  const t = $('toast'); t.textContent = msg; t.className = 'show ' + cls;
  clearTimeout(toast._timer); toast._timer = setTimeout(() => { t.className = ''; }, 2500);
}
function busy(btn, fn) {
  return async () => {
    if (btn.disabled) return;
    const label = btn.textContent; btn.disabled = true; btn.textContent = '処理中…';
    try { const r = await fn(); if (r !== false) toast('完了'); }
    catch (e) { log(e.message, 'err'); toast('失敗: ' + e.message, 'err'); }
    finally { btn.textContent = label; btn.disabled = !state.smp || !state.smp.connected ? btn.classList.contains('needs-conn') : false; }
  };
}
function csvEscape(v) { if (v === null || v === undefined) return ''; const s = String(v); return /[",\n]/.test(s) ? '"' + s.replace(/"/g, '""') + '"' : s; }

/* ---------- state ---------- */

const state = { device: null, smp: null, cal: null, site: null, settings: {}, records: [], rawFiles: {}, deviceId: '' };

/* ---------- connection ---------- */

async function connect() {
  const b = $('btn-connect'); b.disabled = true; b.textContent = '接続中…';
  try {
    const device = await navigator.bluetooth.requestDevice({
      filters: [{ namePrefix: DEVICE_NAME_PREFIX }],
      optionalServices: [SMP_SERVICE, CAL_SVC],
    });
    state.device = device; state.deviceId = device.name || '';
    device.addEventListener('gattserverdisconnected', () => setConnected(false));
    state.smp = new SMPClient(log);
    await state.smp.connect(device);
    setConnected(true);
    log('接続: ' + device.name); toast('接続しました');
    await refreshStatus();
  } catch (e) { log('接続失敗: ' + e.message, 'err'); toast('接続失敗', 'err'); }
  b.disabled = false; b.textContent = '接続';
}
function setConnected(on) {
  $('btn-connect').hidden = on; $('btn-disconnect').hidden = !on;
  $('conn-state').textContent = on ? '接続中: ' + state.deviceId : '未接続';
  document.querySelectorAll('.needs-conn').forEach(el => el.disabled = !on);
  if (!on) { state.cal = null; $('live-state').textContent = '停止'; }
}
async function refreshStatus() {
  try {
    const dt = await state.smp.os.getDateTime();
    const dev = new Date(dt + 'Z'), diff = Math.round((dev - Date.now()) / 1000);
    $('dev-time').textContent = `${dt}Z（スマホとの差 ${diff >= 0 ? '+' : ''}${diff} 秒）`;
    $('dev-time').className = Math.abs(diff) > 60 ? 'warn' : 'ok';
  } catch (e) { $('dev-time').textContent = '時刻未設定または取得失敗 (' + e.message + ')'; $('dev-time').className = 'warn'; }
  await loadSettings();
  await loadSite();
}

/* ---------- sync: time + tz + location ---------- */

async function syncTime() {
  const iso = await state.smp.os.setDateTime(new Date());
  log('時刻設定: ' + iso + 'Z');
}
function getPosition() {
  return new Promise((resolve, reject) => {
    if (!navigator.geolocation) return reject(new Error('位置情報 API なし'));
    navigator.geolocation.getCurrentPosition(p => resolve(p.coords), e => reject(new Error(e.message)), { enableHighAccuracy: true, timeout: 20000, maximumAge: 60000 });
  });
}
function phoneTzMin() { return -new Date().getTimezoneOffset(); }
async function loadSite() {
  try {
    const bytes = await state.smp.fs.download(SITE_FILE);
    state.site = JSON.parse(new TextDecoder().decode(bytes));
  } catch (e) { state.site = null; }
  renderSite();
}
function renderSite() {
  const s = state.site;
  $('site-info').textContent = s ? `${s.name || '(名称なし)'}  lat ${s.lat} lon ${s.lon} alt ${s.alt_m ?? '-'} m (±${s.accuracy_m ?? '-'} m, ${s.source})  tz ${s.tz_min} min  設定 ${s.set_at}` : 'サイト情報なし';
}
async function syncAll() {
  const problems = [];
  try { await syncTime(); } catch (e) { problems.push('時刻: ' + e.message); }
  let coords = null;
  try { coords = await getPosition(); log(`位置取得: ${coords.latitude.toFixed(6)}, ${coords.longitude.toFixed(6)} ±${Math.round(coords.accuracy)} m`); }
  catch (e) { problems.push('位置: ' + e.message + '（手動入力可）'); }
  const tz = phoneTzMin();
  try {
    if (coords) await writeSite({ lat: +coords.latitude.toFixed(6), lon: +coords.longitude.toFixed(6), alt_m: coords.altitude == null ? null : Math.round(coords.altitude), accuracy_m: Math.round(coords.accuracy), source: 'gps' }, tz);
    else await writeTz(tz);
  } catch (e) { problems.push('サイト/タイムゾーン: ' + e.message); }
  await refreshStatus();
  if (problems.length) { problems.forEach(m => log(m, 'warn')); throw new Error(problems.length + ' 件の問題（ログ参照）'); }
  log('同期完了');
}
async function writeTz(tz) {
  await state.smp.settings.write('sg/sched/tz_min', LE.i16(tz));
  await state.smp.settings.save();
  log('タイムゾーン設定: ' + tz + ' 分');
}
async function writeSite(pos, tz) {
  const prev = state.site;
  let tzErr = null;
  const entry = { ...pos, name: $('site-name').value.trim() || (prev && prev.name) || '', tz_min: tz, tz_name: Intl.DateTimeFormat().resolvedOptions().timeZone, set_at: new Date().toISOString(), device_id: state.deviceId };
  const history = (prev && prev.history) ? prev.history.slice(-20) : [];
  if (prev) { const { history: _h, ...old } = prev; history.push(old); }
  const site = { ...entry, history };
  await state.smp.fs.upload(SITE_FILE, new TextEncoder().encode(JSON.stringify(site)));
  state.site = site; renderSite();
  log('サイト情報を保存');
  try { await writeTz(tz); } catch (e) { tzErr = e; }
  if (tzErr) throw new Error('タイムゾーン設定失敗: ' + tzErr.message);
}
async function manualSite() {
  const lat = parseFloat($('m-lat').value), lon = parseFloat($('m-lon').value), alt = parseFloat($('m-alt').value);
  if (isNaN(lat) || isNaN(lon)) throw new Error('緯度経度を入力してください');
  await writeSite({ lat, lon, alt_m: isNaN(alt) ? null : alt, accuracy_m: null, source: 'manual' }, phoneTzMin());
  await refreshStatus();
}

/* ---------- settings ---------- */

function settingValue(def, bytes) { return def.type === 'u32' ? LE.getU32(bytes) : def.type === 'i16' ? LE.getI16(bytes) : LE.getU16(bytes); }
function settingDisplay(def, v) {
  if (def.epoch) return v ? isoLocal(v, tzMin()).slice(0, 16).replace('T', ' ') : '未設定';
  if (def.scale) return (v / def.scale).toFixed(2) + (def.unit || '');
  return (v || v === 0 ? v : '-') + (def.unit || '');
}
function settingBytes(def, v) { return def.type === 'u32' ? LE.u32(v) : def.type === 'i16' ? LE.i16(v) : LE.u16(v); }
function setSelectValue(sel, v, labelFn) {
  if (![...sel.options].some(o => +o.value === v)) { const o = document.createElement('option'); o.value = v; o.textContent = labelFn(v); sel.appendChild(o); }
  sel.value = String(v);
}
function renderSettingsForm() {
  const tb = $('settings-body'); tb.innerHTML = '';
  for (const def of SETTINGS_SCHEMA) {
    if (def.kind === 'auto') continue;
    const tr = document.createElement('tr');
    let ctl;
    if (def.kind === 'hhmm') {
      ctl = `<select id="set-${def.key}" class="needs-conn big" disabled>` + Array.from({ length: 48 }, (_, i) => `<option value="${i * 30}">${hhmm(i * 30)}</option>`).join('') + '</select>';
    } else if (def.kind === 'choice') {
      ctl = `<select id="set-${def.key}" class="needs-conn big" disabled>` + def.choices.map(([v, l]) => `<option value="${v}">${l}</option>`).join('') + '</select>';
    } else {
      ctl = `<span id="set-${def.key}" class="value">-</span>`;
    }
    tr.innerHTML = `<td>${def.label}<div class="help">${def.help || ''}</div></td><td>${ctl}</td>`;
    tb.appendChild(tr);
  }
}
async function loadSettings() {
  for (const def of SETTINGS_SCHEMA) {
    try {
      const bytes = await state.smp.settings.read(def.key);
      state.settings[def.key] = bytes;
      const v = settingValue(def, bytes);
      if (def.kind === 'hhmm') setSelectValue($('set-' + def.key), v, hhmm);
      else if (def.kind === 'choice') setSelectValue($('set-' + def.key), v, x => x + ' 分');
      else if (def.kind === 'display') $('set-' + def.key).textContent = settingDisplay(def, v);
    } catch (e) {
      log('設定読込失敗 ' + def.key + ': ' + e.message + (e.rc === 8 ? '（本体 FW が設定グループ未対応: 4d 版が必要）' : ''), 'warn');
      if (e.rc === 8) break;
    }
  }
  const tz = tzMin(); $('tz-info').textContent = `UTC${tz >= 0 ? '+' : '-'}${hhmm(Math.abs(tz))}（同期時にスマホから自動設定）`;
  $('cal-ref').textContent = calRef() ? `d0 = ${calRef().d0} cm, θ0 = ${calRef().theta0.toFixed(2)}°` : '未設定（無雪 ZERO か積雪深入力で設定）';
}
async function saveSettings() {
  for (const def of SETTINGS_SCHEMA) {
    if (def.kind !== 'hhmm' && def.kind !== 'choice') continue;
    const bytes = settingBytes(def, parseInt($('set-' + def.key).value, 10));
    await state.smp.settings.write(def.key, bytes);
  }
  await state.smp.settings.save();
  log('設定を保存');
  await loadSettings();
}
function tzMin() { const b = state.settings['sg/sched/tz_min']; return b ? LE.getI16(b) : phoneTzMin(); }
function calRef() {
  const d0 = state.settings['sg/cal/d0_cm'] ? LE.getU16(state.settings['sg/cal/d0_cm']) : 0;
  const th = state.settings['sg/cal/theta0_cdeg'] ? LE.getI16(state.settings['sg/cal/theta0_cdeg']) / 100 : 0;
  return d0 ? { d0, theta0: th } : null;
}

/* ---------- data ---------- */

async function listRecordFiles() {
  const names = [];
  const now = new Date(); let y = now.getUTCFullYear(), m = now.getUTCMonth() + 1, missing = 0;
  for (let k = 0; k < 36 && missing < 3; k++) {
    const name = `${FS_DIR}rec_${y}${String(m).padStart(2, '0')}.bin`;
    try { const st = await state.smp.fs.stat(name); names.push({ name, len: st.len }); missing = 0; }
    catch (e) { missing++; }
    if (--m === 0) { m = 12; y--; }
  }
  try { const st = await state.smp.fs.stat(FS_DIR + 'rec_notime.bin'); names.push({ name: FS_DIR + 'rec_notime.bin', len: st.len }); } catch (e) { /* none */ }
  return names;
}
async function downloadAll() {
  {
    const files = await listRecordFiles();
    log(`ファイル ${files.length} 件: ` + files.map(f => `${f.name.split('/').pop()} (${f.len} B)`).join(', '));
    state.records = []; state.rawFiles = {};
    for (const f of files) {
      const t0 = Date.now();
      const bytes = await state.smp.fs.download(f.name, (o, t) => { $('dl-progress').textContent = `${f.name.split('/').pop()}: ${o}/${t} B`; });
      state.rawFiles[f.name.split('/').pop()] = bytes;
      const { records, bad } = decodeRecords(bytes);
      if (bad) log(`${f.name}: 不正レコード ${bad} 件をスキップ`, 'warn');
      state.records.push(...records);
      log(`${f.name.split('/').pop()}: ${records.length} 件 (${((Date.now() - t0) / 1000).toFixed(1)} 秒)`);
    }
    state.records.sort((a, b) => (a.epoch - b.epoch) || (a.seq - b.seq));
    $('dl-progress').textContent = `合計 ${state.records.length} 件`;
    renderData();
  }
}
function qualityFlags(r, intervalMin, prev) {
  const q = [];
  if (r.dist_cm === null) q.push('測距なし');
  if (r.n_frames && r.n_valid < r.n_frames * 0.5) q.push('有効率低');
  if (r.dist_var_cm2 !== null && r.dist_var_cm2 > 4) q.push('ばらつき大');
  if (r.n_frames && r.n_out_of_range > r.n_frames * 0.5) q.push('レンジ外多');
  if (!r.flags.includes('time_synced')) q.push(r.flags.includes('time_estimated') ? '時刻推定' : '時刻なし');
  if (prev && intervalMin && r.epoch && prev.epoch && (r.epoch - prev.epoch) > intervalMin * 60 * 1.5 && !r.flags.includes('manual')) q.push('欠測あり');
  if (r.vbat_end_mv && r.vbat_end_mv < 4400) q.push('電池低');
  return q;
}
function derived(r) {
  const c = calRef();
  if (!c || r.dist_cm === null || r.tilt_deg === null) return { d0: c ? c.d0 : null, theta0: c ? c.theta0 : null, depth: null };
  return { d0: c.d0, theta0: c.theta0, depth: +((c.d0 - r.dist_cm) * Math.cos(r.tilt_deg * Math.PI / 180)).toFixed(1) };
}
function renderData() {
  const tz = tzMin();
  const iv = state.settings['sg/sched/interval_min'] ? LE.getU16(state.settings['sg/sched/interval_min']) : 0;
  const tb = $('data-body'); tb.innerHTML = '';
  let prev = null, nWarn = 0;
  const rows = state.records.slice(-500);
  for (const r of rows) {
    const q = qualityFlags(r, iv, prev); if (q.length) nWarn++;
    const d = derived(r);
    const tr = document.createElement('tr'); if (q.length) tr.className = 'warnrow';
    tr.innerHTML = `<td>${isoLocal(r.epoch, tz) || '(時刻なし) #' + r.seq}</td><td>${r.dist_cm ?? '-'}</td><td>${d.depth ?? '-'}</td><td>${r.tilt_deg ?? '-'}</td><td>${r.vbat_end_mv ?? '-'}</td><td>${r.n_valid}/${r.n_frames}</td><td>${q.join(' ')}</td>`;
    tb.appendChild(tr); prev = r;
  }
  $('data-summary').textContent = `${state.records.length} 件（表示は最新 ${rows.length} 件）、要確認 ${nWarn} 件`;
  drawChart();
  $('btn-csv').disabled = $('btn-raw').disabled = state.records.length === 0;
}
function drawChart() {
  const cv = $('chart'), ctx = cv.getContext('2d');
  const W = cv.width = cv.clientWidth * devicePixelRatio, H = cv.height = 220 * devicePixelRatio;
  ctx.clearRect(0, 0, W, H);
  const rs = state.records.filter(r => r.epoch && r.dist_cm !== null);
  if (rs.length < 2) return;
  const t0 = rs[0].epoch, t1 = rs[rs.length - 1].epoch || t0 + 1;
  const pad = 40 * devicePixelRatio;
  const c = calRef();
  const ys = rs.map(r => c ? derived(r).depth : r.dist_cm);
  const ymin = Math.min(...ys), ymax = Math.max(...ys), yr = (ymax - ymin) || 1;
  const X = e => pad + (e - t0) / (t1 - t0) * (W - 2 * pad), Y = v => H - pad - (v - ymin) / yr * (H - 2 * pad);
  ctx.font = `${12 * devicePixelRatio}px sans-serif`; ctx.fillStyle = '#666';
  ctx.fillText(c ? '積雪深 cm' : '斜距離 cm', 4, 14 * devicePixelRatio);
  ctx.fillText(String(ymax), 4, Y(ymax) + 4); ctx.fillText(String(ymin), 4, Y(ymin) + 4);
  ctx.strokeStyle = '#1976d2'; ctx.lineWidth = 2 * devicePixelRatio; ctx.beginPath();
  rs.forEach((r, i) => { const x = X(r.epoch), y = Y(ys[i]); i ? ctx.lineTo(x, y) : ctx.moveTo(x, y); });
  ctx.stroke();
  ctx.fillStyle = '#d32f2f';
  const iv = state.settings['sg/sched/interval_min'] ? LE.getU16(state.settings['sg/sched/interval_min']) : 0;
  rs.forEach((r, i) => { if (i && iv && r.epoch - rs[i - 1].epoch > iv * 90) ctx.fillRect(X(rs[i - 1].epoch), pad, Math.max(2, X(r.epoch) - X(rs[i - 1].epoch)), 4 * devicePixelRatio); });
  ctx.fillStyle = '#666';
  ctx.fillText(isoLocal(t0, tzMin()).slice(0, 16), pad, H - 8); ctx.fillText(isoLocal(t1, tzMin()).slice(0, 16), W - pad - 130 * devicePixelRatio, H - 8);
}
function exportCSV() {
  const tz = tzMin(), s = state.site || {};
  const lines = [CSV_COLUMNS.join(',')];
  for (const r of state.records) {
    const d = derived(r);
    const row = { device_id: state.deviceId, site_name: s.name ?? '', lat: s.lat ?? '', lon: s.lon ?? '', alt_m: s.alt_m ?? '', tz_offset_min: tz,
      time_utc: isoUTC(r.epoch), time_local: isoLocal(r.epoch, tz), seq: r.seq, flags: r.flags.join('|'),
      dist_cm: r.dist_cm, dist_var_cm2: r.dist_var_cm2, strength: r.strength, n_frames: r.n_frames, n_valid: r.n_valid, n_out_of_range: r.n_out_of_range,
      tilt_deg: r.tilt_deg, pitch_deg: r.pitch_deg, roll_deg: r.roll_deg, imu_temp_c: r.imu_temp_c, lidar_temp_c: r.lidar_temp_c,
      vbat_start_mv: r.vbat_start_mv, vbat_end_mv: r.vbat_end_mv, d0_cm: d.d0, theta0_deg: d.theta0, snow_depth_cm: d.depth };
    lines.push(CSV_COLUMNS.map(c => csvEscape(row[c])).join(','));
  }
  const stamp = new Date().toISOString().slice(0, 19).replace(/[:T]/g, '-');
  saveBlob(`${state.deviceId || 'snowgauge'}_${stamp}.csv`, new Blob(['\ufeff' + lines.join('\n') + '\n'], { type: 'text/csv;charset=utf-8' }));
  log('CSV 保存: ' + state.records.length + ' 件');
}
function exportRaw() {
  for (const [name, bytes] of Object.entries(state.rawFiles)) saveBlob(`${state.deviceId || 'snowgauge'}_${name}`, new Blob([bytes], { type: 'application/octet-stream' }));
  if (state.site) saveBlob(`${state.deviceId || 'snowgauge'}_site.json`, new Blob([JSON.stringify(state.site, null, 2)], { type: 'application/json' }));
}

/* ---------- calibration (custom GATT) ---------- */

async function calConnect() {
  if (state.cal) return state.cal;
  let svc;
  try { svc = await state.device.gatt.getPrimaryService(CAL_SVC); }
  catch (e) { throw new Error('キャリブレーション用サービスが本体にありません（FW 4d 版が必要）'); }
  const live = await svc.getCharacteristic(CAL_LIVE);
  const ctrl = await svc.getCharacteristic(CAL_CTRL);
  const status = await svc.getCharacteristic(CAL_STATUS);
  live.addEventListener('characteristicvaluechanged', ev => {
    const dv = ev.target.value;
    const dist = dv.getUint16(0, true), str = dv.getUint16(2, true), tilt = dv.getInt16(4, true) / 100, vbat = dv.getUint16(6, true), nv = dv.getUint8(8), nf = dv.getUint8(9), v = dv.getUint16(10, true);
    $('live-dist').textContent = dist === 0xffff ? '---' : dist;
    $('live-tilt').textContent = tilt.toFixed(2); $('live-str').textContent = str; $('live-vbat').textContent = vbat; $('live-q').textContent = `${nv}/${nf}, var ${v}`;
    const c = calRef();
    $('live-depth').textContent = (c && dist !== 0xffff) ? ((c.d0 - dist) * Math.cos(tilt * Math.PI / 180)).toFixed(1) : '---';
  });
  await live.startNotifications();
  state.cal = { live, ctrl, status };
  return state.cal;
}
async function calCmd(bytes) { const c = await calConnect(); await c.ctrl.writeValue(new Uint8Array(bytes)); }
async function calStatus() {
  const c = await calConnect(); const dv = await c.status.readValue();
  const d0 = dv.getUint16(0, true), th = dv.getInt16(2, true) / 100, ep = dv.getUint32(4, true), live = dv.getUint8(8), res = dv.getInt8(9);
  $('live-state').textContent = live ? '動作中（センサ通電中、5 分で自動停止）' : '停止';
  $('cal-ref').textContent = d0 ? `d0 = ${d0} cm, θ0 = ${th.toFixed(2)}°` : '未設定（無雪 ZERO か積雪深入力で設定）';
  if (res) log('前回コマンド結果: ' + res, 'warn');
  return { d0, th, live, res };
}
async function liveOn() { await calCmd([0x01]); await calStatus(); }
async function liveOff() { await calCmd([0x00]); await calStatus(); }
/* Reference: ZERO on bare ground (depth 0) or from a probed snow depth. */
async function setReference(depthCm) {
  await calCmd(depthCm ? [0x11, depthCm & 255, depthCm >> 8] : [0x10]);
  log(depthCm ? `基準設定中（現在の積雪深 ${depthCm} cm、約 3 秒）` : 'ZERO 実行中（約 3 秒）');
  await new Promise(r => setTimeout(r, 4000));
  const st = await calStatus(); await loadSettings();
  if (st.res) throw new Error('本体エラー ' + st.res + '（測距か傾斜が取れていません）');
  log('基準を保存しました');
}
function twoTap(btn, label, action) {
  if (btn.dataset.armed) { delete btn.dataset.armed; btn.textContent = label; action(); return; }
  btn.dataset.armed = '1'; btn.textContent = 'もう一度押すと実行';
  setTimeout(() => { if (btn.dataset.armed) { delete btn.dataset.armed; btn.textContent = label; } }, 5000);
}
async function doErase() {
  await calCmd([0x20, 0x45, 0x52, 0x41, 0x53, 0x45]); await new Promise(r => setTimeout(r, 2000)); await calStatus();
  log('全レコードを消去しました', 'warn'); state.records = []; renderData();
}

/* ---------- init ---------- */

if (typeof window !== 'undefined') window.addEventListener('load', () => {
  renderSettingsForm();
  if (!navigator.bluetooth) { $('nosupport').hidden = false; $('btn-connect').disabled = true; }
  $('btn-connect').onclick = connect;
  $('btn-disconnect').onclick = () => state.smp && state.smp.disconnect();
  $('btn-sync').onclick = busy($('btn-sync'), syncAll);
  $('btn-time').onclick = busy($('btn-time'), async () => { await syncTime(); await refreshStatus(); });
  $('btn-site-manual').onclick = busy($('btn-site-manual'), manualSite);
  $('btn-settings-save').onclick = busy($('btn-settings-save'), saveSettings);
  $('btn-settings-reload').onclick = busy($('btn-settings-reload'), loadSettings);
  $('btn-download').onclick = busy($('btn-download'), downloadAll);
  $('btn-csv').onclick = () => { exportCSV(); toast('CSV を保存しました'); };
  $('btn-raw').onclick = () => { exportRaw(); toast('生データを保存しました'); };
  $('btn-live-on').onclick = busy($('btn-live-on'), liveOn); $('btn-live-off').onclick = busy($('btn-live-off'), liveOff);
  $('btn-cal-status').onclick = busy($('btn-cal-status'), calStatus);
  $('btn-zero').onclick = () => twoTap($('btn-zero'), 'ZERO（無雪の地面）', busy($('btn-zero'), () => setReference(0)));
  $('btn-ref-depth').onclick = () => twoTap($('btn-ref-depth'), '積雪深で基準設定', busy($('btn-ref-depth'), () => setReference(parseInt($('depth-sel').value, 10))));
  $('btn-erase').onclick = () => twoTap($('btn-erase'), '全レコード消去', busy($('btn-erase'), doErase));
  $('depth-sel').innerHTML = Array.from({ length: 401 }, (_, i) => `<option value="${i}">${i} cm</option>`).join('');
  window.addEventListener('resize', () => state.records.length && drawChart());
  if ('serviceWorker' in navigator && location.protocol === 'https:') navigator.serviceWorker.register('sw.js').catch(() => {});
});
if (typeof module !== 'undefined') module.exports = { decodeRecords, crc16ccitt, isoLocal, RECORD_SCHEMA, CSV_COLUMNS };
