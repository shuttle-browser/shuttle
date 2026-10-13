// Prints agents (name, color) + each session's tabs whenever they change (1 s poll).
// Usage: node scripts/watch-agents.mjs [port]  (default 9333; exits when the browser is gone)
const call = async (name) => (await (await fetch(`http://127.0.0.1:${process.argv[2] || 9333}/mcp`, { method: 'POST', headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ jsonrpc: '2.0', id: 1, method: 'tools/call', params: { name, arguments: {} } }) })).json()).result.structuredContent;
let last = '';
for (;;) {
  let cur;
  try {
    const a = (await call('list-agents')).agents.map((x) => `${x.id}="${x.name}"(${x.color})`).join(' ');
    const s = (await call('list-sessions')).sessions.map((x) => `${x.agent}/${x.sessionId.slice(0, 4)}:[${x.tabs.map((t) => t.url.replace(/^https?:\/\//, '').slice(0, 30)).join(', ')}]`).join(' ');
    cur = `${a} || ${s}`;
  } catch { cur = 'browser gone'; }
  if (cur !== last) { console.log(cur); last = cur; }
  if (cur === 'browser gone') process.exit(0);
  await new Promise((r) => setTimeout(r, 1000));
}
