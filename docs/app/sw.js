/* Offline-first: the app shell is served from the cache immediately (no
 * network wait on a weak signal); a background fetch refreshes the cache and
 * the next start uses the new version. Only successful responses are cached.
 */
const CACHE = 'snowgauge-app-v6';
const FILES = ['./', './index.html', './app.js', './smp.js', './cbor.js', './manifest.webmanifest', './icon-192.png', './icon-512.png', './apple-touch-icon.png', './favicon.png', './logo_dark.png'];
self.addEventListener('install', e => e.waitUntil(caches.open(CACHE).then(c => c.addAll(FILES)).then(() => self.skipWaiting())));
self.addEventListener('activate', e => e.waitUntil(caches.keys().then(ks => Promise.all(ks.filter(k => k !== CACHE).map(k => caches.delete(k)))).then(() => self.clients.claim())));
self.addEventListener('fetch', e => {
  if (e.request.method !== 'GET') return;
  e.respondWith(caches.match(e.request).then(cached => {
    const refresh = fetch(e.request).then(r => { if (r.ok) caches.open(CACHE).then(c => c.put(e.request, r.clone())); return r; }).catch(() => cached);
    return cached || refresh;
  }));
});
