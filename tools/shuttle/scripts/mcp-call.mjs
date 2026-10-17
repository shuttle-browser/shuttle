// Calls one Shuttle MCP tool and prints its text result. Exit 1 on HTTP/JSON-RPC/tool errors, 2 on usage errors.
// Usage: node scripts/mcp-call.mjs <port> <tool> '<json args>'
const [port, name, args] = process.argv.slice(2);
if (!port || !name) { console.error("usage: mcp-call.mjs <port> <tool> '<json args>'"); process.exit(2); }
let params;
try { params = JSON.parse(args || '{}'); } catch (e) { console.error(`bad JSON args: ${e.message}`); process.exit(2); }
const r = await fetch(`http://127.0.0.1:${port}/mcp`, { method: 'POST', headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ jsonrpc: '2.0', id: 1, method: 'tools/call', params: { name, arguments: params } }),
  signal: AbortSignal.timeout(60_000) });
const text = await r.text();
let b;
try { b = JSON.parse(text); } catch { console.error(`HTTP ${r.status}: ${text.slice(0, 300)}`); process.exit(1); }
const out = b.result?.content?.[0]?.text;
if (!r.ok || b.error || b.result?.isError || out === undefined) { console.error(out ?? JSON.stringify(b)); process.exit(1); }
console.log(out);
