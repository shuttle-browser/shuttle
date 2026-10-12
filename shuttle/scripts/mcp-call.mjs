// Calls one Shuttle MCP tool and prints its text result.
// Usage: node scripts/mcp-call.mjs <port> <tool> '<json args>'
const [port, name, args] = process.argv.slice(2);
const r = await fetch(`http://127.0.0.1:${port}/mcp`, { method: 'POST', headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ jsonrpc: '2.0', id: 1, method: 'tools/call', params: { name, arguments: JSON.parse(args || '{}') } }) });
const b = await r.json();
console.log(b.result?.content?.[0]?.text ?? JSON.stringify(b));
