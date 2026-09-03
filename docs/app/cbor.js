/* Minimal CBOR (RFC 8949) encoder/decoder for SMP payloads.
 * Encodes definite-length items; decodes definite and indefinite lengths
 * (Zephyr's zcbor emits indefinite-length maps unless canonical mode is on).
 */
const CBOR = (() => {
  function head(major, n, out) {
    if (n < 24) out.push((major << 5) | n);
    else if (n < 0x100) out.push((major << 5) | 24, n);
    else if (n < 0x10000) out.push((major << 5) | 25, n >> 8, n & 255);
    else out.push((major << 5) | 26, (n >>> 24) & 255, (n >>> 16) & 255, (n >>> 8) & 255, n & 255);
  }
  function enc(v, out) {
    if (v === null || v === undefined) out.push(0xf6);
    else if (v === true) out.push(0xf5);
    else if (v === false) out.push(0xf4);
    else if (typeof v === 'number') {
      if (Number.isInteger(v)) { if (v >= 0) head(0, v, out); else head(1, -1 - v, out); }
      else { const dv = new DataView(new ArrayBuffer(8)); dv.setFloat64(0, v); out.push(0xfb); for (let i = 0; i < 8; i++) out.push(dv.getUint8(i)); }
    }
    else if (typeof v === 'string') { const b = new TextEncoder().encode(v); head(3, b.length, out); for (const x of b) out.push(x); }
    else if (v instanceof Uint8Array) { head(2, v.length, out); for (const x of v) out.push(x); }
    else if (Array.isArray(v)) { head(4, v.length, out); v.forEach(x => enc(x, out)); }
    else { const keys = Object.keys(v).filter(k => v[k] !== undefined); head(5, keys.length, out); keys.forEach(k => { enc(k, out); enc(v[k], out); }); }
  }
  function encode(v) { const out = []; enc(v, out); return new Uint8Array(out); }

  function decode(bytes) {
    let i = 0;
    const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    function len(ai) {
      if (ai < 24) return ai;
      if (ai === 24) return bytes[i++];
      if (ai === 25) { const n = dv.getUint16(i); i += 2; return n; }
      if (ai === 26) { const n = dv.getUint32(i); i += 4; return n; }
      if (ai === 27) { const hi = dv.getUint32(i), lo = dv.getUint32(i + 4); i += 8; return hi * 4294967296 + lo; }
      if (ai === 31) return -1;
      throw new Error('CBOR: bad additional info ' + ai);
    }
    function concat(parts) { const n = parts.reduce((a, p) => a + p.length, 0); const o = new Uint8Array(n); let k = 0; for (const p of parts) { o.set(p, k); k += p.length; } return o; }
    function item() {
      const b = bytes[i++], major = b >> 5, ai = b & 31;
      switch (major) {
        case 0: return len(ai);
        case 1: return -1 - len(ai);
        case 2: { const n = len(ai); if (n < 0) { const p = []; while (bytes[i] !== 0xff) p.push(item()); i++; return concat(p); } const s = bytes.slice(i, i + n); i += n; return s; }
        case 3: { const n = len(ai); if (n < 0) { let s = ''; while (bytes[i] !== 0xff) s += item(); i++; return s; } const s = new TextDecoder().decode(bytes.subarray(i, i + n)); i += n; return s; }
        case 4: { const n = len(ai); const a = []; if (n < 0) { while (bytes[i] !== 0xff) a.push(item()); i++; } else for (let k = 0; k < n; k++) a.push(item()); return a; }
        case 5: { const n = len(ai); const o = {}; if (n < 0) { while (bytes[i] !== 0xff) { const k = item(); o[k] = item(); } i++; } else for (let k = 0; k < n; k++) { const key = item(); o[key] = item(); } return o; }
        case 6: { len(ai); return item(); }
        case 7: {
          if (ai === 20) return false; if (ai === 21) return true; if (ai === 22 || ai === 23) return null;
          if (ai === 24) { i++; return null; }
          if (ai === 25) { const h = dv.getUint16(i); i += 2; const s = (h & 0x8000) ? -1 : 1, e = (h >> 10) & 31, f = h & 1023; return e === 0 ? s * Math.pow(2, -14) * (f / 1024) : e === 31 ? (f ? NaN : s * Infinity) : s * Math.pow(2, e - 15) * (1 + f / 1024); }
          if (ai === 26) { const f = dv.getFloat32(i); i += 4; return f; }
          if (ai === 27) { const f = dv.getFloat64(i); i += 8; return f; }
          throw new Error('CBOR: bad simple value ' + ai);
        }
      }
    }
    return item();
  }
  return { encode, decode };
})();
if (typeof module !== 'undefined') module.exports = CBOR;
