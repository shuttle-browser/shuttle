// RAM benchmark: N x (chrome-devtools-mcp + its own Chrome) vs one Shuttle with N agents.
// Usage: node bench.mjs cdm <n> <url>
//        node bench.mjs shuttle <n> <url> <shuttleBinary> <port>
// Prints JSON: per-process footprint (phys_footprint, like Activity Monitor "Memory") at two points:
// "idle" (connected, nothing open) and "page" (each agent opened <url> and took a snapshot), after a settle delay.
import { spawn, execFileSync } from 'node:child_process';
import { readFileSync, mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const [mode, nStr, url, bin, portStr] = process.argv.slice(2);
const N = Number(nStr);
const SETTLE = 15000;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

function tree(rootPids) {
  const rows = execFileSync('ps', ['-A', '-o', 'pid=,ppid=,comm=']).toString().trim().split('\n')
    .map((l) => l.trim().split(/\s+/)).map(([pid, ppid, ...c]) => ({ pid: +pid, ppid: +ppid, comm: c.join(' ') }));
  const out = new Set(rootPids);
  let grew = true;
  while (grew) { grew = false; for (const r of rows) if (out.has(r.ppid) && !out.has(r.pid)) { out.add(r.pid); grew = true; } }
  return rows.filter((r) => out.has(r.pid));
}

function measure(rootPids, label) {
  const procs = tree(rootPids);
  const f = join(tmpdir(), `fp-${process.pid}.json`);
  execFileSync('footprint', ['-j', f, '--noCategories', ...procs.flatMap((p) => ['-p', String(p.pid)])], { stdio: 'ignore' });
  const d = JSON.parse(readFileSync(f, 'utf8'));
  const list = d.processes.map((p) => ({ pid: p.pid, name: p.name, mb: +(p.footprint / 1048576).toFixed(1) }));
  const total = +(list.reduce((a, p) => a + p.mb, 0)).toFixed(1);
  return { label, processes: list.length, totalMB: total, list };
}

// ---- chrome-devtools-mcp over stdio
class Stdio {
  constructor(child) {
    this.c = child; this.id = 0; this.wait = new Map(); let buf = '';
    child.stdout.on('data', (d) => {
      buf += d; let i;
      while ((i = buf.indexOf('\n')) >= 0) {
        const line = buf.slice(0, i); buf = buf.slice(i + 1);
        try { const m = JSON.parse(line); if (m.id && this.wait.has(m.id)) { this.wait.get(m.id)(m); this.wait.delete(m.id); } } catch {}
      }
    });
  }
  req(method, params) {
    const id = ++this.id;
    this.c.stdin.write(JSON.stringify({ jsonrpc: '2.0', id, method, params }) + '\n');
    return new Promise((r, j) => { this.wait.set(id, r); setTimeout(() => j(new Error(`timeout ${method}`)), 120000); });
  }
  note(method, params) { this.c.stdin.write(JSON.stringify({ jsonrpc: '2.0', method, params }) + '\n'); }
}

async function cdm() {
  const kids = [];
  for (let i = 0; i < N; i++) {
    const c = spawn('npx', ['-y', 'chrome-devtools-mcp@1.10.1', '--isolated', '--headless', '--no-usage-statistics'],
      { stdio: ['pipe', 'pipe', 'ignore'], cwd: tmpdir() });
    const s = new Stdio(c);
    await s.req('initialize', { protocolVersion: '2025-06-18', capabilities: {}, clientInfo: { name: 'bench', version: '1' } });
    s.note('notifications/initialized', {});
    kids.push({ c, s });
  }
  const tools = (await kids[0].s.req('tools/list', {})).result.tools.map((t) => t.name);
  // First tool call launches Chrome. list_pages = "connected, nothing open yet".
  await Promise.all(kids.map(({ s }) => s.req('tools/call', { name: 'list_pages', arguments: {} })));
  await sleep(SETTLE);
  const idle = measure(kids.map((k) => k.c.pid), 'idle');
  await Promise.all(kids.map(async ({ s }) => {
    const r = await s.req('tools/call', { name: 'new_page', arguments: { url } });
    if (r.error || r.result?.isError) console.error('navigate', JSON.stringify(r).slice(0, 300));
    await s.req('tools/call', { name: 'take_snapshot', arguments: {} });
  }));
  await sleep(SETTLE);
  const page = measure(kids.map((k) => k.c.pid), 'page');
  for (const { c } of kids) c.kill('SIGTERM');
  await sleep(3000);
  for (const p of tree(kids.map((k) => k.c.pid))) try { process.kill(p.pid, 'SIGKILL'); } catch {}
  return { mode: 'cdm', n: N, url, toolsSample: tools.slice(0, 5), idle, page };
}

// ---- Shuttle: one browser, its built-in MCP, N agents
async function shuttle() {
  const port = Number(portStr);
  const udd = mkdtempSync(join(tmpdir(), 'ram-bench-shuttle-'));
  const b = spawn(bin, [`--user-data-dir=${udd}`, `--remote-debugging-port=${port}`, '--headless', '--no-first-run'],
    { stdio: 'ignore' });
  const mcp = async (name, args) => {
    const r = await fetch(`http://127.0.0.1:${port}/mcp`, { method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ jsonrpc: '2.0', id: 1, method: 'tools/call', params: { name, arguments: args } }) });
    const j = await r.json(); const t = j.result?.content?.[0]?.text;
    if (j.error || j.result?.isError) throw new Error(`${name}: ${JSON.stringify(j).slice(0, 300)}`);
    try { return JSON.parse(t); } catch { return t; }
  };
  for (let i = 0; i < 60; i++) { try { await fetch(`http://127.0.0.1:${port}/json/version`); break; } catch { await sleep(500); } }
  const sessions = [];
  for (let i = 0; i < N; i++) {
    await mcp('create-agent', { id: `bench-${i}` });
    sessions.push(await mcp('open-session', { agent: `bench-${i}` }));
  }
  await sleep(SETTLE);
  const idle = measure([b.pid], 'idle');
  await Promise.all(sessions.map(async (s) => {
    const t = await mcp('allocate-tab', { sessionId: s.sessionId, url });
    await sleep(3000); // allocate-tab returns at commit; give the page time to load like navigate_page does
    await mcp('snapshot', { targetId: t.targetId });
  }));
  await sleep(SETTLE);
  const page = measure([b.pid], 'page');
  const ws = new WebSocket((await (await fetch(`http://127.0.0.1:${port}/json/version`)).json()).webSocketDebuggerUrl);
  await new Promise((r) => (ws.onopen = r));
  ws.send(JSON.stringify({ id: 1, method: 'Browser.close' }));
  await sleep(4000);
  for (const p of tree([b.pid])) try { process.kill(p.pid, 'SIGKILL'); } catch {}
  rmSync(udd, { recursive: true, force: true });
  return { mode: 'shuttle', n: N, url, idle, page };
}

const res = mode === 'cdm' ? await cdm() : await shuttle();
// mcp = the MCP server side (Node for chrome-devtools-mcp, the agent-browser page-tools sidecar for Shuttle); browser = everything else.
const isMcp = (n) => /^(node|npm|agent-browser)/.test(n);
const sum = (l) => +(l.reduce((a, p) => a + p.mb, 0)).toFixed(1);
const brief = (m) => ({ processes: m.processes, totalMB: m.totalMB,
  mcpMB: sum(m.list.filter((p) => isMcp(p.name))), mcpProcs: m.list.filter((p) => isMcp(p.name)).length,
  browserMB: sum(m.list.filter((p) => !isMcp(p.name))), browserProcs: m.list.filter((p) => !isMcp(p.name)).length,
  top: m.list.sort((a, b) => b.mb - a.mb).slice(0, 12) });
console.log(JSON.stringify({ ...res, idle: brief(res.idle), page: brief(res.page) }, null, 1));
process.exit(0);
