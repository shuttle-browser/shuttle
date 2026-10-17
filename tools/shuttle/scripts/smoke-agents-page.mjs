// chrome://agents smoke: seeds agents over MCP, opens the page via CDP and drives create, edit (+avatar), open tab
// as agent, show tab, end session, delete and live refresh. Local pages only; agent ids are prefixed smoke-agents-
// and deleted at exit, so it works on a shared profile.
// Usage: node scripts/smoke-agents-page.mjs <port> [screenshotDir]   (run by scripts/smoke-all.sh)
// With screenshotDir: writes agents-dark.png, agents-light.png, agents-dialog.png there.
import {writeFileSync} from 'node:fs';
import http from 'node:http';
import {args, browserWs, cdp, check, cleanup, finish, mcp, sleep, gone } from './lib/smoke.mjs';

const shotDir = args[0];
const P = 'smoke-agents-';
const site = http.createServer((q, r) => { r.setHeader('Content-Type', 'text/html'); r.end(`<title>${q.url}</title>${q.url}`); });
await new Promise(r => site.listen(0, '127.0.0.1', r));
const local = path => `http://127.0.0.1:${site.address().port}${path}`;
cleanup(() => site.close());
cleanup(async () => {
  for (const x of (await mcp('list-sessions')).sessions.filter(x => x.agent.startsWith(P))) {
    await mcp('close-session', {sessionId: x.sessionId}).catch(gone);
  }
  for (const x of (await mcp('list-agents')).agents.filter(x => x.id.startsWith(P))) {
    await mcp('delete-agent', {id: x.id}).catch(gone);
  }
});

const b = await cdp(await browserWs());
const {send, events} = b;

// Seed.
await mcp('create-agent', {id: P + 'alice', name: 'Alice', color: 'cyan'});
await mcp('create-agent', {id: P + 'steve', name: 'Steve', color: 'orange'});
const seeded = await mcp('open-session', {agent: P + 'alice'});
await mcp('allocate-tab', {sessionId: seeded.sessionId, url: local('/one')});
await mcp('allocate-tab', {sessionId: seeded.sessionId, url: local('/two')});

// Open the page.
const created = await send('Target.createTarget', {url: 'chrome://agents', background: true});
const pageId = created.result.targetId;
cleanup(() => send('Target.closeTarget', {targetId: pageId}));
const {result: {sessionId: s}} = await send('Target.attachToTarget', {targetId: pageId, flatten: true});
await send('Runtime.enable', {}, s);
await send('Emulation.setDeviceMetricsOverride', {width: 1100, height: 760, deviceScaleFactor: 1, mobile: false}, s);
const PRELUDE = `const R = document.querySelector('agents-app').shadowRoot;
  const q = x => R.querySelector(x); const qa = x => [...R.querySelectorAll(x)];
  const type = (crInput, v) => { const i = crInput.inputElement; i.value = v;
    i.dispatchEvent(new Event('input', {bubbles: true, composed: true})); };
  const wait = ms => new Promise(r => setTimeout(r, ms));`;
const ev = async expr => {
  const r = await send('Runtime.evaluate', {expression: `(async () => { ${PRELUDE} ${expr} })()`,
    awaitPromise: true, returnByValue: true}, s);
  return r.result.exceptionDetails ? 'EXC ' + JSON.stringify(r.result.exceptionDetails).slice(0, 300) :
                                     r.result.result.value;
};
const shot = async name => {
  if (!shotDir) return;
  const r = await send('Page.captureScreenshot', {format: 'png'}, s);
  writeFileSync(`${shotDir}/${name}`, Buffer.from(r.result.data, 'base64'));
};
await sleep(2500);

const first = await ev(`const c = qa('.card.agent[data-agent^="${P}"]'); return c.length + '/' + c.flatMap(x => [...x.querySelectorAll('.tab-row')]).length;`);
check('page lists seeded agents + tabs', first === '2/2', first);
await shot('agents-dark.png');

// Create via the dialog (id follows the name).
await ev(`q('#create').click(); await wait(300); type(q('#nameInput'), 'Smoke Agents Bot');`);
await sleep(200);
const idVal = await ev(`return q('#idInput').value;`);
check('id follows name', idVal === P + 'bot', idVal);
await ev(`qa('#editDialog .swatch')[4].click(); await wait(100); q('#editSave').click();`);
await sleep(800);
let a = (await mcp('list-agents')).agents.find(x => x.id === P + 'bot');
check('create via dialog', a && a.name === 'Smoke Agents Bot' && a.color === 'green', JSON.stringify(a));

