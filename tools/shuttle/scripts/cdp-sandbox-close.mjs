// Opens chrome://sandbox/ over CDP, prints its status line, then Browser.close. Usage: node cdp-sandbox-close.mjs <port>
const port = process.argv[2] || 9333;
const v = await (await fetch(`http://127.0.0.1:${port}/json/version`)).json();
const ws = new WebSocket(v.webSocketDebuggerUrl);
await new Promise(r => ws.addEventListener('open', r, { once: true }));
let id = 0; const pend = new Map();
ws.addEventListener('message', e => { const m = JSON.parse(e.data); if (m.id && pend.has(m.id)) { pend.get(m.id)(m); pend.delete(m.id); } });
const send = (method, params = {}, sessionId) => new Promise(r => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params, ...(sessionId ? { sessionId } : {}) })); });
const t = await send('Target.createTarget', { url: 'chrome://sandbox/', background: true });
const a = await send('Target.attachToTarget', { targetId: t.result.targetId, flatten: true });
await new Promise(r => setTimeout(r, 2500));
const ev = await send('Runtime.evaluate', { expression: 'location.href + " | " + document.title + " | " + document.body.innerText.slice(0, 400).replace(/\\s+/g, " ")', returnByValue: true }, a.result.sessionId);
console.log('sandbox:', ev.result?.result?.value);
await send('Browser.close');
console.log('closed');
process.exit(0);
