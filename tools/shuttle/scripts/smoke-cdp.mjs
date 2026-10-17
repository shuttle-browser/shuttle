// CDP smoke: WebUI pages load without JS exceptions / console errors, browser contexts, credentials stub, form page.
// Usage: node scripts/smoke-cdp.mjs <port>   (run by scripts/smoke-all.sh; local pages only, no internet)
// Every exception/error must match EXPECTED; an EXPECTED entry that does not occur fails too (behavior changed).
import http from 'node:http';
import { browserWs, cdp, check, cleanup, evaluate, finish, sleep } from './lib/smoke.mjs';

const EXPECTED = [
  // OTR profiles are gone since B2d: an error until the MCP service provides partitions; success = OTR path is back.
  { url: /^Target\.createBrowserContext$/, text: /^Failed to create browser context\.$/ },
  // Autofill backend removed: the CDP domain is gone.
  { url: /^Autofill\.enable$/, text: /^'Autofill\.enable' wasn't found$/ },
];
const seen = new Set();
const judge = (url, issues) => {
  const bad = issues.filter((text) => {
    const i = EXPECTED.findIndex((e) => e.url.test(url) && e.text.test(text));
    if (i >= 0) { seen.add(i); console.log(`     expected: ${url}: ${text.slice(0, 160)}`); }
    return i < 0;
  });
  check(url, bad.length === 0, bad.join(' | ').slice(0, 300));
};

const site = http.createServer((q, r) => { r.setHeader('Content-Type', 'text/html'); r.end('<title>local</title>local'); });
await new Promise((r) => site.listen(0, '127.0.0.1', r));
cleanup(() => site.close());
const local = `http://localhost:${site.address().port}/`;  // localhost = secure context (credentials API)

const b = await cdp(await browserWs());
async function visit(url, { browserContextId, waitMs = 2500, evalExpr } = {}) {
  const t = await b.send('Target.createTarget', { url: 'about:blank', background: true, ...(browserContextId ? { browserContextId } : {}) });
  if (t.error) return { issues: [`createTarget: ${t.error.message}`] };
  const targetId = t.result.targetId;
  cleanup(() => b.send('Target.closeTarget', { targetId }).catch(() => {}));
  const sid = (await b.send('Target.attachToTarget', { targetId, flatten: true })).result?.sessionId;
  if (!sid) return { issues: ['attachToTarget failed'] };
  await b.send('Runtime.enable', {}, sid);
  await b.send('Log.enable', {}, sid);
  const start = b.events.length;
  const nav = await b.send('Page.navigate', { url }, sid);
  await sleep(waitMs);
  const ev = b.events.slice(start).filter((e) => e.sessionId === sid);
  const issues = [
    ...(nav.error || nav.result?.errorText ? [`navigate: ${nav.error?.message ?? nav.result.errorText}`] : []),
    ...ev.filter((e) => e.method === 'Runtime.exceptionThrown')
      .map((e) => e.params.exceptionDetails.exception?.description?.split('\n')[0] || e.params.exceptionDetails.text),
    ...ev.filter((e) => e.method === 'Runtime.consoleAPICalled' && e.params.type === 'error')
      .map((e) => e.params.args.map((x) => x.value ?? x.description).join(' ').slice(0, 200)),
  ];
  const at = await evaluate(b, sid, 'location.href');
  if (/^chrome-error:/.test(at ?? '')) issues.push(`error page (${at})`);
  const extra = evalExpr ? await evaluate(b, sid, evalExpr) : undefined;
  await b.send('Target.closeTarget', { targetId });
  return { issues, extra };
}

// Pages removed by fork commits (sync/signin/profile picker, password manager/autofill, bookmarks, side panel /
// Read Anything) are not listed: an error page fails the visit.
for (const u of ['chrome://newtab/', local, 'chrome://settings/',
  'chrome://settings/people', 'chrome://settings/appearance', 'chrome://settings/privacy',
  'chrome://settings/clearBrowserData', 'chrome://history/', 'chrome://history/syncedTabs',
  'chrome://extensions/', 'chrome://downloads/', 'chrome://flags/', 'chrome://version/',
  'chrome://chrome-urls/', 'chrome://settings/autofill', 'chrome://settings/security',
  'chrome://settings/securityKeys', 'chrome://safe-browsing/',
  // Sweep 2 (dead loadTimeData keys / removed icons crash Dev builds):
  'chrome://settings/content', 'chrome://settings/content/all', 'chrome://settings/cookies',
  'chrome://settings/content/location', 'chrome://settings/content/notifications',
  'chrome://settings/searchEngines', 'chrome://settings/onStartup', 'chrome://settings/downloads',
  'chrome://settings/languages', 'chrome://settings/accessibility', 'chrome://settings/system',
  'chrome://settings/performance', 'chrome://settings/reset', 'chrome://settings/help',
  'chrome://settings/privacySandbox', 'chrome://history/grouped',
  'chrome://policy/', 'chrome://management/', 'chrome://components/']) {
  judge(u, (await visit(u)).issues);
}

const ctx = await b.send('Target.createBrowserContext');
judge('Target.createBrowserContext', ctx.error ? [ctx.error.message] : []);
if (!ctx.error) {
  const { browserContextId } = ctx.result;
  const r = await visit(local, { browserContextId, evalExpr: 'document.cookie = "a=1"; document.cookie' });
  judge('browser context page', r.issues);
  await b.send('Target.disposeBrowserContext', { browserContextId });
}

// Credential Management API must settle (stub), WebAuthn feature detection must not throw.
const cred = await visit(local, { evalExpr: `(async () => {
  const t = (p) => Promise.race([p.then(v => 'resolved:' + JSON.stringify(v), e => 'rejected:' + e.name),
    new Promise(r => setTimeout(() => r('TIMEOUT'), 3000))]);
  return {
    get: await t(navigator.credentials.get({password: true, mediation: 'silent'})),
    store: await t(navigator.credentials.store(new PasswordCredential({id: 'a', password: 'b'}))),
    uvpaa: await t(PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable()),
    conditional: await t(PublicKeyCredential.isConditionalMediationAvailable()),
  };
})()` });
judge('credentials page', cred.issues);
const vals = Object.values(cred.extra ?? {});
check('credentials API settles', vals.length === 4 && !vals.includes('TIMEOUT'), JSON.stringify(cred.extra));

// Form page: renderer autofill agents are gone; typing into a password field must not crash.
const form = await visit('data:text/html,<form><input name=u autocomplete=username><input type=password name=p></form>',
  { evalExpr: 'document.forms[0].p.focus(); document.forms[0].p.value="x"; "ok"' });
judge('password form page', form.issues);
check('password form typing', form.extra === 'ok', String(form.extra));

const af = await b.send('Autofill.enable');
judge('Autofill.enable', af.error ? [af.error.message] : []);
check('Browser.getVersion', /Shuttle|Chrome/.test((await b.send('Browser.getVersion')).result?.product ?? ''));
EXPECTED.forEach((e, i) => check(`expected error occurred: ${e.url}`, seen.has(i), seen.has(i) ? '' : 'behavior changed: update EXPECTED'));
await finish();
