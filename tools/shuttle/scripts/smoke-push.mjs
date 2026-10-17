// Push subscribe smoke (opt-in, needs the internet: skipped unless SHUTTLE_SMOKE_NET=1).
// Usage: node scripts/smoke-push.mjs <port>   (scripts/smoke-all.sh --with push)
// Serves a page + service worker on http://localhost:<ephemeral> (secure context), grants notifications via CDP and
// calls pushManager.subscribe: pass = resolves with an endpoint (first GCM checkin + registration, ~30-90 s;
// PUSH_TIMEOUT_MS, default 90000). Rejection or timeout fails.
import http from 'node:http';
import { browserWs, cdp, check, cleanup, finish, skip, sleep } from './lib/smoke.mjs';

if (process.env.SHUTTLE_SMOKE_NET !== '1') await skip('needs the push service (set SHUTTLE_SMOKE_NET=1)');
const key = 'BEl62iUYgUivxIkv69yViEuiBIa-Ib9-SkvMeAtA3LFgDzkrxZJjSgSnfckjBJuBkr3qBUYIHBQFLXYp5Nksh8U'; // public VAPID demo key (web-push codelab), not a secret  gitleaks:allow
const srv = http.createServer((q, r) => {
  if (q.url === '/sw.js') { r.writeHead(200, {'content-type': 'text/javascript'}); r.end("self.addEventListener('push',()=>{});"); }
  else { r.writeHead(200, {'content-type': 'text/html'}); r.end('<title>push</title>'); }
});
await new Promise((r) => srv.listen(0, 'localhost', r));
cleanup(() => srv.close());
const origin = `http://localhost:${srv.address().port}`;
const b = await cdp(await browserWs());
const g = await b.send('Browser.grantPermissions', {origin, permissions: ['notifications']});
check('grant notifications', !g.error, g.error?.message);
const {targetId} = (await b.send('Target.createTarget', {url: origin + '/'})).result;
cleanup(() => b.send('Target.closeTarget', {targetId}));
const s = (await b.send('Target.attachToTarget', {targetId, flatten: true})).result.sessionId;
await sleep(1500);
const ms = +(process.env.PUSH_TIMEOUT_MS || 90000);
const r = await b.send('Runtime.evaluate', {awaitPromise: true, returnByValue: true, expression: `(async () => {
  const to = (p, ms) => Promise.race([p, new Promise((_, j) => setTimeout(() => j(new Error('TIMEOUT')), ms))]);
  const out = {perm: Notification.permission};
  try {
    const reg = await navigator.serviceWorker.register('/sw.js'); await to(navigator.serviceWorker.ready, 5000);
    out.sw = 'ready';
    out.permState = await reg.pushManager.permissionState({userVisibleOnly: true});
    try { const sub = await to(reg.pushManager.subscribe({userVisibleOnly: true, applicationServerKey: '${key}'}), ${ms});
      out.subscribe = 'resolved ' + sub.endpoint.slice(0, 60); await sub.unsubscribe(); }
    catch (e) { out.subscribe = 'rejected ' + e.name + ': ' + e.message; }
  } catch (e) { out.error = e.name + ': ' + e.message; }
  return out; })()`}, s, ms + 10000);
const out = r.result?.result?.value;
check('pushManager.subscribe resolves', /^resolved https:/.test(out?.subscribe ?? ''), JSON.stringify(out ?? r.error ?? r.result));
await finish();
