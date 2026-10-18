// MCP smoke for Shuttle's in-browser MCP server (POST /mcp on the CDP port).
// Usage: node scripts/smoke-mcp.mjs <port> --phase1|--phase2   (run by scripts/smoke-all.sh, which restarts the
//   browser on the same profile between the phases)
//   --phase1: protocol, agents (identity), sessions, tab isolation, popup inheritance, viewport, capture, navigation,
//             origin check, cleanup. Leaves agents "smoke-persist" (persistent + session cookie) and
//             "smoke-nopersist" (persistSessionCookies false; persistent + session cookie) for the restart check.
//   --phase2: smoke-persist's cookies (session cookie included) and identity survived the restart, smoke-nopersist
//             kept its persistent cookie but not its session cookie; then deletes both agents.
import http from 'node:http';
import { args, base, cdp, check as chk, cleanup, finish, rpc, sleep } from './lib/smoke.mjs';

const phase = args[0];
if (!['--phase1', '--phase2'].includes(phase)) { console.error('usage: smoke-mcp.mjs <port> --phase1|--phase2'); process.exit(2); }
const check = (ok, label, detail) => chk(label, ok, detail);

// Local site: /set?v=X sets cookie v=X (1 day), &s=Y adds session cookie s=Y (no Expires/Max-Age); /show shows
// document.cookie.
const site = http.createServer((req, res) => {
  const url = new URL(req.url, 'http://x');
  if (url.pathname === '/set') {
    const set = [`v=${url.searchParams.get('v')}; Path=/; Max-Age=86400`];
    if (url.searchParams.has('s')) set.push(`s=${url.searchParams.get('s')}; Path=/`);
    res.setHeader('Set-Cookie', set);
  }
  res.setHeader('Content-Type', 'text/html');
  res.end(`<title>${url.pathname}</title><a id=l href="/show" target="_blank">show</a>`);
});
await new Promise((r) => site.listen(0, '127.0.0.1', r));
cleanup(() => site.close());
const sitePort = site.address().port;
const siteUrl = (path) => `http://127.0.0.1:${sitePort}${path}`;

// tool(): result, or {error} for a tool error (several checks expect one); protocol errors throw.
async function tool(name, args = {}) {
  const { body } = await rpc('tools/call', { name, arguments: args });
  if (!body?.result) throw new Error(`${name}: ${JSON.stringify(body?.error ?? body).slice(0, 200)}`);
  if (body.result.isError) return { error: body.result.content[0].text };
  return body.result.structuredContent ?? body.result;
}
const opened = [], agents = phase === '--phase2' ? ['smoke-persist', 'smoke-nopersist'] : ['smoke-a', 'smoke-b'];  // closed/deleted at exit
const openSession = async (agent) => { const s = await tool('open-session', { agent }); if (s.sessionId) opened.push(s.sessionId); return s; };
const must = (r) => { if (r?.error && !/^Unknown (agent|session)/.test(r.error)) throw new Error(r.error); };
cleanup(async () => {
  for (const sessionId of opened) must(await tool('close-session', { sessionId }));
  for (const id of agents) must(await tool('delete-agent', { id }));
});
async function evalIn(cdpUrl, expression) {
  const c = await cdp(cdpUrl);
  try {
    return (await c.send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true, userGesture: true })).result?.result?.value;
  } finally { c.close(); }
}
// document.cookie of the agent's tab on /show, as {name: value}.
async function cookiesOf(agent) {
  const s = await openSession(agent);
  if (s.error) return { error: s.error };
  const t = await tool('allocate-tab', { sessionId: s.sessionId, url: siteUrl('/show') });
  await sleep(1500);
  const raw = await evalIn(t.cdpUrl, 'document.cookie');
  await tool('close-session', { sessionId: s.sessionId });
  return Object.fromEntries((raw || '').split('; ').filter(Boolean).map((kv) => kv.split('=')));
}

if (phase === '--phase2') {
  const cp = await cookiesOf('smoke-persist');
  check(!cp.error, 'open-session smoke-persist', cp.error);
  check(cp.v === 'persist', 'cookie survived restart', JSON.stringify(cp));
  check(cp.s === 'persist', 'session cookie survived restart', JSON.stringify(cp));
  const cn = await cookiesOf('smoke-nopersist');
  check(cn.v === 'kept' && cn.s === undefined, 'opted-out agent: session cookie dropped, persistent kept', JSON.stringify(cn));
  const all = (await tool('list-agents')).agents;
  const pa = all.find((a) => a.id === 'smoke-persist');
  check(pa?.name === 'Persist Agent' && pa?.color === 'purple' && pa?.persistSessionCookies === true,
    'identity survived restart', JSON.stringify(pa));
  const na = all.find((a) => a.id === 'smoke-nopersist');
  check(na?.persistSessionCookies === false, 'persistSessionCookies opt-out survived restart', JSON.stringify(na));
  for (const id of ['smoke-persist', 'smoke-nopersist']) {
    check((await tool('delete-agent', { id })).deleted, `delete-agent ${id}`);
  }
  await finish();
}