// Invalid id: the error shows on the id field, the dialog stays open.
const invalid = await ev(`q('#create').click(); await wait(300); type(q('#idInput'), 'Bad Id'); await wait(100);
  q('#editSave').click(); await wait(400);
  const r = q('#idInput').invalid + ':' + q('#editDialog').open; q('#editDialog').cancel(); return r;`);
check('invalid id shows error', invalid === 'true:true', invalid);

// Edit Steve via the more-actions menu: rename + avatar data URL.
const avatar = 'data:image/svg+xml;base64,' + Buffer.from(
  '<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10"><rect width="10" height="10" fill="#9B6BFF"/></svg>')
  .toString('base64');
await ev(`q('.card[data-agent=${P}steve] .more').click(); await wait(200); q('#menuEdit').click(); await wait(300);
  type(q('#nameInput'), 'Steve Jr'); type(q('#avatarInput'), ${JSON.stringify(avatar)}); await wait(100);`);
await sleep(200);
if (shotDir) {
  await send('Emulation.setEmulatedMedia', {features: [{name: 'prefers-color-scheme', value: 'light'}]}, s);
  await sleep(200);
  await shot('agents-dialog.png');
}
await ev(`q('#editSave').click();`);
await sleep(800);
a = (await mcp('list-agents', {avatarData: true})).agents.find(x => x.id === P + 'steve');
check('edit name+avatar', a.name === 'Steve Jr' && a.avatar === avatar);
const imgOk = await ev(`await wait(300); const img = q('.card[data-agent=${P}steve] .avatar img');
  return img ? img.complete && img.naturalWidth > 0 : 'no img';`);
check('avatar image renders (CSP)', imgOk === true, String(imgOk));

// Open a tab as Steve (no session yet).
await ev(`q('.card[data-agent=${P}steve] .open-tab').click(); await wait(300); type(q('#urlInput'), '${local('/steve')}');
  await wait(100); q('#openConfirm').click();`);
await sleep(1500);
const ss = (await mcp('list-sessions')).sessions.filter(x => x.agent === P + 'steve');
check('open tab as agent', ss.length === 1 && ss[0].tabs.length === 1 && ss[0].tabs[0].url.includes('/steve'),
      JSON.stringify(ss.map(x => x.tabs.map(t => t.url))));
const bad = await ev(`q('.card[data-agent=${P}steve] .open-tab').click(); await wait(300);
  type(q('#urlInput'), 'file:///etc/passwd'); await wait(100); q('#openConfirm').click(); await wait(500);
  const e = q('#urlInput').invalid ? q('#urlInput').errorMessage : ''; q('#openDialog').cancel(); return e;`);
check('non-http URL rejected', /http/.test(bad), bad);

// Show tab: clicking one of Alice's tabs brings it to the front.
await sleep(2500);
const alice = (await mcp('list-sessions')).sessions.find(x => x.agent === P + 'alice');
await ev(`q('.tab-row[data-target="${alice.tabs[0].targetId}"]').click();`);
await sleep(800);
const {result: {sessionId: ts}} = await send('Target.attachToTarget', {targetId: alice.tabs[0].targetId, flatten: true});
const vis = await send('Runtime.evaluate', {expression: 'document.visibilityState', returnByValue: true}, ts);
check('show tab activates it', vis.result.result.value === 'visible', vis.result.result.value);

// End Alice's session.
await send('Target.activateTarget', {targetId: pageId});
await sleep(300);
await ev(`q('cr-button[data-session="${alice.sessionId}"]').click();`);
await sleep(1000);
check('end session', !(await mcp('list-sessions')).sessions.some(x => x.sessionId === alice.sessionId));

// Delete the dialog-created agent via the menu + confirmation dialog.
await ev(`q('.card[data-agent=${P}bot] .more').click(); await wait(200); q('#menuDelete').click(); await wait(300);
  q('#deleteConfirm').click();`);
await sleep(1500);
check('delete agent', !(await mcp('list-agents')).agents.some(x => x.id === P + 'bot'));

// Live refresh: an agent created over MCP shows up without a reload.
await mcp('create-agent', {id: P + 'late', name: 'Late Arrival'});
await sleep(2600);
check('live refresh', await ev(`return qa('.card .name').some(n => n.textContent.includes('Late Arrival'));`) === true);

// Search filters the list.
const filtered = await ev(`const t = q('cr-toolbar'); t.getSearchField().setValue('stev'); await wait(400);
  const n = qa('.card.agent[data-agent^="${P}"]').length; t.getSearchField().setValue(''); await wait(300); return n;`);
check('search filters agents', filtered === 1, String(filtered));
await shot('agents-light.png');

const ex = events.filter(e => e.method === 'Runtime.exceptionThrown');
check('no JS exceptions', ex.length === 0, ex.map(e => JSON.stringify(e.params).slice(0, 200)).join(' '));
await finish();
