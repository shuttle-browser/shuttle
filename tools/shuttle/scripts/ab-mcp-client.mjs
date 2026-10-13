// Minimal newline-delimited JSON-RPC client for `agent-browser mcp` (spike helper).
import { spawn } from 'node:child_process';
export function startAb({ bin, args = [], env = {} }) {
  const p = spawn(bin, args, { env: { ...process.env, ...env }, stdio: ['pipe', 'pipe', 'pipe'] });
  let buf = '', id = 0; const pending = new Map(); let stderr = '';
  p.stdout.on('data', d => { buf += d; let i; while ((i = buf.indexOf('\n')) >= 0) { const l = buf.slice(0, i); buf = buf.slice(i + 1); if (!l.trim()) continue; const m = JSON.parse(l); pending.get(m.id)?.(m); pending.delete(m.id); } });
  p.stderr.on('data', d => stderr += d);
  const req = (method, params) => new Promise(res => { const i = ++id; pending.set(i, res); p.stdin.write(JSON.stringify({ jsonrpc: '2.0', id: i, method, params }) + '\n'); });
  return { p, req, stderr: () => stderr,
    init: async () => { const r = await req('initialize', { protocolVersion: '2025-11-25', capabilities: {}, clientInfo: { name: 'ac', version: '0' } }); p.stdin.write(JSON.stringify({ jsonrpc: '2.0', method: 'notifications/initialized' }) + '\n'); return r; },
    listAll: async () => { let all = [], cursor; do { const r = await req('tools/list', cursor ? { cursor } : {}); all.push(...r.result.tools); cursor = r.result.nextCursor; } while (cursor); return all; },
    call: (name, args) => req('tools/call', { name, arguments: args }),
    kill: () => p.kill() };
}
