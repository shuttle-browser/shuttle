// Fork 45: Page.bringToFront / Target.activateTarget over session + page CDP succeed (and a foreign tab is refused)
// without making Shuttle the frontmost app.
// Usage: node scripts/smoke-focus.mjs <port> [windowed]   (headless in scripts/smoke-all.sh)
//   windowed: also assert Shuttle is not the frontmost app (macOS `lsappinfo`; skipped elsewhere). The real focus
//   check is a separate, explicit windowed run against a visible browser.
import { execSync } from 'node:child_process';
import { args, cdp, check, cleanup, finish, mcp, port, skipCheck, sleep, gone } from './lib/smoke.mjs';

const windowed = args[0] === 'windowed';
const hasLsappinfo = process.platform === 'darwin';
const front = () => { try { return execSync('lsappinfo info -only name `lsappinfo front`').toString().trim(); } catch { return '?'; } };
const IDS = ['smoke-focus-a', 'smoke-focus-b'], opened = [];
cleanup(async () => {
  for (const sessionId of opened) await mcp('close-session', { sessionId }).catch(gone);
  for (const id of IDS) await mcp('delete-agent', { id }).catch(gone);
});
const f0 = hasLsappinfo ? front() : '?';
for (const id of IDS) { await mcp('delete-agent', { id }).catch(() => {}); await mcp('create-agent', { id, name: id }); }
const sa = await mcp('open-session', { agent: IDS[0] });
const sb = await mcp('open-session', { agent: IDS[1] });
opened.push(sa.sessionId, sb.sessionId);
const tb = await mcp('allocate-tab', { sessionId: sb.sessionId, url: 'data:text/html,B' });
const c = await cdp(sa.cdpEndpoint);
const t1 = (await c.send('Target.createTarget', { url: 'data:text/html,<title>one</title>1' })).result?.targetId;
const t2 = (await c.send('Target.createTarget', { url: 'data:text/html,<title>two</title>2' })).result?.targetId;
check('two tabs created over session CDP', t1 && t2);
await sleep(2000);
const f1 = hasLsappinfo ? front() : '?';
const s1 = (await c.send('Target.attachToTarget', { targetId: t1, flatten: true })).result?.sessionId;
for (let i = 0; i < 3; i++) {
  const r1 = await c.send('Page.bringToFront', {}, s1);
  check(`Page.bringToFront tab one #${i}`, !r1.error, JSON.stringify(r1.error ?? r1.result));
  const r2 = await c.send('Target.activateTarget', { targetId: t2 });
  check(`Target.activateTarget tab two #${i}`, !r2.error, JSON.stringify(r2.error ?? r2.result));
}
const vis = await c.send('Runtime.evaluate', { expression: 'document.visibilityState', returnByValue: true }, s1);
check('tab one still renders (visible)', vis.result?.result?.value === 'visible', vis.result?.result?.value);
const foreign = await c.send('Target.activateTarget', { targetId: tb.targetId });
check('activateTarget on a foreign tab is refused', !!foreign.error, foreign.error?.message);
// Page connection (per-tab endpoint, no sessionId).
const p = await cdp(`ws://127.0.0.1:${port}/devtools/page/${t1}`);
const rp = await p.send('Page.bringToFront');
check('Page.bringToFront on a page connection', !rp.error, JSON.stringify(rp.error ?? rp.result));
p.close();
if (windowed) {
  await sleep(1000);
  const f2 = front();
  console.log(`frontmost app: before=${f0} after-allocate=${f1} after-bringToFront=${f2}`);
  if (hasLsappinfo) check('Shuttle is not the frontmost app after bringToFront', f2 !== '?' && !/Shuttle/.test(f2), `${f1} -> ${f2}`);
  else skipCheck('Shuttle is not the frontmost app', 'no lsappinfo on this platform');
}
await finish();
