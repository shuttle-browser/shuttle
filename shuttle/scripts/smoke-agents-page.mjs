// chrome://agents smoke test: seeds agents over MCP, opens the page via CDP and drives create,
// edit (+avatar), open tab as agent, show tab, end session, delete and live refresh.
// Usage: node scripts/smoke-agents-page.mjs [port=9333] [screenshotDir]
//   Use a fresh profile; leaves agents alice, steve, late. Exit 1 on any FAIL.
// With screenshotDir: writes agents-dark.png, agents-light.png, agents-dialog.png there.
import {writeFileSync} from 'node:fs';

const port = process.argv[2] || '9333';
const shotDir = process.argv[3];
const base = `http://127.0.0.1:${port}`;
const sleep = ms => new Promise(r => setTimeout(r, ms));
let fails = 0;
const check = (label, ok, extra = '') => {
  if (!ok) fails++;
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${label} ${extra}`);
};

const mcp = async (name, args = {}) => {
  const r = await fetch(`${base}/mcp`, {method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({jsonrpc: '2.0', id: 1, method: 'tools/call', params: {name, arguments: args}})});
  return (await r.json()).result.structuredContent;
};

// CDP plumbing.
const v = await (await fetch(`${base}/json/version`)).json();
const ws = new WebSocket(v.webSocketDebuggerUrl);
let id = 0;
const pending = new Map();
const events = [];
ws.onmessage = m => {
  const d = JSON.parse(m.data);
  if (d.id && pending.has(d.id)) { pending.get(d.id)(d); pending.delete(d.id); } else if (d.method) events.push(d);
};
await new Promise(r => ws.onopen = r);
const send = (method, params = {}, sessionId) => new Promise(r => {
  const i = ++id; pending.set(i, r);
  ws.send(JSON.stringify({id: i, method, params, ...(sessionId ? {sessionId} : {})}));
});

// Seed.
await mcp('create-agent', {id: 'alice', name: 'Alice', color: 'cyan'});
await mcp('create-agent', {id: 'steve', name: 'Steve', color: 'orange'});
const seeded = await mcp('open-session', {agent: 'alice'});
await mcp('allocate-tab', {sessionId: seeded.sessionId, url: 'https://example.com/'});
await mcp('allocate-tab', {sessionId: seeded.sessionId, url: 'https://www.iana.org/'});

// Open the page.
const created = await send('Target.createTarget', {url: 'chrome://agents', background: true});
const pageId = created.result.targetId;
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

const first = await ev(`return qa('.card.agent').length + '/' + qa('.tab-row').length;`);
check('page lists seeded agents + tabs', first === '2/2', first);
await shot('agents-dark.png');

// Create via the dialog (id follows the name).
await ev(`q('#create').click(); await wait(300); type(q('#nameInput'), 'Research Bot');`);
await sleep(200);
const idVal = await ev(`return q('#idInput').value;`);
check('id follows name', idVal === 'research-bot', idVal);
await ev(`qa('#editDialog .swatch')[4].click(); await wait(100); q('#editSave').click();`);
await sleep(800);
let a = (await mcp('list-agents')).agents.find(x => x.id === 'research-bot');
check('create via dialog', a && a.name === 'Research Bot' && a.color === 'green', JSON.stringify(a));

// Invalid id: the error shows on the id field, the dialog stays open.
const invalid = await ev(`q('#create').click(); await wait(300); type(q('#idInput'), 'Bad Id'); await wait(100);
  q('#editSave').click(); await wait(400);
  const r = q('#idInput').invalid + ':' + q('#editDialog').open; q('#editDialog').cancel(); return r;`);
check('invalid id shows error', invalid === 'true:true', invalid);

// Edit Steve via the more-actions menu: rename + avatar data URL.
const avatar = 'data:image/svg+xml;base64,' + Buffer.from(
  '<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10"><rect width="10" height="10" fill="#9B6BFF"/></svg>')
  .toString('base64');
await ev(`q('.card[data-agent=steve] .more').click(); await wait(200); q('#menuEdit').click(); await wait(300);
  type(q('#nameInput'), 'Steve Jr'); type(q('#avatarInput'), ${JSON.stringify(avatar)}); await wait(100);`);
await sleep(200);
if (shotDir) {
  await send('Emulation.setEmulatedMedia', {features: [{name: 'prefers-color-scheme', value: 'light'}]}, s);
  await sleep(200);
  await shot('agents-dialog.png');
}
await ev(`q('#editSave').click();`);
await sleep(800);
a = (await mcp('list-agents', {avatarData: true})).agents.find(x => x.id === 'steve');
check('edit name+avatar', a.name === 'Steve Jr' && a.avatar === avatar);
const imgOk = await ev(`await wait(300); const img = q('.card[data-agent=steve] .avatar img');
  return img ? img.complete && img.naturalWidth > 0 : 'no img';`);
check('avatar image renders (CSP)', imgOk === true, String(imgOk));

// Open a tab as Steve (no session yet).
await ev(`q('.card[data-agent=steve] .open-tab').click(); await wait(300); type(q('#urlInput'), 'example.org');
  await wait(100); q('#openConfirm').click();`);
await sleep(1500);
const ss = (await mcp('list-sessions')).sessions.filter(x => x.agent === 'steve');
check('open tab as agent', ss.length === 1 && ss[0].tabs.length === 1 && ss[0].tabs[0].url.includes('example.org'),
      JSON.stringify(ss.map(x => x.tabs.map(t => t.url))));
const bad = await ev(`q('.card[data-agent=steve] .open-tab').click(); await wait(300);
  type(q('#urlInput'), 'file:///etc/passwd'); await wait(100); q('#openConfirm').click(); await wait(500);
  const e = q('#urlInput').invalid ? q('#urlInput').errorMessage : ''; q('#openDialog').cancel(); return e;`);
check('non-http URL rejected', /http/.test(bad), bad);

// Show tab: clicking one of Alice's tabs brings it to the front.
await sleep(2500);
const alice = (await mcp('list-sessions')).sessions.find(x => x.agent === 'alice');
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

// Delete research-bot via the menu + confirmation dialog.
await ev(`q('.card[data-agent=research-bot] .more').click(); await wait(200); q('#menuDelete').click(); await wait(300);
  q('#deleteConfirm').click();`);
await sleep(1500);
check('delete agent', !(await mcp('list-agents')).agents.some(x => x.id === 'research-bot'));

// Live refresh: an agent created over MCP shows up without a reload.
await mcp('create-agent', {id: 'late', name: 'Late Arrival'});
await sleep(2600);
check('live refresh', await ev(`return qa('.card .name').some(n => n.textContent.includes('Late Arrival'));`) === true);

// Search filters the list.
const filtered = await ev(`const t = q('cr-toolbar'); t.getSearchField().setValue('stev'); await wait(400);
  const n = qa('.card.agent').length; t.getSearchField().setValue(''); await wait(300); return n;`);
check('search filters agents', filtered === 1, String(filtered));
await shot('agents-light.png');

const ex = events.filter(e => e.method === 'Runtime.exceptionThrown');
check('no JS exceptions', ex.length === 0, ex.map(e => JSON.stringify(e.params).slice(0, 200)).join(' '));
console.log(fails ? `${fails} FAILED` : 'ALL OK');
process.exit(fails ? 1 : 0);
