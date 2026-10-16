// Smoke test for per-session CDP endpoints (docs/MCP.md "Per-session CDP endpoint").
// Usage: node scripts/smoke-session-cdp.mjs [port]   (default 9333; use a fresh test browser)
//   Raw CDP checks need nothing. Optional: PLAYWRIGHT_CORE=/path/to/node_modules/playwright-core
//   and AGENT_BROWSER=/path/to/agent-browser add Playwright / agent-browser checks.
// Checks: session endpoint sees only its tabs, foreign attach rejected, auto-attach doesn't
// stall foreign tabs, createTarget leases a tab, cookies are the agent's, page endpoints are
// scoped too, viewport on the active tab, CSS-px capture, top-chrome UI is "other",
// Browser.close ends only the session.
import { execFileSync } from 'node:child_process';

const port = process.argv[2] || 9333;
const base = `http://127.0.0.1:${port}`;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let failures = 0;
const check = (ok, label, detail = '') => {
  if (!ok) failures++;
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${label}${detail ? ` - ${detail}` : ''}`);
};

async function mcp(name, args = {}) {
  const r = await fetch(`${base}/mcp`, { method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ jsonrpc: '2.0', id: 1, method: 'tools/call', params: { name, arguments: args } }) });
  const b = await r.json();
  if (b.result?.isError) throw new Error(`${name}: ${b.result.content?.[0]?.text}`);
  return b.result?.structuredContent ?? b.result;
}

// Minimal CDP client: send() resolves with the message (result or error); events are collected.
function cdp(url) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(url);
    let id = 0;
    const pending = new Map();
    const events = [];
    const client = {
      events,
      send(method, params = {}, sessionId) {
        const msg = { id: ++id, method, params };
        if (sessionId) msg.sessionId = sessionId;
        ws.send(JSON.stringify(msg));
        return new Promise((res) => pending.set(msg.id, res));
      },
      close: () => ws.close(),
    };
    ws.onopen = () => resolve(client);
    ws.onerror = (e) => reject(new Error(`ws error ${url}: ${e.message || 'failed'}`));
    ws.onmessage = (e) => {
      const m = JSON.parse(e.data);
      if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); } else events.push(m);
    };
  });
}

const ids = (infos) => infos.map((t) => t.targetId);
const evalIn = async (client, sessionId, expression) =>
  (await client.send('Runtime.evaluate', { expression, returnByValue: true }, sessionId)).result?.result?.value;

// JPEG size from the SOF0/SOF2 marker.
function jpegSize(b64) {
  const b = Buffer.from(b64, 'base64');
  for (let i = 2; i < b.length;) {
    const marker = b[i + 1], len = b.readUInt16BE(i + 2);
    if (marker === 0xc0 || marker === 0xc2) return { height: b.readUInt16BE(i + 5), width: b.readUInt16BE(i + 7) };
    i += 2 + len;
  }
  return null;
}

// Setup: two agents, one session each, one tab each.
for (const id of ['smoke-a', 'smoke-b']) {
  try { await mcp('delete-agent', { id }); } catch {}
  await mcp('create-agent', { id, name: id });
}
const sa = await mcp('open-session', { agent: 'smoke-a' });
const sb = await mcp('open-session', { agent: 'smoke-b' });
check(sa.cdpEndpoint?.startsWith(`ws://127.0.0.1:${port}/devtools/session/${sa.sessionId}`), 'open-session returns a session endpoint', sa.cdpEndpoint);
const ta = await mcp('allocate-tab', { sessionId: sa.sessionId, url: 'data:text/html,<title>A</title>A', width: 390, height: 844 });
const tb = await mcp('allocate-tab', { sessionId: sb.sessionId, url: 'data:text/html,<title>B</title>B' });
await sleep(1500);

// Top-chrome UI is not a page.
const list = await (await fetch(`${base}/json/list`)).json();
const topChrome = list.filter((t) => t.url.includes('.top-chrome'));
check(topChrome.every((t) => t.type !== 'page'), 'top-chrome UI is not type page', topChrome.map((t) => `${t.type} ${t.url}`).join(', '));

// Unknown session endpoint is rejected.
let rejected = false;
try { (await cdp(`ws://127.0.0.1:${port}/devtools/session/nope`)).close(); } catch { rejected = true; }
check(rejected, 'unknown session endpoint rejected');

const a = await cdp(sa.cdpEndpoint);
const targets = (await a.send('Target.getTargets')).result?.targetInfos ?? [];
check(ids(targets).includes(ta.targetId) && !ids(targets).includes(tb.targetId), 'getTargets: own tab only', `${targets.length} targets`);
check(targets.every((t) => !t.url.startsWith('chrome://')), 'getTargets: no browser UI');

const foreign = await a.send('Target.attachToTarget', { targetId: tb.targetId, flatten: true });
check(!!foreign.error, 'attachToTarget foreign rejected', foreign.error?.message);
check(!!(await a.send('Target.attachToBrowserTarget')).error, 'attachToBrowserTarget rejected');
check(!!(await a.send('Target.createBrowserContext')).error, 'createBrowserContext rejected');
check(!!(await a.send('Target.closeTarget', { targetId: tb.targetId })).error, 'closeTarget foreign rejected');

// Auto-attach (Playwright style): own targets only; foreign tabs keep running.
// Own connection: a client that auto-attached with waitForDebuggerOnStart holds new navigations
// until it disconnects (upstream behavior, even after setAutoAttach false).
const aa = await cdp(sa.cdpEndpoint);
await aa.send('Target.setDiscoverTargets', { discover: true });
await aa.send('Target.setAutoAttach', { autoAttach: true, waitForDebuggerOnStart: true, flatten: true });
await sleep(1000);
const tb2 = await mcp('allocate-tab', { sessionId: sb.sessionId, url: 'data:text/html,<title>B2</title>B2' });
await sleep(1500);
const seen = aa.events.filter((e) => e.method?.startsWith('Target.')).map((e) => e.params?.targetInfo?.targetId).filter(Boolean);
check(seen.includes(ta.targetId), 'auto-attach/discover: own tab announced');
check(!seen.includes(tb.targetId) && !seen.includes(tb2.targetId), 'auto-attach/discover: foreign tabs hidden');
const b = await cdp(sb.cdpEndpoint);
const bAttach = await b.send('Target.attachToTarget', { targetId: tb2.targetId, flatten: true });
const b2Title = await evalIn(b, bAttach.result?.sessionId, 'document.title');
check(b2Title === 'B2', 'foreign tab created during auto-attach is not stalled', `title=${b2Title}`);
aa.close();
await sleep(300);

// Viewport on the active tab (the session's first tab opened the window).
const attachA = await a.send('Target.attachToTarget', { targetId: ta.targetId, flatten: true });
const width = await evalIn(a, attachA.result?.sessionId, 'innerWidth');
check(width === 390, 'allocate-tab width applies (first tab, active)', `innerWidth=${width}`);
await mcp('set-tab-viewport', { targetId: ta.targetId, width: 500, height: 700 });
await sleep(500);
const width2 = await evalIn(a, attachA.result?.sessionId, 'innerWidth');
check(width2 === 500, 'set-tab-viewport applies to the active tab', `innerWidth=${width2}`);
const shot = await (await fetch(`${base}/mcp`, { method: 'POST', headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ jsonrpc: '2.0', id: 1, method: 'tools/call', params: { name: 'capture-tab', arguments: { targetId: ta.targetId } } }) })).json();
const size = jpegSize(shot.result?.content?.[0]?.data ?? '');
check(size?.width === 500 && size?.height === 700, 'capture-tab is the viewport in CSS px', JSON.stringify(size));

