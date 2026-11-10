// Linux headless screenshot hang (docs/LINUX.md "Known issues"): repro + CDP capture of agent-browser's traffic.
// Usage: node scripts/repro-screenshot-hang.mjs <port> <mode> [options]
//   raw                 8x Page.captureScreenshot on a fresh session tab over its session endpoint (baseline)
//   sidecar             run agent-browser (`mcp`) ourselves against the session endpoint through a logging WebSocket
//                       proxy: tab_switch, open, 4 sidecar screenshots, then 8 raw captures on our own connection;
//                       the proxied CDP traffic goes to --log (JSONL)
//   plain <file.json>   as replay, on an ordinary tab (Target.createTarget about:blank on the browser-wide endpoint;
//                       no agent, no lease; --fg foreground, --window new window, --url start
//                       on the test page): tells upstream behavior from Shuttle's
//   replay <file.json>  raw connection: send the CDP commands in <file.json> ([{method, params, page: bool}], page =
//                       on the tab's session) first, then 8 raw captures
// Options: --agent-browser <bin> (default: next to the browser binary, $SHUTTLE_AGENT_BROWSER), --log <file>,
//          --blank (raw/replay: allocate the tab without a URL; replay can navigate it), --viewport <W>x<H> (set-tab-viewport first, as smoke-page-tools does),
//          --keep-sidecar (sidecar mode: keep agent-browser running during the raw captures; default: close it first)
// The browser must already run with --remote-debugging-port=<port> (e.g. --headless). Needs Node 22+ (WebSocket,
// fetch); the logging proxy uses the `ws` package from third_party/devtools-frontend.
import { spawn } from 'node:child_process';
import { mkdtempSync, readFileSync, writeFileSync, appendFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const [port, mode, ...rest] = process.argv.slice(2);
const opt = (name) => { const i = rest.indexOf(name); return i >= 0 ? rest[i + 1] : undefined; };
if (!port || !['raw', 'sidecar', 'replay', 'plain'].includes(mode)) {
  console.error('usage: repro-screenshot-hang.mjs <port> raw|sidecar|replay <file.json> [--agent-browser <bin>] [--log <file>] [--viewport WxH] [--keep-sidecar]');
  process.exit(2);
}

let mcpId = 0;
async function tool(name, args) {
  const r = await fetch(`http://127.0.0.1:${port}/mcp`, { method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ jsonrpc: '2.0', id: ++mcpId, method: 'tools/call', params: { name, arguments: args } }) });
  const res = (await r.json()).result;
  if (res?.isError) throw new Error(`${name}: ${res.content?.[0]?.text}`);
  return JSON.parse(res.content[0].text);
}

// Minimal CDP client over one WebSocket (flat sessions).
async function cdp(url) {
  const ws = new WebSocket(url);
  await new Promise((ok, fail) => { ws.onopen = ok; ws.onerror = fail; });
  const pending = new Map(); let id = 0;
  ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && pending.has(d.id)) { pending.get(d.id)(d); pending.delete(d.id); } };
  const send = (method, params = {}, sessionId, ms = 10_000) => new Promise((ok) => {
    const i = ++id; pending.set(i, ok); ws.send(JSON.stringify({ id: i, method, params, sessionId }));
    setTimeout(() => { if (pending.has(i)) { pending.delete(i); ok({ error: { message: 'TIMEOUT' } }); } }, ms);
  });
  return { send, close: () => ws.close() };
}

async function rawCaptures(c, sessionId, n = 8) {
  const out = [];
  for (let i = 0; i < n; i++) {
    const t0 = Date.now(); const r = await c.send('Page.captureScreenshot', { format: 'png' }, sessionId);
    out.push(r.error ? (r.error.message === 'TIMEOUT' ? 'HANG' : `ERR(${r.error.message})`) : `${Date.now() - t0}`);
  }
  return out.join(' ');
}

const page = 'data:text/html,' + encodeURIComponent('<p>' + 'line '.repeat(400) + '</p>');
if (mode === 'plain') {
  const { webSocketDebuggerUrl } = await (await fetch(`http://127.0.0.1:${port}/json/version`)).json();
  const c = await cdp(webSocketDebuggerUrl);
  const { result: { targetId } } = await c.send('Target.createTarget', { url: rest.includes('--url') ? page : 'about:blank', background: !rest.includes('--fg'), newWindow: rest.includes('--window') });
  const { result: { sessionId } } = await c.send('Target.attachToTarget', { targetId, flatten: true });
  for (const s of JSON.parse(readFileSync(rest[0], 'utf8'))) {
    const r = await c.send(s.method, s.params ?? {}, s.page ? sessionId : undefined);
    console.log(`  ${s.method}${s.page ? ' (page)' : ''}: ${r.error ? 'error ' + r.error.message : 'ok'}`);
  }
  await new Promise((ok) => setTimeout(ok, 1000));
  console.log(`plain: ${await rawCaptures(c, sessionId)}`);
  await c.send('Target.closeTarget', { targetId }); c.close();
  process.exit(0);
}
const agent = await tool('create-agent', { name: `repro ${mode}`, throwaway: true });
const sess = await tool('open-session', { agent: agent.id });
const { targetId } = await tool('allocate-tab', { sessionId: sess.sessionId, url: mode === 'sidecar' || rest.includes('--blank') ? undefined : page });
if (opt('--viewport')) {
  const [width, height] = opt('--viewport').split('x').map(Number);
  await tool('set-tab-viewport', { targetId, width, height });
}

