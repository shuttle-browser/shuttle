// CDP smoke test for Shuttle. Usage: node scripts/smoke-cdp.mjs <port>
// Launch the browser without stealing focus:
//   open -g -n -a $SHUTTLE_ROOT/src/out/Dev/Chromium.app --args --user-data-dir=/tmp/ac-profile \
//     --remote-debugging-port=9333 --no-first-run about:blank
const port = process.argv[2] || 9333;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function browserWs() {
  for (let i = 0; i < 60; i++) {
    try {
      const r = await fetch(`http://127.0.0.1:${port}/json/version`);
      return (await r.json()).webSocketDebuggerUrl;
    } catch { await sleep(500); }
  }
  throw new Error('no CDP endpoint');
}

const ws = new WebSocket(await browserWs());
await new Promise((r) => ws.addEventListener('open', r, { once: true }));
let id = 0;
const pending = new Map();
const events = [];
ws.addEventListener('message', (e) => {
  const m = JSON.parse(e.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
  else events.push(m);
});
const send = (method, params = {}, sessionId) => new Promise((res) => {
  const i = ++id; pending.set(i, res);
  ws.send(JSON.stringify({ id: i, method, params, ...(sessionId ? { sessionId } : {}) }));
});

async function visit(url, { browserContextId, waitMs = 2500, evalExpr } = {}) {
  const t = await send('Target.createTarget', { url: 'about:blank', background: true, ...(browserContextId ? { browserContextId } : {}) });
  if (t.error) return { url, error: t.error.message };
  const targetId = t.result.targetId;
  const a = await send('Target.attachToTarget', { targetId, flatten: true });
  const sid = a.result.sessionId;
  await send('Runtime.enable', {}, sid);
  await send('Log.enable', {}, sid);
  const start = events.length;
  await send('Page.navigate', { url }, sid);
  await sleep(waitMs);
  const ev = events.slice(start).filter((e) => e.sessionId === sid);
  const exceptions = ev.filter((e) => e.method === 'Runtime.exceptionThrown')
    .map((e) => e.params.exceptionDetails.exception?.description?.split('\n')[0] || e.params.exceptionDetails.text);
  const errors = ev.filter((e) => e.method === 'Runtime.consoleAPICalled' && e.params.type === 'error')
    .map((e) => e.params.args.map((x) => x.value ?? x.description).join(' ').slice(0, 200));
  const loc = await send('Runtime.evaluate', { expression: 'location.href + " | " + document.title', returnByValue: true }, sid);
  let extra;
  if (evalExpr) extra = (await send('Runtime.evaluate', { expression: evalExpr, returnByValue: true, awaitPromise: true }, sid)).result?.result?.value;
  await send('Target.closeTarget', { targetId });
  return { url, at: loc.result?.result?.value, exceptions, errors, extra };
}

const results = [];
for (const u of ['chrome://newtab/', 'https://example.com/', 'chrome://settings/',
  'chrome://settings/people', 'chrome://settings/appearance', 'chrome://settings/privacy',
  'chrome://settings/clearBrowserData', 'chrome://history/', 'chrome://history/syncedTabs',
  'chrome://extensions/', 'chrome://downloads/', 'chrome://flags/', 'chrome://version/',
  'chrome://chrome-urls/', 'chrome://sync-internals/', 'chrome://signin-internals/',
  'chrome://profile-picker/', 'chrome://settings/autofill', 'chrome://settings/security',
  'chrome://settings/securityKeys', 'chrome://password-manager/', 'chrome://autofill-internals/',
  'chrome://password-manager-internals/', 'chrome://reset-password/', 'chrome://safe-browsing/',
  // Sweep 2 (dead loadTimeData keys / removed icons crash Dev builds):
  'chrome://settings/content', 'chrome://settings/content/all', 'chrome://settings/cookies',
  'chrome://settings/content/location', 'chrome://settings/content/notifications',
  'chrome://settings/searchEngines', 'chrome://settings/onStartup', 'chrome://settings/downloads',
  'chrome://settings/languages', 'chrome://settings/accessibility', 'chrome://settings/system',
  'chrome://settings/performance', 'chrome://settings/reset', 'chrome://settings/help',
  'chrome://settings/privacySandbox', 'chrome://history/grouped', 'chrome://bookmarks/',
  'chrome-untrusted://read-anything-side-panel.top-chrome/',
  'chrome://policy/', 'chrome://management/', 'chrome://components/']) {
  results.push(await visit(u));
}
// Browser contexts: OTR profiles are gone since B2d, so an error is expected until the MCP
// service provides partitions; a success would mean the OTR path is back.
const ctx = await send('Target.createBrowserContext');
results.push({ url: 'Target.createBrowserContext', at: ctx.error ? 'ERROR ' + ctx.error.message : 'ok ' + ctx.result.browserContextId });
if (!ctx.error) {
  results.push(await visit('https://example.com/', { browserContextId: ctx.result.browserContextId,
    evalExpr: 'document.cookie = "a=1"; document.cookie' }));
  await send('Target.disposeBrowserContext', { browserContextId: ctx.result.browserContextId });
}
// Credential Management API must settle (stub), WebAuthn feature detection must not throw.
results.push(await visit('https://example.com/', { evalExpr: `(async () => {
  const t = (p) => Promise.race([p.then(v => 'resolved:' + JSON.stringify(v), e => 'rejected:' + e.name),
    new Promise(r => setTimeout(() => r('TIMEOUT'), 3000))]);
  return {
    get: await t(navigator.credentials.get({password: true, mediation: 'silent'})),
    store: await t(navigator.credentials.store(new PasswordCredential({id: 'a', password: 'b'}))),
    uvpaa: await t(PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable()),
    conditional: await t(PublicKeyCredential.isConditionalMediationAvailable()),
  };
})()` }));
// Form page: renderer autofill agents are gone; typing into a password field must not crash.
results.push(await visit('data:text/html,<form><input name=u autocomplete=username><input type=password name=p></form>',
  { evalExpr: 'document.forms[0].p.focus(); document.forms[0].p.value="x"; "ok"' }));
const af = await send('Autofill.enable');
results.push({ url: 'CDP Autofill.enable', at: af.error ? 'ERROR ' + af.error.message : 'ok' });
// Tab groups: create a group via chrome.tabGroups is not reachable; check tab strip via Browser domain.
const v = await send('Browser.getVersion');
results.push({ url: 'Browser.getVersion', at: v.result?.product });
for (const r of results) console.log(JSON.stringify(r));
ws.close();