// createTarget leases a tab to the session.
const created = await a.send('Target.createTarget', { url: 'about:blank' });
const sessionsNow = (await mcp('list-sessions')).sessions;
const own = sessionsNow.find((s) => s.sessionId === sa.sessionId);
check(own?.tabs.some((t) => t.targetId === created.result?.targetId), 'createTarget leases a session tab');

// Cookies are the agent's.
await a.send('Storage.setCookies', { cookies: [{ name: 'smoke', value: 'a', domain: 'example.com', path: '/' }] });
const ca = (await a.send('Storage.getCookies')).result?.cookies ?? [];
const cb = (await b.send('Storage.getCookies')).result?.cookies ?? [];
check(ca.some((c) => c.name === 'smoke' && c.value === 'a'), 'Storage cookies: set/get in agent storage');
check(!cb.some((c) => c.name === 'smoke'), 'Storage cookies: other agent does not see them');
const pageCookies = (await a.send('Network.getCookies', { urls: ['https://example.com/'] }, attachA.result?.sessionId)).result?.cookies ?? [];
check(pageCookies.some((c) => c.name === 'smoke'), 'Storage cookies: the tab sees them');

// Page endpoint of a leased tab is scoped too.
const p = await cdp(ta.cdpUrl);
const pTargets = (await p.send('Target.getTargets')).result?.targetInfos ?? [];
check(!ids(pTargets).includes(tb.targetId), 'page endpoint: foreign tabs hidden', `${pTargets.length} targets`);
check(!!(await p.send('Target.attachToTarget', { targetId: tb.targetId, flatten: true })).error, 'page endpoint: foreign attach rejected');
p.close();

