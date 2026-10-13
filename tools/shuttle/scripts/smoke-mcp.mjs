// MCP smoke test for Shuttle's in-browser MCP server (POST /mcp on the CDP port).
// Usage: node scripts/smoke-mcp.mjs <port> [--persist-check]
//   Default run: protocol, agents (identity), sessions, tab isolation, popup inheritance,
//   viewport, capture, navigation, origin check, cleanup. Leaves agent
//   "smoke-persist" with a cookie for a restart check.
//   --persist-check (after a browser restart): checks that cookie, and the agent's identity, then deletes the agent.
// Launch (no focus stealing):
//   open -g -n "$SHUTTLE_ROOT/src/out/Sccache/Shuttle.app" --args \
//     --user-data-dir=/tmp/sh-profile --remote-debugging-port=9333 --use-mock-keychain
import http from 'node:http';

const port = process.argv[2] || 9333;
const persistCheck = process.argv.includes('--persist-check');
const base = `http://127.0.0.1:${port}`;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let failures = 0;
const check = (ok, label, detail = '') => {
  if (!ok) failures++;
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${label}${detail ? ` - ${detail}` : ''}`);
};

// Local site: /set?v=X sets cookie v=X; /show shows document.cookie.
const site = http.createServer((req, res) => {
  const url = new URL(req.url, 'http://x');
  if (url.pathname === '/set') {
    res.setHeader('Set-Cookie', `v=${url.searchParams.get('v')}; Path=/; Max-Age=86400`);
  }
  res.setHeader('Content-Type', 'text/html');
  res.end(`<title>${url.pathname}</title><a id=l href="/show" target="_blank">show</a>`);
});
await new Promise((r) => site.listen(0, '127.0.0.1', r));
const sitePort = site.address().port;
const siteUrl = (path) => `http://127.0.0.1:${sitePort}${path}`;

let rpcId = 0;
async function rpc(method, params, headers = {}) {
  const r = await fetch(`${base}/mcp`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', Accept: 'application/json, text/event-stream', ...headers },
    body: JSON.stringify({ jsonrpc: '2.0', id: ++rpcId, method, ...(params ? { params } : {}) }),
  });
  const text = await r.text();
  let body = null;
  try { body = text ? JSON.parse(text) : null; } catch { body = text; }
  return { status: r.status, body };
}
async function tool(name, args = {}) {
  const { body } = await rpc('tools/call', { name, arguments: args });
  if (body.error) throw new Error(`${name}: ${body.error.message}`);
  const res = body.result;
  if (res.isError) return { error: res.content[0].text };
  return res.structuredContent ?? res;
}

// Minimal CDP page client.
async function evalIn(cdpUrl, expression) {
  const ws = new WebSocket(cdpUrl);
  await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
  const result = await new Promise((r) => {
    ws.onmessage = (e) => { const m = JSON.parse(e.data); if (m.id === 1) r(m); };
    ws.send(JSON.stringify({ id: 1, method: 'Runtime.evaluate', params: { expression, returnByValue: true, awaitPromise: true, userGesture: true } }));
  });
  ws.close();
  return result.result?.result?.value;
}

for (let i = 0; i < 60; i++) {
  try { await fetch(`${base}/json/version`); break; } catch { await sleep(500); }
}

if (persistCheck) {
  const s = await tool('open-session', { agent: 'smoke-persist' });
  check(!s.error, 'open-session smoke-persist', s.error);
  const t = await tool('allocate-tab', { sessionId: s.sessionId, url: siteUrl('/show') });
  await sleep(1500);
  const cookie = await evalIn(t.cdpUrl, 'document.cookie');
  check(cookie === 'v=persist', 'cookie survived restart', cookie);
  const pa = (await tool('list-agents')).agents.find((a) => a.id === 'smoke-persist');
  check(pa?.name === 'Persist Agent' && pa?.color === 'purple', 'identity survived restart', JSON.stringify(pa));
  await tool('close-session', { sessionId: s.sessionId });
  const d = await tool('delete-agent', { id: 'smoke-persist' });
  check(d.deleted, 'delete-agent smoke-persist');
  site.close();
  console.log(failures ? `\n${failures} FAILED` : '\nALL OK');
  process.exit(failures ? 1 : 0);
}

