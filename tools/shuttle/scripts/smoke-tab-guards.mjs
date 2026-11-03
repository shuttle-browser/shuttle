// Tab and session guards (docs/MCP.md "Session lifetime"): node scripts/smoke-tab-guards.mjs <port>
// Run by scripts/smoke-all.sh on its own launch with --shuttle-session-idle-timeout=8s.
//   1. A fresh session-endpoint connection lists the allocate-tab tab (Playwright pages() at connect).
//   2. A connection that creates no targets (agent-browser style) closes nothing when it ends.
//   3. Tabs a connection created (Target.createTarget) close when it ends; the allocate-tab tab and the session stay.
//   4. open-session warns when the agent already has open sessions and lists them.
//   5. Idle timeout: MCP activity keeps a session open; without it, the session and its tabs close.
//   6. update-session: idleTimeoutMinutes 0 keeps an idle session open; invalid values are rejected.
//   7. A tool call still running (wait-for longer than the timeout) keeps its session open.
//   8. beforeunload can't keep a tab: neither the connection cleanup nor close-session leaves one behind.
//   9. Headless (--headless arg): an activated session tab doesn't count as a person looking; it still idles out.
// Tab existence is checked on the browser's own target list, not list-sessions (a tab can outlive its session).
import { args, base, browserWs, cdp, check, cleanup, evaluate, finish, gone, json, mcp, sleep } from './lib/smoke.mjs';

const AGENT = 'smoke-tg';
const IDLE_MS = 8000;      // the launch's --shuttle-session-idle-timeout
const CHECK_MS = 5000;     // SessionManager's idle check interval
const opened = [];
cleanup(async () => {
  for (const sessionId of opened) await mcp('close-session', { sessionId }).catch(gone);
  await mcp('delete-agent', { id: AGENT }).catch(gone);
});

const sessionOf = async (id) => (await mcp('list-sessions')).sessions.find((s) => s.sessionId === id);
const tabIds = (session) => (session?.tabs ?? []).map((t) => t.targetId);
const tabExists = async (targetId) => (await json('/json/list')).some((t) => t.id === targetId);
const waitGone = async (targetId, ms = 5000) => {
  for (const until = Date.now() + ms; Date.now() < until; await sleep(250)) if (!(await tabExists(targetId))) return true;
  return !(await tabExists(targetId));
};
// beforeunload needs a user gesture to show its dialog: click the page through `client`; true once the page has one.
const BEFOREUNLOAD = 'data:text/html,<title>bu</title><script>onbeforeunload=(e)=>{e.preventDefault();return e.returnValue="stay"}</script>bu';
const clickIn = async (client, targetId) => {
  const sid = (await client.send('Target.attachToTarget', { targetId, flatten: true })).result?.sessionId;
  if (!sid) return false;
  for (const type of ['mousePressed', 'mouseReleased']) {
    const r = await client.send('Input.dispatchMouseEvent', { type, x: 20, y: 20, button: 'left', clickCount: 1 }, sid);
    if (r.error) return false;
  }
  return (await evaluate(client, sid, 'navigator.userActivation.hasBeenActive')) === true;
};
const open = async (args = {}) => {
  const s = await mcp('open-session', { agent: AGENT, ...args });
  opened.push(s.sessionId);
  return s;
};

try { await mcp('delete-agent', { id: AGENT }); } catch {}
await mcp('create-agent', { id: AGENT, name: AGENT });

// 1. The allocate-tab tab is there at connect.
const s1 = await open({ idleTimeoutMinutes: 0 });
check('open-session idleTimeoutMinutes 0 is kept', s1.idleTimeoutMinutes === 0, JSON.stringify(s1.idleTimeoutMinutes));
check('first session has no warning', !s1.warning, s1.warning);
const kept = await mcp('allocate-tab', { sessionId: s1.sessionId, url: 'data:text/html,<title>kept</title>kept' });
const c1 = await cdp(s1.cdpEndpoint);
const listed = (await c1.send('Target.getTargets')).result?.targetInfos ?? [];
check('a new connection lists the allocate-tab tab', listed.some((t) => t.targetId === kept.targetId),
  listed.map((t) => t.targetId).join(','));

// 2. A connection that creates nothing closes nothing.
const c2 = await cdp(s1.cdpEndpoint);
await c2.send('Target.getTargets');
c2.close();
await sleep(1500);
check('a connection without createTarget closes nothing', tabIds(await sessionOf(s1.sessionId)).includes(kept.targetId));

// 3. Tabs a connection created close with it.
const created = (await c1.send('Target.createTarget', { url: 'data:text/html,<title>made</title>made' })).result?.targetId;
await sleep(1000);
check('createTarget tab is a session tab', tabIds(await sessionOf(s1.sessionId)).includes(created));
c1.close();
let after;
for (let i = 0; i < 20; i++) {
  await sleep(250);
  after = await sessionOf(s1.sessionId);
  if (!tabIds(after).includes(created)) break;
}
check('createTarget tab closes with its connection', !!after && !tabIds(after).includes(created), tabIds(after).join(','));
check('allocate-tab tab stays after the disconnect', tabIds(after).includes(kept.targetId));
check('session stays open after the disconnect', !!after);

