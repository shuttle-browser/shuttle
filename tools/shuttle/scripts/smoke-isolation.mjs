// Agent storage isolation across blank tabs and about:blank: node scripts/smoke-isolation.mjs <port>
// (run by scripts/smoke-all.sh). Three agents on one local site (cookie + localStorage):
//   A: blank tab (allocate-tab without url), navigates, writes its storage.
//   B: blank tab, navigates to the same site: must see nothing of A's.
//   C: url tab, writes, hops through about:blank and back: must keep its own storage.
//   A again: still only its own.
// Regression probe for blank/about:blank agent tabs landing in the default storage partition (fork commit 50).
import http from 'node:http';
import { cdp, check, cleanup, finish, gone, mcp, sleep } from './lib/smoke.mjs';

const site = http.createServer((req, res) => { res.setHeader('Content-Type', 'text/html'); res.end('<title>iso</title>'); });
await new Promise((r) => site.listen(0, '127.0.0.1', r));
cleanup(() => site.close());
const siteUrl = `http://127.0.0.1:${site.address().port}/`;

const agents = ['smoke-iso-a', 'smoke-iso-b', 'smoke-iso-c'];
cleanup(async () => { for (const id of agents) await mcp('delete-agent', { id }).catch(gone); });
async function evalIn(cdpUrl, expression) {
  const c = await cdp(cdpUrl);
  try { return (await c.send('Runtime.evaluate', { expression, returnByValue: true })).result?.result?.value; }
  finally { c.close(); }
}
async function tab(agent, url) {
  await mcp('create-agent', { id: agent, persistSessionCookies: false });
  const s = await mcp('open-session', { agent });
  const t = await mcp('allocate-tab', url ? { sessionId: s.sessionId, url } : { sessionId: s.sessionId });
  if (url) await sleep(800);
  return t;
}
const nav = async (t, url) => { await mcp('navigate-tab', { targetId: t.targetId, url }); await sleep(800); };
const storage = (t) => evalIn(t.cdpUrl, 'document.cookie + " | ls=" + (localStorage.getItem("k") ?? "")');
const write = (t, v) => evalIn(t.cdpUrl, `document.cookie = "c=${v}; max-age=3600"; localStorage.setItem("k", "${v}")`);
const expect = async (label, t, want) => { const got = await storage(t); check(label, got === want, JSON.stringify(got)); };

const a = await tab(agents[0]); await nav(a, siteUrl); await write(a, 'A');
await expect('A (blank start) sees its own storage', a, 'c=A | ls=A');
const b = await tab(agents[1]); await nav(b, siteUrl);
await expect('B (blank start) sees nothing of A', b, ' | ls=');
const c = await tab(agents[2], siteUrl); await write(c, 'C');
await nav(c, 'about:blank'); await nav(c, siteUrl);
await expect('C keeps its own storage after an about:blank hop', c, 'c=C | ls=C');
await expect('A unchanged', a, 'c=A | ls=A');
await finish();
