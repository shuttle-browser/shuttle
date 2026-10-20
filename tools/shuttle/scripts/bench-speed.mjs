// Speed benchmark: compare Shuttle builds (e.g. plain release vs ThinLTO+PGO) on cold start and Speedometer 3.1.
// Usage: node scripts/bench-speed.mjs [--rounds N] [--starts N] [--no-speedometer] <label>=<binary|App.app> ...
// Builds run interleaved (A B A B ...) so thermal drift and background load hit all of them alike. Each launch gets
// a fresh temp profile, --headless, its own debug port; one discarded warm-up launch per build first. Measures:
//   start: launch -> DevToolsActivePort written, and launch -> first local page loaded (median of --starts)
//   speedometer: browserbench.org Speedometer 3.1 score (needs internet; mean of --rounds)
// Prints a table and JSON. Close other heavy apps first; plug in the power adapter.
import { spawn } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync } from 'node:fs';
import http from 'node:http';
import { tmpdir } from 'node:os';
import { join, basename } from 'node:path';

const argv = process.argv.slice(2);
let rounds = 2, starts = 5, speedometer = true;
const builds = [];
for (let i = 0; i < argv.length; i++) {
  const a = argv[i];
  if (a === '--rounds') rounds = +argv[++i];
  else if (a === '--starts') starts = +argv[++i];
  else if (a === '--no-speedometer') speedometer = false;
  else if (a.includes('=')) {
    const [label, p] = [a.slice(0, a.indexOf('=')), a.slice(a.indexOf('=') + 1)];
    const bin = p.endsWith('.app') ? join(p, 'Contents/MacOS', basename(p, '.app')) : p;
    if (!existsSync(bin)) { console.error(`no binary: ${bin}`); process.exit(2); }
    builds.push({ label, bin, start: [], load: [], sp: [] });
  } else { console.error(`unknown arg ${a}`); process.exit(2); }
}
if (!builds.length) {
  console.error('usage: bench-speed.mjs [--rounds N] [--starts N] [--no-speedometer] <label>=<binary|App.app> ...');
  process.exit(2);
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const median = (a) => { const s = [...a].sort((x, y) => x - y); return s.length ? s[Math.floor(s.length / 2)] : NaN; };
const mean = (a) => a.reduce((x, y) => x + y, 0) / a.length;

const site = http.createServer((q, r) => { r.setHeader('Content-Type', 'text/html'); r.end('<title>t</title>ok'); });
await new Promise((r) => site.listen(0, '127.0.0.1', r));
const local = `http://127.0.0.1:${site.address().port}/`;

function cdp(url) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(url), pending = new Map(), waiters = [];
    let id = 0;
    ws.onopen = () => resolve({
      send: (method, params = {}, sessionId) => new Promise((res) => {
        const i = ++id; pending.set(i, res);
        ws.send(JSON.stringify({ id: i, method, params, ...(sessionId ? { sessionId } : {}) }));
      }),
      waitEvent: (method, sessionId) => new Promise((res) => waiters.push({ method, sessionId, res })),
      close: () => ws.close(),
    });
    ws.onerror = () => reject(new Error(`ws error ${url}`));
    ws.onmessage = (e) => {
      const m = JSON.parse(e.data);
      if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); return; }
      const w = waiters.findIndex((x) => x.method === m.method && (!x.sessionId || x.sessionId === m.sessionId));
      if (w >= 0) waiters.splice(w, 1)[0].res(m);
    };
  });
}

