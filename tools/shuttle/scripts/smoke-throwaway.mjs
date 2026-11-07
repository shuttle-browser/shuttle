// Throwaway agents (docs/plans/THROWAWAY-AGENTS.md): node scripts/smoke-throwaway.mjs <port> <userDataDir>
// Run by scripts/smoke-all.sh on its own launch with --shuttle-throwaway-idle-timeout=6s
// --shuttle-max-throwaway-agents=4.
//   1. create-agent { throwaway: true }: generated tmp- id, throwaway, idleMinutes 15, expiresAt; listed.
//   2. Refused: id with throwaway, avatar, persistSessionCookies, idleMinutes out of range, a persistent "tmp-" id,
//      idleMinutes on a persistent agent; update-agent takes name/color/idleMinutes, refuses avatar.
//   3. In-memory storage: a cookie set in its session is readable there; on disk only GPU caches (deleted with it).
//   4. state-save is refused; a save-text file is deleted with the agent (needs the agent-browser sidecar).
//   5. Expiry: activity keeps an agent with a session; idle, the agent and its tab go; one without sessions goes too.
//   6. The cap: the fifth live throwaway is refused.
import fs from 'node:fs';
import path from 'node:path';
import { args, cdp, check, cleanup, finish, gone, json, mcp, skipCheck, sleep } from './lib/smoke.mjs';

const udd = args[0];
if (!udd) { console.error('usage: smoke-throwaway.mjs <port> <userDataDir>'); process.exit(2); }
const IDLE_MS = 6000;      // the launch's --shuttle-throwaway-idle-timeout
const CHECK_MS = 1500;     // ShuttleService checks every timeout/4 with the override
const PERSISTENT = 'smoke-ta-persistent';
const created = [];
cleanup(async () => {
  for (const id of created) await mcp('delete-agent', { id }).catch(gone);
  await mcp('delete-agent', { id: PERSISTENT }).catch(gone);
});

const errorOf = async (name, a) => { try { await mcp(name, a); return ''; } catch (e) { return e.message; } };
const listed = async (id) => (await mcp('list-agents')).agents.find((a) => a.id === id);
const tabExists = async (targetId) => (await json('/json/list')).some((t) => t.id === targetId);
const throwaway = async (extra = {}) => {
  const agent = await mcp('create-agent', { throwaway: true, ...extra });
  created.push(agent.id);
  return agent;
};
const waitUntil = async (fn, ms) => {
  for (const until = Date.now() + ms; Date.now() < until; await sleep(250)) if (await fn()) return true;
  return fn();
};
const profileDir = fs.existsSync(path.join(udd, 'Default')) ? path.join(udd, 'Default') : udd;

// 1. Create.
const a = await throwaway({ name: 'Smoke throwaway', color: 'cyan' });
check('generated tmp- id', /^tmp-[0-9a-f]{12}$/.test(a.id), a.id);
check('marked throwaway', a.throwaway === true, JSON.stringify(a.throwaway));
check('idleMinutes defaults to 15', a.idleMinutes === 15, JSON.stringify(a.idleMinutes));
check('expiresAt in the future', Date.parse(a.expiresAt) > Date.now() - 1000, a.expiresAt);
check('name and color kept', a.name === 'Smoke throwaway' && a.color === 'cyan', `${a.name} ${a.color}`);
check('listed by list-agents', (await listed(a.id))?.throwaway === true);

// 2. Refusals.
check('id refused with throwaway', /generated id/.test(await errorOf('create-agent', { throwaway: true, id: 'x' })));
check('avatar refused', /no avatar/.test(await errorOf('create-agent', { throwaway: true, avatar: 'https://example.com/a.png' })));
check('persistSessionCookies refused',
  /persistSessionCookies/.test(await errorOf('create-agent', { throwaway: true, persistSessionCookies: false })));
check('idleMinutes 0 refused', /idleMinutes/.test(await errorOf('create-agent', { throwaway: true, idleMinutes: 0 })));
check('idleMinutes 1441 refused', /idleMinutes/.test(await errorOf('create-agent', { throwaway: true, idleMinutes: 1441 })));
check('persistent tmp- id refused', /reserved/.test(await errorOf('create-agent', { id: 'tmp-mine' })));
check('idleMinutes on a persistent agent refused',
  /throwaway agents only/.test(await errorOf('create-agent', { id: PERSISTENT, idleMinutes: 5 })));