// Optional: Playwright.
if (process.env.PLAYWRIGHT_CORE) {
  const { chromium } = await import(`${process.env.PLAYWRIGHT_CORE}/index.mjs`);
  const browser = await chromium.connectOverCDP(sa.cdpEndpoint, { timeout: 15000 });
  const pages = browser.contexts()[0]?.pages() ?? [];
  check(browser.contexts().length === 1 && pages.length >= 1 && pages.every((pg) => !pg.url().includes('B')), 'playwright: one context, own pages only', pages.map((pg) => pg.url()).join(', '));
  const cookies = await browser.contexts()[0].cookies();
  check(cookies.some((c) => c.name === 'smoke'), 'playwright: context.cookies() is the agent\'s');
  const np = await browser.contexts()[0].newPage();
  await np.goto('data:text/html,<title>PW</title>pw');
  const s2 = (await mcp('list-sessions')).sessions.find((s) => s.sessionId === sa.sessionId);
  check(s2.tabs.some((t) => t.title === 'PW'), 'playwright: newPage is a session tab');
  await browser.close();
}

// Optional: agent-browser.
if (process.env.AGENT_BROWSER) {
  const env = { ...process.env, AGENT_BROWSER_SESSION: 'smoke-session-cdp' };
  const run = (...args) => execFileSync(process.env.AGENT_BROWSER, args, { env, encoding: 'utf8', timeout: 40000 });
  try {
    const tabs = run('--cdp', sa.cdpEndpoint, 'tab');
    check(!tabs.includes('B2') && !/\bB\b/.test(tabs), 'agent-browser: sees only session tabs', tabs.trim().split('\n').length + ' tabs');
    const snap = run('snapshot');
    check(snap.length > 0, 'agent-browser: snapshot works');
  } catch (e) { check(false, 'agent-browser run', e.message.split('\n')[0]); }
  try { run('close', '--all'); } catch {}
}

// Browser.close on the session endpoint ends only the session.
await a.send('Browser.close');
await sleep(1000);
const after = (await mcp('list-sessions')).sessions;
check(!after.some((s) => s.sessionId === sa.sessionId) && after.some((s) => s.sessionId === sb.sessionId), 'Browser.close ends only this session');
check((await fetch(`${base}/json/version`)).ok, 'browser still running');

a.close(); b.close();
await mcp('close-session', { sessionId: sb.sessionId });
for (const id of ['smoke-a', 'smoke-b']) await mcp('delete-agent', { id });
console.log(failures ? `${failures} FAILED` : 'all ok');
process.exit(failures ? 1 : 0);