// 4. Warning on a second session.
const s2 = await open({ idleTimeoutMinutes: 0 });
check('second open-session warns', typeof s2.warning === 'string' && s2.warning.includes(AGENT), s2.warning);
check('warning lists the open session', (s2.openSessions ?? []).some((o) => o.sessionId === s1.sessionId),
  JSON.stringify(s2.openSessions));
await mcp('close-session', { sessionId: s2.sessionId });

// 5. Idle timeout (8 s from the launch switch).
const s3 = await open();
check('default idle timeout from the switch', Math.abs(s3.idleTimeoutMinutes - IDLE_MS / 60000) < 0.01,
  JSON.stringify(s3.idleTimeoutMinutes));
const idleTab = await mcp('allocate-tab', { sessionId: s3.sessionId, url: 'data:text/html,idle' });
for (let t = 0; t < IDLE_MS + CHECK_MS + 2000; t += 2000) {
  // Activity: a tool call that names one of its tabs (set-tab-viewport, not capture-tab: a
  // background data: tab may have no frame to capture yet in headless).
  await mcp('set-tab-viewport', { targetId: idleTab.targetId, width: 800, height: 600 });
  await sleep(2000);
}
check('MCP activity keeps the session open', !!(await sessionOf(s3.sessionId)));
let closed = false;
for (let t = 0; t < IDLE_MS + 2 * CHECK_MS + 3000 && !closed; t += 1000) {
  await sleep(1000);
  closed = !(await sessionOf(s3.sessionId));
}
check('idle session closes', closed);
check('its tab closes too', await waitGone(idleTab.targetId));

// 6. update-session.
const s4 = await open();
let rejected = '';
try { await mcp('update-session', { sessionId: s4.sessionId, idleTimeoutMinutes: -1 }); } catch (e) { rejected = e.message; }
check('update-session rejects a negative timeout', rejected.includes('idleTimeoutMinutes'), rejected);
const u = await mcp('update-session', { sessionId: s4.sessionId, idleTimeoutMinutes: 0 });
check('update-session sets idleTimeoutMinutes 0', u.idleTimeoutMinutes === 0, JSON.stringify(u.idleTimeoutMinutes));
await sleep(IDLE_MS + 2 * CHECK_MS);
check('session with idleTimeoutMinutes 0 stays open', !!(await sessionOf(s4.sessionId)));
check('session opened with idleTimeoutMinutes 0 stays open', !!(await sessionOf(s1.sessionId)));

// 7. A running tool call is activity until it answers (a 20 s wait-for on an 8 s timeout).
const s5 = await open();
const busyTab = await mcp('allocate-tab', { sessionId: s5.sessionId, url: 'data:text/html,busy' });
const WAIT_MS = IDLE_MS + 2 * CHECK_MS + 2000;
const r = await fetch(`${base}/mcp`, { method: 'POST', signal: AbortSignal.timeout(WAIT_MS + 30_000),
  headers: { 'Content-Type': 'application/json', Accept: 'application/json, text/event-stream' },
  body: JSON.stringify({ jsonrpc: '2.0', id: 1, method: 'tools/call',
    params: { name: 'wait-for', arguments: { targetId: busyTab.targetId, ms: WAIT_MS } } }) });
const waited = await r.json().catch(() => null);
check('a wait-for longer than the timeout completes', r.status === 200 && waited?.result && !waited.result.isError,
  JSON.stringify(waited?.result?.content?.[0]?.text ?? waited).slice(0, 160));
check('the session stays open while the call runs', !!(await sessionOf(s5.sessionId)) && await tabExists(busyTab.targetId));
await mcp('close-session', { sessionId: s5.sessionId });

// 8. beforeunload keeps no tab.
const s6 = await open({ idleTimeoutMinutes: 0 });
const c6 = await cdp(s6.cdpEndpoint);
const buCreated = (await c6.send('Target.createTarget', { url: BEFOREUNLOAD })).result?.targetId;
await sleep(1000);
check('the createTarget beforeunload tab has a user gesture', await clickIn(c6, buCreated));
c6.close();
check('a createTarget tab with beforeunload closes with its connection', await waitGone(buCreated));
const buLeased = await mcp('allocate-tab', { sessionId: s6.sessionId, url: BEFOREUNLOAD });
const c6b = await cdp(s6.cdpEndpoint);
await sleep(1000);
check('the allocate-tab beforeunload tab has a user gesture', await clickIn(c6b, buLeased.targetId));
await mcp('close-session', { sessionId: s6.sessionId });
c6b.close();
check('close-session closes a tab with beforeunload', await waitGone(buLeased.targetId));

// 9. Headless: activation isn't a person looking.
if (args.includes('--headless')) {
  const s7 = await open();
  const front = await mcp('allocate-tab', { sessionId: s7.sessionId, url: 'data:text/html,front' });
  const b = await cdp(await browserWs());
  await b.send('Target.activateTarget', { targetId: front.targetId });
  b.close();
  let gone7 = false;
  for (let t = 0; t < IDLE_MS + 2 * CHECK_MS + 3000 && !gone7; t += 1000) {
    await sleep(1000);
    gone7 = !(await sessionOf(s7.sessionId));
  }
  check('headless: an activated session tab still idles out', gone7);
  check('headless: and its tab closes', await waitGone(front.targetId));
}

await finish();