// Protocol.
const init = await rpc('initialize', { protocolVersion: '2025-06-18', capabilities: {}, clientInfo: { name: 'smoke', version: '1' } });
check(init.status === 200 && init.body.result?.serverInfo?.name === 'shuttle', 'initialize',
  `${init.body.result?.protocolVersion} ${init.body.result?.serverInfo?.version}`);
const note = await fetch(`${base}/mcp`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{"jsonrpc":"2.0","method":"notifications/initialized"}' });
check(note.status === 202, 'notification -> 202', String(note.status));
const list = await rpc('tools/list');
const names = list.body.result.tools.map((t) => t.name);
// Browser tools + a few agent-browser page tools (config/agent-browser-tools.json); additions don't break this.
const REQUIRED = ['create-agent', 'update-agent', 'delete-agent', 'list-agents', 'open-session', 'close-session',
  'list-sessions', 'allocate-tab', 'release-tab', 'navigate-tab', 'set-tab-viewport', 'capture-tab', 'snapshot', 'click', 'fill'];
const missing = REQUIRED.filter((n) => !names.includes(n));
check(missing.length === 0, 'tools/list has the required tools', `${names.length} tools${missing.length ? `, missing ${missing}` : ''}`);
const get = await fetch(`${base}/mcp`);
check(get.status === 405, 'GET -> 405', String(get.status));
const evil = await rpc('tools/list', undefined, { Origin: 'https://evil.example' });
check(evil.status === 403, 'foreign Origin -> 403', String(evil.status));
const bad = await rpc('tools/call', { name: 'nope' });
check(bad.body.error?.code === -32602, 'unknown tool -> -32602');

// Agents + identity.
for (const id of ['smoke-a', 'smoke-b', 'smoke-persist', 'smoke-nopersist']) {
  const p = await tool('list-agents');
  if (p.agents.some((x) => x.id === id)) await tool('delete-agent', { id });
}
const ca0 = await tool('create-agent', { id: 'smoke-a', name: 'Smoke A', color: 'green' });
check(ca0.id === 'smoke-a' && ca0.name === 'Smoke A' && ca0.color === 'green', 'create-agent with identity', JSON.stringify(ca0));
const cb0 = await tool('create-agent', { id: 'smoke-b' });
check(cb0.name === 'smoke-b' && cb0.color && cb0.color !== 'grey', 'create-agent defaults (name = id, color)', JSON.stringify(cb0));
const cp0 = await tool('create-agent', { id: 'smoke-persist', name: 'Persist Agent', color: 'purple' });
check(cp0.id === 'smoke-persist' && cp0.persistSessionCookies === true, 'create-agent smoke-persist (session cookies kept by default)',
  cp0.error ?? JSON.stringify(cp0));
const cn0 = await tool('create-agent', { id: 'smoke-nopersist', name: 'No Persist', persistSessionCookies: false });
check(cn0.id === 'smoke-nopersist' && cn0.persistSessionCookies === false, 'create-agent persistSessionCookies false',
  cn0.error ?? JSON.stringify(cn0));
check(!!(await tool('create-agent', { id: 'smoke-x', persistSessionCookies: 'no' })).error, 'non-boolean persistSessionCookies rejected');
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
const up3 = await tool('update-agent', { id: 'smoke-b', persistSessionCookies: false });
const up4 = await tool('update-agent', { id: 'smoke-b', persistSessionCookies: true });
check(up3.persistSessionCookies === false && up4.persistSessionCookies === true && up4.name === 'Smoke B',
  'update-agent persistSessionCookies', `${JSON.stringify(up3)} ${JSON.stringify(up4)}`);

// Sessions + isolation.
const sa = await openSession('smoke-a');
const sb = await openSession('smoke-b');
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

// Persistence prep: a persistent and a session cookie in smoke-persist and smoke-nopersist.
for (const [agent, v, sv] of [['smoke-persist', 'persist', 'persist'], ['smoke-nopersist', 'kept', 'gone']]) {
  const sp = await openSession(agent);
  const tp = await tool('allocate-tab', { sessionId: sp.sessionId, url: siteUrl(`/set?v=${v}&s=${sv}`) });
  await sleep(1500);
  const c = await evalIn(tp.cdpUrl, 'document.cookie');
  check(c?.split('; ').sort().join('; ') === `s=${sv}; v=${v}`, `cookies set in ${agent}`, c);
  await tool('close-session', { sessionId: sp.sessionId });
}

// Cleanup.
const rel = await tool('release-tab', { targetId: tb.targetId });
check(rel.closed, 'release-tab');
check((await tool('close-session', { sessionId: sa.sessionId })).closed, 'close-session');
await tool('close-session', { sessionId: sb.sessionId });
for (const id of ['smoke-a', 'smoke-b']) {
  check((await tool('delete-agent', { id })).deleted, `delete-agent ${id}`);
}
const left = (await tool('list-agents')).agents.map((p) => p.id).sort();
check(left.join() === 'smoke-nopersist,smoke-persist', 'list-agents after cleanup', left.join());
check((await tool('list-sessions')).sessions.length === 0, 'no sessions left');

await finish();
