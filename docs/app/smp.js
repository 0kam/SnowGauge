/* SMP (mcumgr) client over Web Bluetooth. Generic: reuse as-is for any
 * Zephyr device with CONFIG_MCUMGR_TRANSPORT_BT.
 *
 *   const smp = new SMPClient(log);
 *   await smp.connect(device);            // BluetoothDevice from requestDevice()
 *   await smp.os.echo('hi');
 *   await smp.os.setDateTime(new Date());
 *   const bytes = await smp.fs.download('/lfs1/rec_202612.bin', progressCb);
 *   await smp.settings.write('sg/sched/interval_min', u16le(90)); await smp.settings.save();
 */
const SMP_SERVICE = '8d53dc1d-1db7-4cd3-868b-8a527460aa84';
const SMP_CHAR = 'da2e7828-fbce-4e01-ae9e-261174997c48';

const SMP_OP = { READ: 0, READ_RSP: 1, WRITE: 2, WRITE_RSP: 3 };
const SMP_GROUP = { OS: 0, IMAGE: 1, STAT: 2, SETTINGS: 3, FS: 8 };
const SMP_ERR = { 0: 'OK', 1: 'EUNKNOWN', 2: 'ENOMEM', 3: 'EINVAL', 4: 'ETIMEOUT', 5: 'ENOENT', 6: 'EBADSTATE', 7: 'EMSGSIZE', 8: 'ENOTSUP', 9: 'ECORRUPT', 10: 'EBUSY', 11: 'EACCESSDENIED', 12: 'UNSUPPORTED_TOO_OLD', 13: 'UNSUPPORTED_TOO_NEW' };

class SMPError extends Error {
  constructor(rc, group) { super(`SMP error ${rc}${SMP_ERR[rc] ? ' (' + SMP_ERR[rc] + ')' : ''}${group !== undefined ? ' group ' + group : ''}`); this.rc = rc; this.group = group; }
}

class SMPClient {
  constructor(log = () => {}) {
    this.log = log;
    this.seq = 0;
    this.pending = new Map();
    this.rx = new Uint8Array(0);
    this.writeChunk = 20;     // safe for any ATT MTU; raise after mcumgrParams() if desired
    this.timeoutMs = 5000;
    this._queue = Promise.resolve(); // requests are strictly serialized (one SMP frame in flight)
    this.os = new OSGroup(this);
    this.fs = new FSGroup(this);
    this.settings = new SettingsGroup(this);
  }

  async connect(device) {
    this.device = device;
    device.addEventListener('gattserverdisconnected', () => { this.log('SMP: disconnected'); this._failAll(new Error('disconnected')); });
    const server = await device.gatt.connect();
    const svc = await server.getPrimaryService(SMP_SERVICE);
    this.chr = await svc.getCharacteristic(SMP_CHAR);
    this.chr.addEventListener('characteristicvaluechanged', ev => this._onNotify(new Uint8Array(ev.target.value.buffer)));
    await this.chr.startNotifications();
    this.log('SMP: connected');
  }

  get connected() { return !!(this.device && this.device.gatt.connected); }

  disconnect() { if (this.device && this.device.gatt.connected) this.device.gatt.disconnect(); }

  _failAll(err) { for (const p of this.pending.values()) p.reject(err); this.pending.clear(); this.rx = new Uint8Array(0); }

  _onNotify(chunk) {
    const merged = new Uint8Array(this.rx.length + chunk.length);
    merged.set(this.rx); merged.set(chunk, this.rx.length);
    this.rx = merged;
    while (this.rx.length >= 8) {
      const op = this.rx[0] & 7, len = (this.rx[2] << 8) | this.rx[3];
      if ((op !== 1 && op !== 3) || len > 4096) { this.log('SMP: bad header, dropping buffer'); this.rx = new Uint8Array(0); break; }
      if (this.rx.length < 8 + len) break;
      const frame = this.rx.slice(0, 8 + len);
      this.rx = this.rx.slice(8 + len);
      const seq = frame[6];
      const p = this.pending.get(seq);
      if (!p) { this.log('SMP: unexpected seq ' + seq); continue; }
      this.pending.delete(seq);
      clearTimeout(p.timer);
      try {
        const body = len ? CBOR.decode(frame.subarray(8)) : {};
        if (body && typeof body === 'object' && 'rc' in body && body.rc !== 0) p.reject(new SMPError(body.rc));
        else if (body && body.err && body.err.rc) p.reject(new SMPError(body.err.rc, body.err.group));
        else p.resolve(body);
      } catch (e) { p.reject(e); }
    }
  }

  request(op, group, id, payload = {}, timeoutMs = this.timeoutMs) {
    const run = () => this._request(op, group, id, payload, timeoutMs);
    const p = this._queue.then(run, run);
    this._queue = p.catch(() => {});
    return p;
  }

