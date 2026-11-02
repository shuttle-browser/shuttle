// Pinned home tab (fork commit 53): node scripts/smoke-home-tab.mjs <port> [--after-restart] (run by scripts/smoke-all.sh).
//   1. An agent tab opens a window: the window gets exactly one chrome://agents home tab.
//   2. The browser-wide endpoint can't close it: Target.closeTarget, Page.close and /json/close leave it alive; the
//      first two reply an error (fork 55).
//   3. Releasing the agent's last tab closes the window the agent opened (home tab included); a window it joined stays.
//   4. A window opened from the browser endpoint (not an agent's) keeps its home tab when its last other tab closes.
//   5. Opening chrome://agents again adds no second agents tab and lands on the home tab, even next to an active
//      ordinary chrome://agents tab (fork 54).
//   6. CDP Page.navigate away from the home tab is sent back to chrome://agents (fork 54).
// Group focus on collapse/close (fork 54) needs the tab strip UI: manual check.
// --after-restart (second launch on the same profile): no window holds more than one chrome://agents tab.
// Tab strip details (index 0, pinned look, Cmd+W, drag) aren't visible over CDP: manual check, REMOVAL-LOG fork 53.
import { args, browserWs, cdp, check, cleanup, evaluate, finish, gone, json, mcp, sleep } from './lib/smoke.mjs';

const isAgents = (t) => t.type === 'page' && /^chrome:\/\/agents\b/.test(t.url);
const browser = await cdp(await browserWs());
const targets = async () => (await browser.send('Target.getTargets')).result.targetInfos;
const windowOf = async (targetId) => (await browser.send('Browser.getWindowForTarget', { targetId })).result?.windowId;
async function homesByWindow() {
  const map = new Map();
  for (const t of (await targets()).filter(isAgents)) {
    const w = await windowOf(t.targetId);
    map.set(w, [...(map.get(w) ?? []), t]);
  }
  return map;
}
async function waitFor(fn, ms = 5000) {
  const until = Date.now() + ms;
  for (;;) { const v = await fn(); if (v || Date.now() > until) return v; await sleep(250); }
}
async function alive(targetId) {
  if (!(await targets()).some((t) => t.targetId === targetId)) return false;
  const att = await browser.send('Target.attachToTarget', { targetId, flatten: true });
  const sid = att.result?.sessionId;
  if (!sid) return false;
  try { return (await evaluate(browser, sid, 'document.readyState')) === 'complete'; }
  finally { await browser.send('Target.detachFromTarget', { sessionId: sid }); }
}

if (args.includes('--after-restart')) {
  await sleep(1500);
  const dupes = [...(await homesByWindow()).values()].filter((list) => list.length > 1);
  check('after restart: at most one chrome://agents tab per window', dupes.length === 0,
    dupes.map((l) => l.map((t) => t.url).join(',')).join('; '));
  await finish();
}

// 1. Agent window with its home tab.
const agent = 'smoke-home-tab';
cleanup(async () => { await mcp('delete-agent', { id: agent }).catch(gone); });
await mcp('create-agent', { id: agent, persistSessionCookies: false });
const s = await mcp('open-session', { agent });
const windowsBefore = new Set(await Promise.all((await targets()).filter((t) => t.type === 'page').map((t) => windowOf(t.targetId))));
const tab = await mcp('allocate-tab', { sessionId: s.sessionId, url: 'about:blank' });
const agentWindow = await windowOf(tab.targetId);
// Earlier probes may have left a window open: the agent's tab then joins it, and that window isn't the agent's.
const agentOpened = !windowsBefore.has(agentWindow);
const homes = await waitFor(async () => (await homesByWindow()).get(agentWindow));
check('agent window has exactly one home tab', homes?.length === 1, JSON.stringify(homes?.map((t) => t.url)));
const home = homes?.[0];
if (!home) await finish();
check('home tab is live', await waitFor(() => alive(home.targetId)));