const updated = await mcp('update-agent', { id: a.id, name: 'Renamed', idleMinutes: 30 });
check('update-agent sets name and idleMinutes', updated.name === 'Renamed' && updated.idleMinutes === 30,
  `${updated.name} ${updated.idleMinutes}`);
check('update-agent refuses avatar', /no avatar/.test(await errorOf('update-agent', { id: a.id, avatar: '' })));

// 3. In-memory storage.
const s = await mcp('open-session', { agent: a.id, idleTimeoutMinutes: 0 });
const tab = await mcp('allocate-tab', { sessionId: s.sessionId, url: 'data:text/html,<title>ta</title>ta' });
const client = await cdp(s.cdpEndpoint);
// The session endpoint's default browser context is the agent's storage.
const set = await client.send('Storage.setCookies', { cookies: [
  { name: 'ta', value: '1', domain: 'throwaway.test', path: '/', secure: true, expires: Date.now() / 1000 + 3600 }] });
const got = await client.send('Storage.getCookies', {});
check('cookie set in its storage', !set.error && got.result?.cookies?.some((c) => c.name === 'ta'),
  JSON.stringify(set.error ?? got.error ?? ''));
client.close?.();
// Chromium keeps only the GPU shader caches of an in-memory partition on disk.
const GPU_CACHES = new Set(['GPUCache', 'DawnGraphiteCache', 'DawnWebGPUCache']);
const partitionDir = path.join(profileDir, 'Storage', 'ext', `shuttle-${a.id}`);
const onDisk = fs.existsSync(path.join(partitionDir, 'def')) ? fs.readdirSync(path.join(partitionDir, 'def')) : [];
const stray = onDisk.filter((d) => !GPU_CACHES.has(d));
check('no storage on disk (GPU caches only)', stray.length === 0, stray.join(','));

// 4. state-save refused; save-text file deleted with the agent.
check('state-save refused', /throwaway/.test(await errorOf('state-save', { targetId: tab.targetId, name: 's1' })));
await mcp('navigate-tab', { targetId: tab.targetId, url: 'data:text/html,<p id=k>tmp-secret-42</p>' });
const secretsDir = path.join(profileDir, 'shuttle', 'secrets', a.id);
const saveError = await errorOf('save-text', { targetId: tab.targetId, selector: '#k', name: 'k1' });
if (/agent-browser/.test(saveError)) {
  skipCheck('save-text file deleted with the agent', 'no agent-browser sidecar');
} else {
  check('save-text writes the file', !saveError && fs.existsSync(path.join(secretsDir, 'k1.txt')), saveError);
}
await mcp('delete-agent', { id: a.id });
check('delete-agent removes it', !(await listed(a.id)));
check('its tab closes', await waitUntil(async () => !(await tabExists(tab.targetId)), 5000));
check('its secrets directory is gone', await waitUntil(() => !fs.existsSync(secretsDir), 5000), secretsDir);
check('its partition directory is gone', await waitUntil(() => !fs.existsSync(partitionDir), 5000), partitionDir);

// 5. Expiry.
const b = await throwaway();
const sb = await mcp('open-session', { agent: b.id, idleTimeoutMinutes: 0 });
const tb = await mcp('allocate-tab', { sessionId: sb.sessionId });
const c = await throwaway();
for (const until = Date.now() + IDLE_MS + 2 * CHECK_MS; Date.now() < until; await sleep(1000)) {
  await mcp('set-tab-viewport', { targetId: tb.targetId, width: 800, height: 600 });
}
check('activity keeps it', !!(await listed(b.id)));
check('one without sessions expires', !(await listed(c.id)));
check('idle, it expires', await waitUntil(async () => !(await listed(b.id)), IDLE_MS + 3 * CHECK_MS));
check('its tab closes on expiry', await waitUntil(async () => !(await tabExists(tb.targetId)), 5000));
check('its session is gone', !(await mcp('list-sessions')).sessions.some((x) => x.sessionId === sb.sessionId));

// 6. The cap (4 in this launch).
const live = [];
for (let i = 0; i < 4; i++) live.push(await throwaway());
check('the fifth is refused', /Too many throwaway agents \(max 4\)/.test(await errorOf('create-agent', { throwaway: true })));
for (const x of live) await mcp('delete-agent', { id: x.id });
check('after deleting, creating works again', !!(await throwaway()).id);

finish();