// Launch with a fresh profile; returns timings and a connected browser client.
async function launch(b) {
  const prof = mkdtempSync(join(tmpdir(), 'bench-speed-'));
  const t0 = performance.now();
  const proc = spawn(b.bin, ['--headless', `--user-data-dir=${prof}`, '--remote-debugging-port=0', '--no-first-run',
    '--no-default-browser-check', '--use-mock-keychain', 'about:blank'], { stdio: 'ignore' });
  const portFile = join(prof, 'DevToolsActivePort');
  while (!existsSync(portFile) || !readFileSync(portFile, 'utf8').includes('\n')) {
    if (performance.now() - t0 > 60000) throw new Error(`${b.label}: no DevToolsActivePort in 60 s`);
    await sleep(5);
  }
  const tPort = performance.now() - t0;
  const [port, path] = readFileSync(portFile, 'utf8').trim().split('\n');
  const c = await cdp(`ws://127.0.0.1:${port}${path}`);
  const { result: { targetId } } = await c.send('Target.createTarget', { url: 'about:blank' });
  const { result: { sessionId } } = await c.send('Target.attachToTarget', { targetId, flatten: true });
  await c.send('Page.enable', {}, sessionId);
  const loaded = c.waitEvent('Page.loadEventFired', sessionId);
  await c.send('Page.navigate', { url: local }, sessionId);
  await loaded;
  const tLoad = performance.now() - t0;
  const stop = async () => {
    c.close(); proc.kill('SIGTERM');
    await new Promise((r) => (proc.exitCode !== null ? r() : proc.once('exit', r)));
    // helpers may still write for a moment after the browser exits: retry, and never fail the run on cleanup
    try { rmSync(prof, { recursive: true, force: true, maxRetries: 10, retryDelay: 200 }); } catch {}
  };
  return { tPort, tLoad, c, sessionId, stop };
}

async function runSpeedometer(b) {
  const l = await launch(b);
  try {
    await l.c.send('Runtime.enable', {}, l.sessionId);
    await l.c.send('Page.navigate', { url: 'https://browserbench.org/Speedometer3.1/?startAutomatically=true' }, l.sessionId);
    const t0 = Date.now();
    for (;;) {
      await sleep(5000);
      const r = await l.c.send('Runtime.evaluate', { returnByValue: true, expression:
        `(() => { const s = document.querySelector('#summary'); const n = document.querySelector('#result-number');
           return s && s.offsetParent !== null && n && n.textContent.trim() ? n.textContent.trim() : ''; })()` }, l.sessionId);
      const v = parseFloat(r.result?.result?.value);
      if (v > 0) return v;
      if (Date.now() - t0 > 20 * 60000) throw new Error(`${b.label}: Speedometer did not finish in 20 min`);
    }
  } finally { await l.stop(); }
}

for (const b of builds) await (await launch(b)).stop();  // warm-up: first launch pays disk cache + signature checks
for (let s = 0; s < starts; s++) {
  for (const b of builds) {
    const l = await launch(b);
    b.start.push(l.tPort); b.load.push(l.tLoad);
    await l.stop(); await sleep(1500);
  }
}
console.log('start done');
if (speedometer) {
  for (let r = 0; r < rounds; r++) {
    for (const b of builds) {
      const v = await runSpeedometer(b);
      b.sp.push(v);
      console.log(`round ${r + 1} ${b.label}: Speedometer ${v}`);
      await sleep(10000);
    }
  }
}
site.close();

const base = builds[0];
const pct = (v, ref, lowerBetter) => (builds.length > 1 && ref ? `${(((lowerBetter ? ref / v : v / ref) - 1) * 100).toFixed(1)}%` : '');
console.log('\nbuild            start ms (port/page)   speedometer        vs first');
for (const b of builds) {
  const sp = b.sp.length ? mean(b.sp) : NaN;
  console.log(`${b.label.padEnd(16)} ${median(b.start).toFixed(0).padStart(6)} / ${median(b.load).toFixed(0).padStart(6)}` +
    `        ${Number.isNaN(sp) ? '-' : sp.toFixed(2).padStart(7)}        ` +
    `start ${pct(median(b.load), median(base.load), true)}  speedometer ${Number.isNaN(sp) ? '-' : pct(sp, mean(base.sp))}`);
}
console.log(JSON.stringify(builds.map(({ label, start, load, sp }) => ({ label, start, load, sp }))));