// 2. The browser endpoint can't close it.
const closeReply = await browser.send('Target.closeTarget', { targetId: home.targetId });
await sleep(1000);
// Fork 55: the reply is an error, not Chromium's fire-and-forget success.
check('Target.closeTarget on the home tab replies an error', !!closeReply.error,
  JSON.stringify(closeReply.error ?? closeReply.result));
check('home tab alive after Target.closeTarget', await alive(home.targetId));
const att = await browser.send('Target.attachToTarget', { targetId: home.targetId, flatten: true });
const pageClose = await browser.send('Page.close', {}, att.result?.sessionId);
await sleep(1000);
check('Page.close on the home tab replies an error', !!pageClose.error, JSON.stringify(pageClose.result));
check('home tab alive after Page.close', await alive(home.targetId));
await json(`/json/close/${home.targetId}`).catch(() => {});
await sleep(1000);
check('home tab alive after /json/close', await alive(home.targetId));

// 3. The agent's window goes with its last tab (a window it only joined stays).
await mcp('release-tab', { targetId: tab.targetId });
if (agentOpened) {
  const closed = await waitFor(async () => !(await targets()).some((t) => t.targetId === home.targetId), 8000);
  check('agent window closes once only its home tab is left', closed);
} else {
  await sleep(1500);
  check('a window the agent joined keeps its home tab', await alive(home.targetId));
}

// 4. A window the agent didn't open stays with its home tab.
const blank = (await browser.send('Target.createTarget', { url: 'about:blank', newWindow: true })).result?.targetId;
const userWindow = blank && await windowOf(blank);
const userHomes = await waitFor(async () => (await homesByWindow()).get(userWindow));
check('new window has exactly one home tab', userHomes?.length === 1);
await browser.send('Target.closeTarget', { targetId: blank });
await sleep(1500);
check('window keeps its home tab after its last other tab closes',
  !!userHomes?.[0] && await alive(userHomes[0].targetId));

// 5. Fork 54: opening chrome://agents switches to the home tab instead of adding a copy.
const userHome = userHomes?.[0];
if (userHome) {
  await browser.send('Target.createTarget', { url: 'chrome://agents/' });
  await sleep(1500);
  const after = (await homesByWindow()).get(userWindow) ?? [];
  check('chrome://agents opens no second agents tab in the window', after.length === 1,
    JSON.stringify(after.map((t) => t.url)));

  // 5b. Even with an ordinary chrome://agents tab active (Page.navigate bypasses Navigate()), the switch goes to the
  // marked home tab, not to the first URL match.
  const copy = (await browser.send('Target.createTarget', { url: 'about:blank' })).result?.targetId;
  const ac = await browser.send('Target.attachToTarget', { targetId: copy, flatten: true });
  await browser.send('Page.navigate', { url: 'chrome://agents/' }, ac.result?.sessionId);
  await sleep(1000);
  const switched = (await browser.send('Target.createTarget', { url: 'chrome://agents/#again' })).result?.targetId;
  check('chrome://agents switches to the home tab, not an ordinary copy', switched === userHome.targetId,
    `${switched} vs home ${userHome.targetId}`);
  // Other targets fall through to Chromium's close (fork 55 guards only the home tab).
  const copyClose = await browser.send('Target.closeTarget', { targetId: copy });
  check('Target.closeTarget still closes an ordinary tab',
    !copyClose.error && await waitFor(async () => !(await alive(copy)), 5000), JSON.stringify(copyClose.error));

  // 6. Fork 54: a navigation that bypasses Navigate() (CDP Page.navigate) is sent back to chrome://agents.
  const a2 = await browser.send('Target.attachToTarget', { targetId: userHome.targetId, flatten: true });
  await browser.send('Page.navigate', { url: 'about:blank#away' }, a2.result?.sessionId);
  const back = await waitFor(async () => (await targets()).find((t) => t.targetId === userHome.targetId && isAgents(t)), 5000);
  check('home tab returns to chrome://agents after Page.navigate', !!back);
}
await finish();