// Protocol.
const init = await rpc('initialize', { protocolVersion: '2025-06-18', capabilities: {}, clientInfo: { name: 'smoke', version: '1' } });
check(init.status === 200 && init.body.result?.serverInfo?.name === 'shuttle', 'initialize',
  `${init.body.result?.protocolVersion} ${init.body.result?.serverInfo?.version}`);
const note = await fetch(`${base}/mcp`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{"jsonrpc":"2.0","method":"notifications/initialized"}' });
check(note.status === 202, 'notification -> 202', String(note.status));
const list = await rpc('tools/list');
const names = list.body.result.tools.map((t) => t.name);
// 12 browser tools + 24 agent-browser page tools (config/agent-browser-tools.json).
check(names.length === 36 && names.includes('allocate-tab') && names.includes('snapshot'),
      'tools/list', `${names.length}: ${names.join(', ')}`);
const get = await fetch(`${base}/mcp`);
check(get.status === 405, 'GET -> 405', String(get.status));
const evil = await rpc('tools/list', undefined, { Origin: 'https://evil.example' });
check(evil.status === 403, 'foreign Origin -> 403', String(evil.status));
const bad = await rpc('tools/call', { name: 'nope' });
check(bad.body.error?.code === -32602, 'unknown tool -> -32602');

// Agents + identity.
for (const id of ['smoke-a', 'smoke-b', 'smoke-persist']) {
  const p = await tool('list-agents');
  if (p.agents.some((x) => x.id === id)) await tool('delete-agent', { id });
}
const ca0 = await tool('create-agent', { id: 'smoke-a', name: 'Smoke A', color: 'green' });
check(ca0.id === 'smoke-a' && ca0.name === 'Smoke A' && ca0.color === 'green', 'create-agent with identity', JSON.stringify(ca0));
const cb0 = await tool('create-agent', { id: 'smoke-b' });
check(cb0.name === 'smoke-b' && cb0.color && cb0.color !== 'grey', 'create-agent defaults (name = id, color)', JSON.stringify(cb0));
const cp0 = await tool('create-agent', { id: 'smoke-persist', name: 'Persist Agent', color: 'purple' });
check(cp0.id === 'smoke-persist', 'create-agent smoke-persist', cp0.error);
check(!!(await tool('create-agent', { id: 'smoke-a' })).error, 'duplicate create-agent rejected');
check(!!(await tool('create-agent', { id: 'Bad_Id' })).error, 'invalid id rejected');
check(!!(await tool('create-agent', { id: 'smoke-x', color: 'teal' })).error, 'invalid color rejected');
check(!!(await tool('create-agent', { id: 'smoke-x', name: '  ' })).error, 'empty name rejected');
check(!!(await tool('create-agent', { id: 'smoke-x', avatar: 'http://x/a.png' })).error, 'non-https avatar rejected');
check(!(await tool('list-agents')).agents.some((a) => a.id === 'smoke-x'), 'rejected agents not created');
const avatar = 'data:image/png;base64,iVBORw0KGgo=';
const up = await tool('update-agent', { id: 'smoke-b', name: 'Smoke B', color: 'orange', avatar });
check(up.name === 'Smoke B' && up.color === 'orange' && up.avatar === 'data:image/png (1 KB, set; content omitted)', 'update-agent (data avatar summarized)', JSON.stringify(up));
const up2 = await tool('update-agent', { id: 'smoke-b', avatar: '' });
check(up2.name === 'Smoke B' && up2.avatar === undefined, 'update-agent removes avatar, keeps name', JSON.stringify(up2));
check(!!(await tool('update-agent', { id: 'nope', name: 'X' })).error, 'update-agent unknown id rejected');

// Sessions + isolation.
const sa = await tool('open-session', { agent: 'smoke-a' });
const sb = await tool('open-session', { agent: 'smoke-b' });
check(!!(await tool('open-session', { agent: 'nope' })).error, 'open-session unknown agent rejected');
check(sa.sessionId && sb.sessionId && sa.cdpEndpoint, 'open-session x2', `${sa.sessionId} ${sb.sessionId} ${sa.cdpEndpoint}`);
const ta = await tool('allocate-tab', { sessionId: sa.sessionId, url: siteUrl('/set?v=a') });
const tb = await tool('allocate-tab', { sessionId: sb.sessionId, url: siteUrl('/set?v=b') });
check(ta.targetId && tb.targetId, 'allocate-tab x2', ta.error || tb.error);
await sleep(1500);
const ca = await evalIn(ta.cdpUrl, 'document.cookie');
const cb = await evalIn(tb.cdpUrl, 'document.cookie');
check(ca === 'v=a' && cb === 'v=b', 'cookies isolated per agent', `a=${ca} b=${cb}`);
const hidden = await evalIn(ta.cdpUrl, 'document.visibilityState');
check(hidden === 'visible', 'leased background tab renders as visible', hidden);

// Popup / target=_blank inheritance.
await evalIn(ta.cdpUrl, "document.getElementById('l').click()");
await evalIn(ta.cdpUrl, "window.open('/show', '_blank', 'noopener'); 1");
await sleep(2000);
const sessions = (await tool('list-sessions')).sessions;
const tabsA = sessions.find((s) => s.sessionId === sa.sessionId)?.tabs ?? [];
check(tabsA.length === 3, 'opened tabs join the session', `${tabsA.length} tabs`);
for (const t of tabsA.filter((t) => t.targetId !== ta.targetId)) {
  const c = await evalIn(t.cdpUrl, 'document.cookie');
  check(c === 'v=a', `opened tab keeps the partition (${t.url})`, c);
}

// Viewport, capture, navigation.
const vp = await tool('set-tab-viewport', { targetId: tb.targetId, width: 390, height: 844 });
await sleep(500);
const iw = await evalIn(tb.cdpUrl, 'innerWidth + "x" + innerHeight');
check(iw === '390x844', 'set-tab-viewport 390x844', `${JSON.stringify(vp)} page=${iw}`);
const { body: cap } = await rpc('tools/call', { name: 'capture-tab', arguments: { targetId: tb.targetId } });
const img = cap.result?.content?.[0];
check(img?.type === 'image' && img.data.length > 1000, 'capture-tab', `${img?.mimeType} ${img?.data?.length} b64 chars`);
const nav = await tool('navigate-tab', { targetId: tb.targetId, url: siteUrl('/show') });
check(nav.committed === true, 'navigate-tab waits for commit', JSON.stringify(nav));

// Persistence prep: cookie in smoke-persist.
const sp = await tool('open-session', { agent: 'smoke-persist' });
await tool('allocate-tab', { sessionId: sp.sessionId, url: siteUrl('/set?v=persist') });
await sleep(1500);
await tool('close-session', { sessionId: sp.sessionId });

// Cleanup.
const rel = await tool('release-tab', { targetId: tb.targetId });
check(rel.closed, 'release-tab');
check((await tool('close-session', { sessionId: sa.sessionId })).closed, 'close-session');
await tool('close-session', { sessionId: sb.sessionId });
for (const id of ['smoke-a', 'smoke-b']) {
  check((await tool('delete-agent', { id })).deleted, `delete-agent ${id}`);
}
const left = (await tool('list-agents')).agents.map((p) => p.id);
check(left.join() === 'smoke-persist', 'list-agents after cleanup', left.join());
check((await tool('list-sessions')).sessions.length === 0, 'no sessions left');

site.close();
console.log(failures ? `\n${failures} FAILED` : '\nALL OK (restart the browser, then run with --persist-check)');
process.exit(failures ? 1 : 0);
