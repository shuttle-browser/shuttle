// Push subscribe smoke (scripts/smoke-push.mjs <port>): serves a page + service worker on
// http://localhost:8765 (secure context), grants notifications via CDP, calls pushManager.subscribe
// and prints how it settles. Expect 'resolved https://...fcm/send/...' after ~30-90 s (first GCM
// checkin + registration); timeout PUSH_TIMEOUT_MS (default 90000).
import http from 'node:http';
const port = +(process.argv[2] || 9222), web = 8765;
const key = 'BEl62iUYgUivxIkv69yViEuiBIa-Ib9-SkvMeAtA3LFgDzkrxZJjSgSnfckjBJuBkr3qBUYIHBQFLXYp5Nksh8U'; // public VAPID demo key (web-push codelab), not a secret  gitleaks:allow
const srv = http.createServer((q, r) => {
  if (q.url === '/sw.js') { r.writeHead(200, {'content-type': 'text/javascript'}); r.end("self.addEventListener('push',()=>{});"); }
  else { r.writeHead(200, {'content-type': 'text/html'}); r.end('<title>push</title>'); }
}).listen(web, 'localhost');
const ver = await (await fetch(`http://127.0.0.1:${port}/json/version`)).json();
const ws = new WebSocket(ver.webSocketDebuggerUrl);
await new Promise(r => ws.onopen = r);
let id = 0; const pend = new Map();
ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id && pend.has(m.id)) { pend.get(m.id)(m); pend.delete(m.id); } };
const send = (method, params = {}, sessionId) => new Promise(r => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({id: i, method, params, sessionId})); });
const origin = `http://localhost:${web}`;
const g = await send('Browser.grantPermissions', {origin, permissions: ['notifications']});
const t = await send('Target.createTarget', {url: origin + '/'});
const a = await send('Target.attachToTarget', {targetId: t.result.targetId, flatten: true});
const s = a.result.sessionId;
await new Promise(r => setTimeout(r, 1500));
const ev = await send('Runtime.evaluate', {awaitPromise: true, returnByValue: true, expression: `(async () => {
  const to = (p, ms) => Promise.race([p, new Promise((_, j) => setTimeout(() => j(new Error('TIMEOUT')), ms))]);
  const out = {perm: Notification.permission};
  try {
    const reg = await navigator.serviceWorker.register('/sw.js'); await to(navigator.serviceWorker.ready, 5000);
    out.sw = 'ready';
    out.permState = await reg.pushManager.permissionState({userVisibleOnly: true});
    try { const sub = await to(reg.pushManager.subscribe({userVisibleOnly: true, applicationServerKey: '${key}'}), ${+(process.env.PUSH_TIMEOUT_MS || 90000)});
      out.subscribe = 'resolved ' + sub.endpoint.slice(0, 60); await sub.unsubscribe(); }
    catch (e) { out.subscribe = 'rejected ' + e.name + ': ' + e.message; }
  } catch (e) { out.error = e.name + ': ' + e.message; }
  return out; })()`}, s);
console.log('grant:', g.error ? 'ERROR ' + g.error.message : 'ok');
console.log('push:', JSON.stringify(ev.result?.result?.value ?? ev.result?.exceptionDetails ?? ev.error));
await send('Target.closeTarget', {targetId: t.result.targetId});
ws.close(); srv.close(); process.exit(0);
