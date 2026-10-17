// Shared by the smoke probes (scripts/smoke-*.mjs, check-automation.mjs; run by scripts/smoke-all.sh).
// Probe contract: `node <probe>.mjs <port> [args]`; exit 0 pass, 1 fail, 2 usage, 3 skipped. Probes register their
// agents/sessions with cleanup(); cleanups and open CDP sockets are closed on finish, skip, throw and deadline.
// Deadlines: SHUTTLE_SMOKE_DEADLINE_MS for the whole probe (default 120 s), CALL_MS per CDP/MCP call.
export const port = process.argv[2];
export const args = process.argv.slice(3);
if (!/^\d+$/.test(port ?? '')) { console.error(`usage: node ${process.argv[1]} <port> [args]`); process.exit(2); }
export const base = `http://127.0.0.1:${port}`;
export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const CALL_MS = 20_000;
let failed = 0, skipped = 0, ending = false;
const cleanups = [], sockets = new Set();

export function check(name, ok, detail = '') {
  if (!ok) failed++;
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${detail ? ` - ${detail}` : ''}`);
  return !!ok;
}
export const skipCheck = (name, reason) => { skipped++; console.log(`SKIP ${name} - ${reason}`); };
export const cleanup = (fn) => cleanups.push(fn);
async function end(code, msg) {
  if (ending) process.exit(code || 1);  // a throw/deadline during cleanup: don't recurse
  ending = true;
  if (msg) console.log(msg);
  const until = Date.now() + 10_000;  // total cleanup budget
  for (const fn of cleanups.reverse()) {
    try { await Promise.race([fn(), sleep(Math.max(0, until - Date.now())).then(() => { throw new Error('timed out'); })]); }
    catch (e) { console.log(`cleanup: ${e.message}`); }
  }
  for (const ws of sockets) ws.close();
  process.exit(code);
}
export const finish = () => end(failed ? 1 : skipped ? 3 : 0, failed ? `${failed} FAILED` : skipped ? 'SKIPPED (partly)' : 'ALL OK');
export const skip = (reason) => end(3, `SKIP ${reason}`);
export const fail = (msg) => end(1, `FAIL ${msg}`);
process.on('uncaughtException', (e) => fail(e?.stack || e));
process.on('unhandledRejection', (e) => fail(e?.stack || e));
const ms = Number(process.env.SHUTTLE_SMOKE_DEADLINE_MS || 120_000);
setTimeout(() => fail(`deadline: probe ran longer than ${ms} ms`), ms).unref();

// CDP client: send(method, params, sessionId, ms = CALL_MS) resolves with the whole reply ({result} or {error}); other messages land in events.
export function cdp(url) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(url);
    const pending = new Map(), events = [];
    let id = 0;
    const t = setTimeout(() => { ws.close(); reject(new Error(`connect timeout ${url}`)); }, CALL_MS);
    const client = { events, ws, close: () => ws.close(),
      send: (method, params = {}, sessionId, ms = CALL_MS) => new Promise((res, rej) => {
        if (ws.readyState !== WebSocket.OPEN) return rej(new Error(`${method}: socket closed`));
        const i = ++id;
        const timer = setTimeout(() => { pending.delete(i); rej(new Error(`${method}: no reply in ${ms} ms`)); }, ms);
        pending.set(i, { res: (m) => { clearTimeout(timer); res(m); }, rej: (e) => { clearTimeout(timer); rej(e); } });
        ws.send(JSON.stringify({ id: i, method, params, ...(sessionId ? { sessionId } : {}) }));
      }) };
    ws.onopen = () => { clearTimeout(t); sockets.add(ws); resolve(client); };
    ws.onerror = () => { clearTimeout(t); reject(new Error(`ws error ${url}`)); };
    ws.onclose = () => { sockets.delete(ws); for (const p of pending.values()) p.rej(new Error('socket closed')); pending.clear(); };
    ws.onmessage = (e) => {
      const m = JSON.parse(e.data), p = m.id && pending.get(m.id);
      if (p) { pending.delete(m.id); p.res(m); } else events.push(m);
    };
  });
}
export const json = async (path) => {
  const r = await fetch(base + path, { signal: AbortSignal.timeout(CALL_MS) });
  if (!r.ok) throw new Error(`GET ${path}: HTTP ${r.status}`);
  return r.json();
};
export const browserWs = async () => (await json('/json/version')).webSocketDebuggerUrl;
export const evaluate = async (client, sessionId, expression) =>
  (await client.send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true }, sessionId)).result?.result?.value;

// MCP: rpc() returns {status, body}; mcp() calls a tool and throws on HTTP, JSON-RPC and tool errors.
let rpcId = 0;
export async function rpc(method, params, headers = {}, url = base) {
  const r = await fetch(`${url}/mcp`, { method: 'POST', signal: AbortSignal.timeout(CALL_MS),
    headers: { 'Content-Type': 'application/json', Accept: 'application/json, text/event-stream', ...headers },
    body: JSON.stringify({ jsonrpc: '2.0', id: ++rpcId, method, ...(params ? { params } : {}) }) });
  const text = await r.text();
  try { return { status: r.status, body: text ? JSON.parse(text) : null }; } catch { return { status: r.status, body: text }; }
}
export async function mcp(name, args = {}) {
  const { status, body } = await rpc('tools/call', { name, arguments: args });
  if (status !== 200 || !body?.result) throw new Error(`${name}: HTTP ${status} ${JSON.stringify(body?.error ?? body).slice(0, 200)}`);
  if (body.result.isError) throw new Error(`${name}: ${body.result.content?.[0]?.text}`);
  return body.result.structuredContent ?? body.result;
}