  async _request(op, group, id, payload, timeoutMs) {
    if (!this.chr) throw new Error('not connected');
    const body = CBOR.encode(payload);
    const seq = this.seq = (this.seq + 1) & 0xff;
    const frame = new Uint8Array(8 + body.length);
    frame[0] = op; frame[1] = 0; frame[2] = body.length >> 8; frame[3] = body.length & 255;
    frame[4] = group >> 8; frame[5] = group & 255; frame[6] = seq; frame[7] = id;
    frame.set(body, 8);
    const promise = new Promise((resolve, reject) => {
      const timer = setTimeout(() => { this.pending.delete(seq); this.rx = new Uint8Array(0); reject(new Error(`SMP timeout (group ${group} id ${id})`)); }, timeoutMs);
      this.pending.set(seq, { resolve, reject, timer });
    });
    for (let off = 0; off < frame.length; off += this.writeChunk) {
      await this.chr.writeValueWithoutResponse(frame.subarray(off, Math.min(off + this.writeChunk, frame.length)));
    }
    return promise;
  }
}

class OSGroup {
  constructor(c) { this.c = c; }
  async echo(text) { return (await this.c.request(SMP_OP.WRITE, SMP_GROUP.OS, 0, { d: text })).r; }
  /* Device time as ISO string "YYYY-MM-DDTHH:MM:SS" (UTC on SnowGauge). */
  async getDateTime() { return (await this.c.request(SMP_OP.READ, SMP_GROUP.OS, 4)).datetime; }
  async setDateTime(date) {
    const iso = date.toISOString().slice(0, 19);
    await this.c.request(SMP_OP.WRITE, SMP_GROUP.OS, 4, { datetime: iso });
    return iso;
  }
  async reset() { return this.c.request(SMP_OP.WRITE, SMP_GROUP.OS, 5, {}); }
  async mcumgrParams() { return this.c.request(SMP_OP.READ, SMP_GROUP.OS, 6); }
}

class FSGroup {
  constructor(c) { this.c = c; }
  /* Returns {len} or throws SMPError(ENOENT). */
  async stat(name) { return this.c.request(SMP_OP.READ, SMP_GROUP.FS, 1, { name }); }
  async download(name, onProgress) {
    const parts = []; let off = 0, total = null;
    while (total === null || off < total) {
      const r = await this.c.request(SMP_OP.READ, SMP_GROUP.FS, 0, { name, off }, 10000);
      if (total === null) total = r.len;
      const data = r.data || new Uint8Array(0);
      if (data.length === 0 && off < total) throw new Error('empty chunk at ' + off);
      parts.push(data); off += data.length;
      if (onProgress) onProgress(off, total);
    }
    const out = new Uint8Array(off); let k = 0; for (const p of parts) { out.set(p, k); k += p.length; }
    return out;
  }
  async upload(name, bytes, onProgress, chunk = 128) {
    let off = 0;
    while (off < bytes.length || (off === 0 && bytes.length === 0)) {
      const data = bytes.subarray(off, Math.min(off + chunk, bytes.length));
      const payload = { name, off, data };
      if (off === 0) payload.len = bytes.length;
      const r = await this.c.request(SMP_OP.WRITE, SMP_GROUP.FS, 0, payload, 10000);
      off = r.off !== undefined ? r.off : off + data.length;
      if (onProgress) onProgress(off, bytes.length);
      if (bytes.length === 0) break;
    }
  }
}

class SettingsGroup {
  constructor(c) { this.c = c; }
  /* Raw bytes of a setting. */
  async read(name) { return (await this.c.request(SMP_OP.READ, SMP_GROUP.SETTINGS, 0, { name })).val; }
  async write(name, bytes) { return this.c.request(SMP_OP.WRITE, SMP_GROUP.SETTINGS, 0, { name, val: bytes }); }
  async save() { return this.c.request(SMP_OP.WRITE, SMP_GROUP.SETTINGS, 3, {}); }
  async load() { return this.c.request(SMP_OP.READ, SMP_GROUP.SETTINGS, 3, {}); }
}

/* Little-endian helpers for settings values. */
const LE = {
  u16: v => new Uint8Array([v & 255, (v >> 8) & 255]),
  i16: v => LE.u16(v < 0 ? v + 0x10000 : v),
  u32: v => new Uint8Array([v & 255, (v >>> 8) & 255, (v >>> 16) & 255, (v >>> 24) & 255]),
  getU16: b => b[0] | (b[1] << 8),
  getI16: b => { const v = b[0] | (b[1] << 8); return v & 0x8000 ? v - 0x10000 : v; },
  getU32: b => (b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)) >>> 0,
};
if (typeof module !== 'undefined') module.exports = { SMPClient, SMPError, SMP_OP, SMP_GROUP, LE };
