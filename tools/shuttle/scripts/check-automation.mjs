// Checks fork commit 16 (always-on remote debugging, no automation UI).
// Usage: node scripts/check-automation.mjs <port> [userDataDir]
// Launch the browser first WITHOUT --remote-debugging-port (default 9222) and with
// --enable-automation, e.g. on macOS:
//   open -g -n -a $SHUTTLE_ROOT/src/out/Dev/Chromium.app --args --user-data-dir=/tmp/sh-auto \
//     --no-first-run --enable-automation about:blank
// Exits non-zero when a check fails. Leaves the browser running (close it with Browser.close).
import { readFileSync } from 'node:fs';

const port = Number(process.argv[2] || 9222);
const userDataDir = process.argv[3];
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => results.push({ name, ok: !!ok, detail });

let version;
for (let i = 0; i < 60 && !version; i++) {
  try { version = await (await fetch(`http://127.0.0.1:${port}/json/version`)).json(); }
  catch { await sleep(500); }
}
check('server answers /json/version', version, version?.Browser);
if (!version) { console.log(JSON.stringify(results)); process.exit(1); }

if (userDataDir) {
  let content = '';
  try { content = readFileSync(`${userDataDir}/DevToolsActivePort`, 'utf8'); } catch {}
  check('DevToolsActivePort written', content.split('\n')[0] === String(port), JSON.stringify(content.split('\n')[0]));
}

const ws = new WebSocket(version.webSocketDebuggerUrl);
await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
let id = 0;
const pending = new Map();
ws.onmessage = (e) => { const m = JSON.parse(e.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); } };
const send = (method, params = {}, sessionId) => new Promise((res) => {
  const i = ++id; pending.set(i, res);
  ws.send(JSON.stringify({ id: i, method, params, ...(sessionId ? { sessionId } : {}) }));
});
check('browser websocket connects without approval', true);

const t = await send('Target.createTarget', { url: 'https://example.com/' });
const { sessionId } = (await send('Target.attachToTarget', { targetId: t.result.targetId, flatten: true })).result;
await sleep(2000);
const evalJs = async (expression) => (await send('Runtime.evaluate', { expression, returnByValue: true }, sessionId)).result?.result?.value;
check('navigator.webdriver === false', (await evalJs('navigator.webdriver')) === false);
await send('Emulation.setAutomationOverride', { enabled: true }, sessionId);
check('still false after Emulation.setAutomationOverride', (await evalJs('navigator.webdriver')) === false);

// Infobars live in the browser UI above the page, so they shrink innerHeight. Tab strip +
// toolbar alone take ~80-95 px; an infobar adds ~40 px (old code: one per tab on override).
const chromeHeight = await evalJs('window.outerHeight - window.innerHeight');
check('no infobar (browser UI height < 110 px)', chromeHeight > 0 && chromeHeight < 110, `${chromeHeight} px`);
await send('Target.closeTarget', { targetId: t.result.targetId });

// chrome://inspect loads without JS errors and hides the remote-debugging toggle.
const t2 = await send('Target.createTarget', { url: 'about:blank', background: true });
const s2 = (await send('Target.attachToTarget', { targetId: t2.result.targetId, flatten: true })).result.sessionId;
const errors = [];
ws.addEventListener('message', (e) => {
  const m = JSON.parse(e.data);
  if (m.sessionId === s2 && m.method === 'Runtime.exceptionThrown') errors.push(m.params.exceptionDetails.text);
});
await send('Runtime.enable', {}, s2);
await send('Page.navigate', { url: 'chrome://inspect/#devices' }, s2);
await sleep(2500);
const inspectTitle = (await send('Runtime.evaluate', { expression: 'document.title', returnByValue: true }, s2)).result?.result?.value;
check('chrome://inspect loads without JS errors', inspectTitle && errors.length === 0, `${inspectTitle} ${errors.join('; ')}`);
await send('Target.closeTarget', { targetId: t2.result.targetId });

for (const r of results) console.log(`${r.ok ? 'PASS' : 'FAIL'}  ${r.name}${r.detail ? `  (${r.detail})` : ''}`);
ws.close();
process.exit(results.every((r) => r.ok) ? 0 : 1);