try {
  if (mode === 'raw' || mode === 'replay') {
    const c = await cdp(sess.cdpEndpoint);
    const { result: { sessionId } } = await c.send('Target.attachToTarget', { targetId, flatten: true });
    if (mode === 'replay') {
      for (const s of JSON.parse(readFileSync(rest[0], 'utf8'))) {
        const r = await c.send(s.method, s.params ?? {}, s.page ? sessionId : undefined);
        console.log(`  ${s.method}${s.page ? ' (page)' : ''}: ${r.error ? 'error ' + r.error.message : 'ok'}`);
      }
      await new Promise((ok) => setTimeout(ok, 1000));  // let a navigation commit
    }
    console.log(`${mode}: ${await rawCaptures(c, sessionId)}`);
    c.close();
  } else {
    const log = opt('--log') ?? join(tmpdir(), `repro-cdp-${Date.now()}.jsonl`);
    writeFileSync(log, '');
    const t0 = Date.now();
    const rec = (dir, d) => {
      const e = { t: Date.now() - t0, dir, id: d.id, method: d.method, sessionId: d.sessionId };
      if (d.params) e.params = JSON.stringify(d.params).slice(0, 400);
      if (d.error) e.error = d.error.message;
      if (d.result) e.result = JSON.stringify(d.result).slice(0, 200);
      appendFileSync(log, JSON.stringify(e) + '\n');
    };
    // Logging proxy: every client connection gets its own upstream connection to the session endpoint.
    const req = createRequire(join(here, '../../../third_party/devtools-frontend/src/package.json'));
    const { WebSocketServer } = req('ws');
    const wss = new WebSocketServer({ port: 0, host: '127.0.0.1' });
    await new Promise((ok) => wss.on('listening', ok));
    wss.on('connection', (client) => {
      const up = new WebSocket(sess.cdpEndpoint); const queue = [];
      up.onopen = () => { for (const m of queue) up.send(m); queue.length = 0; };
      up.onmessage = (m) => { rec('<', JSON.parse(m.data)); client.send(m.data); };
      up.onclose = () => client.close();
      client.on('message', (m) => { const s = m.toString(); rec('>', JSON.parse(s)); up.readyState === 1 ? up.send(s) : queue.push(s); });
      client.on('close', () => up.close());
    });
    const proxied = `ws://127.0.0.1:${wss.address().port}/devtools/session/${sess.sessionId}`;

    const bin = opt('--agent-browser') ?? process.env.SHUTTLE_AGENT_BROWSER ?? '/opt/shuttle/agent-browser';
    const work = mkdtempSync(join(tmpdir(), 'repro-ab-'));
    const ab = spawn(bin, ['mcp', '--tools', 'all'], { stdio: ['pipe', 'pipe', 'inherit'], env: { ...process.env,
      AGENT_BROWSER_CDP: proxied, AGENT_BROWSER_SESSION: `repro${process.pid}`, AGENT_BROWSER_SOCKET_DIR: work,
      AGENT_BROWSER_SCREENSHOT_DIR: work } });
    let buf = ''; const waiters = new Map(); let abId = 0;
    ab.stdout.on('data', (d) => { buf += d; let i;
      while ((i = buf.indexOf('\n')) >= 0) { const line = buf.slice(0, i); buf = buf.slice(i + 1);
        try { const m = JSON.parse(line); if (m.id && waiters.has(m.id)) { waiters.get(m.id)(m); waiters.delete(m.id); } } catch {} } });
    const abCall = (method, params, ms = 60_000) => new Promise((ok) => { const i = ++abId; waiters.set(i, ok);
      ab.stdin.write(JSON.stringify({ jsonrpc: '2.0', id: i, method, params }) + '\n');
      setTimeout(() => { if (waiters.has(i)) { waiters.delete(i); ok({ error: { message: 'TIMEOUT' } }); } }, ms); });
    const abTool = async (name, args) => { const t = Date.now(); const r = await abCall('tools/call', { name, arguments: args });
      const err = r.error?.message ?? (r.result?.isError ? r.result.content?.[0]?.text?.slice(0, 100) : '');
      appendFileSync(log, JSON.stringify({ t: Date.now() - t0, mark: name, ms: Date.now() - t, err }) + '\n');
      return err ? `ERR(${Date.now() - t}: ${err})` : `${Date.now() - t}`; };

    await abCall('initialize', { protocolVersion: '2025-06-18', capabilities: {}, clientInfo: { name: 'repro', version: '0' } });
    ab.stdin.write(JSON.stringify({ jsonrpc: '2.0', method: 'notifications/initialized' }) + '\n');
    console.log(`tab_switch ${await abTool('agent_browser_tab_switch', { tab: targetId })}`);
    console.log(`open ${await abTool('agent_browser_open', { url: page })}`);
    const shots = [];
    for (let i = 0; i < 4; i++) shots.push(await abTool('agent_browser_screenshot', {}));
    console.log(`sidecar screenshots: ${shots.join(' ')}`);
    if (!rest.includes('--keep-sidecar')) {
      await abTool('agent_browser_close', {});
      ab.kill(); await new Promise((ok) => setTimeout(ok, 500));
    }
    const c = await cdp(sess.cdpEndpoint);
    const { result: { sessionId } } = await c.send('Target.attachToTarget', { targetId, flatten: true });
    console.log(`raw after sidecar${rest.includes('--keep-sidecar') ? ' (still attached)' : ' (closed)'}: ${await rawCaptures(c, sessionId)}`);
    c.close(); ab.kill(); wss.close();
    console.log(`log: ${log}`);
  }
} finally {
  await tool('close-session', { sessionId: sess.sessionId }).catch(() => {});
}
process.exit(0);
