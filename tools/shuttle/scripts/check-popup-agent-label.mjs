// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Usage: DISPLAY=<dedicated X11 display> SHUTTLE_POPUP_TEST_DISPLAY=$DISPLAY
// node check-popup-agent-label.mjs <binary> <baseline|patched> <evidence-dir>
// Owns its browser/profile/port; never connects to the installed browser.
// Linux/X11 only; requires Python 3 and libX11. Uses neutral loopback pages.
import {execFileSync, spawn} from 'node:child_process';
import {createHash} from 'node:crypto';
import {
  mkdirSync,
  openSync,
  readFileSync,
  rmSync,
  writeFileSync
} from 'node:fs';
import http from 'node:http';
import net from 'node:net';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const [binary, mode, dir] = process.argv.slice(2);
if (!binary || !['baseline', 'patched'].includes(mode) || !dir)
  throw Error('binary, mode, evidence directory required');
if (!process.env.DISPLAY ||
    process.env.DISPLAY !== process.env.SHUTTLE_POPUP_TEST_DISPLAY)
  throw Error('Dedicated test display required');
mkdirSync(dir, {recursive : true});
const extension =
    fileURLToPath(new URL('../test-data/popup-extension', import.meta.url));
const manifest =
    JSON.parse(readFileSync(path.join(extension, 'manifest.json')));
const extensionId = [...createHash('sha256')
                         .update(Buffer.from(manifest.key, 'base64'))
                         .digest('hex')
                         .slice(0, 32) ]
                        .map(n => String.fromCharCode(97 + parseInt(n, 16)))
                        .join('');
const helper = fileURLToPath(new URL('./lib/popup-window.py', import.meta.url));
let port;
const reservation = net.createServer();
await new Promise((r, j) => {
  reservation.once('error', j);
  reservation.listen(0, '127.0.0.1', r);
});
port = reservation.address().port;
await new Promise(r => reservation.close(r));
const checks = [], requests = [], sockets = [];
let browser, control, lane;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const deadline = setTimeout(() => {
  browser?.kill('SIGTERM');
  process.exit(1);
}, 180000);
deadline.unref();
function check(name, ok, detail) {
  checks.push({name, ok, detail});
  console.log(`${ok ? 'ok' : 'FAIL'} ${name}: ${JSON.stringify(detail ?? '')}`);
}
async function until(fn, label) {
  for (let i = 0; i < 100; i++) {
    try {
      const v = await fn();
      if (v)
        return v;
    } catch {
    }
    await sleep(100);
  }
  throw Error(`timeout: ${label}`);
}
async function connect(url) {
  const ws = new WebSocket(url), pending = new Map();
  sockets.push(ws);
  let id = 0;
  ws.onmessage = e => {
    const m = JSON.parse(e.data), p = pending.get(m.id);
    if (p) {
      pending.delete(m.id);
      clearTimeout(p.t);
      m.error ? p.reject(Error(JSON.stringify(m.error))) : p.resolve(m.result);
    }
  };
  await new Promise((r, j) => {
    ws.onopen = r;
    ws.onerror = j;
  });
  return {
    send(method, params = {}, sessionId) {
      return new Promise((resolve, reject) => {
        const i = ++id, t = setTimeout(() => {
                          pending.delete(i);
                          reject(Error(`timeout: ${method}`));
                        }, 15000);
        pending.set(i, {resolve, reject, t});
        ws.send(JSON.stringify(
            {id : i, method, params, ...(sessionId ? {sessionId} : {})}));
      });
    },
    close() { ws.close(); }
  };
}
async function tool(name, args = {}) {
  const b = await (await fetch(`http://127.0.0.1:${port}/mcp`, {
              method : 'POST',
              headers : {'content-type' : 'application/json'},
              body : JSON.stringify({
                jsonrpc : '2.0',
                id : 1,
                method : 'tools/call',
                params : {name, arguments : args}
              })
            })).json();
  if (b.error || b.result?.isError)
    throw Error(JSON.stringify(b));
  return b.result.structuredContent;
}
async function attach(client, targetId) {
  return (await client.send('Target.attachToTarget',
                            {targetId, flatten : true}))
      .sessionId;
}
async function evaluate(client, sid, expression) {
  const r = await client.send('Runtime.evaluate', {
    expression,
    returnByValue : true,
    awaitPromise : true,
    userGesture : true
  },
                              sid);
  if (r.exceptionDetails)
    throw Error(JSON.stringify(r.exceptionDetails));
  return r.result?.value;
}
function native(title, shot) {
  return JSON.parse(execFileSync(
      'python3', [ helper, title, ...(shot ? [ path.join(dir, shot) ] : []) ], {
        encoding : 'utf8',
        timeout : 3000,
        stdio : [ 'ignore', 'pipe', 'pipe' ]
      }));
}
const site = http.createServer((req, res) => {
  requests.push({
    url : req.url,
    host : req.headers.host,
    cookie : req.headers.cookie || ''
  });
  res.setHeader('content-type', 'text/html');
  if (req.url.startsWith('/oauth')) {
    res.writeHead(
        302,
        {Location : `http://localhost:${site.address().port}/Popup-redirect`});
    res.end();
    return;
  }
  const name = req.url.slice(1) || 'Opener';
  res.end(`<title>${
      name}</title><style>body{font:18px sans-serif;margin:40px;background:#fff;color:#222}button{font:inherit;padding:10px}</style><h1>Neutral popup test</h1><p>This local page uses no account or credentials.</p><button id="popup" onclick="window.open('/Popup-main','main','popup,width=700,height=480')">Open test popup</button>`);
});
await new Promise(r => site.listen(0, '127.0.0.1', r));
const url = p => `http://127.0.0.1:${site.address().port}/${p}`;
const agent = 'agent-popup-label-probe';
const colors = s => s.chromeColors.filter(
    c => Math.max(...c.rgb) - Math.min(...c.rgb) > 60 && c.pixels > 300);
try {
  const log = openSync(path.join(dir, 'browser.log'), 'w');
  browser = spawn(
      binary,
      [
        `--user-data-dir=${dir}/profile`, `--remote-debugging-port=${port}`,
        '--ozone-platform=x11', '--no-first-run', '--no-default-browser-check',
        '--disable-component-update', `--load-extension=${extension}`,
        `--disable-extensions-except=${extension}`, '--disable-gpu',
        '--use-gl=angle', '--use-angle=swiftshader', '--disable-dev-shm-usage',
        'about:blank'
      ],
      {stdio : [ 'ignore', log, log ]});
  const info = await until(async () => {
    if (browser.exitCode !== null)
      throw Error('browser died');
    return (await fetch(`http://127.0.0.1:${port}/json/version`)).json();
  }, 'browser');
  control = await connect(info.webSocketDebuggerUrl);
  await tool('create-agent', {id : agent, name : agent, color : 'blue'});
  const session = await tool('open-session', {agent});
  lane = await connect(session.cdpEndpoint);
  await lane.send('Storage.setCookies', {
    cookies : [
      {name : 'identity', value : 'popup-agent', url : url(''), path : '/'}, {
        name : 'identity',
        value : 'popup-agent',
        url : `http://localhost:${site.address().port}/`,
        path : '/'
      }
    ]
  });
  const opener = await lane.send('Target.createTarget', {url : 'about:blank'});
  const osid = await attach(lane, opener.targetId);
  await lane.send('Page.navigate', {url : url('Opener')}, osid);
  await until(() => evaluate(lane, osid, 'document.title==="Opener"'),
              'opener');
  await control.send('Target.activateTarget', {targetId : opener.targetId});
  const group = await until(() => native('Opener - Shuttle', 'agent-group.png'),
                            'agent group');
  async function popup(name, expression, client = lane, sid = osid) {
    const before = new Set((await client.send('Target.getTargets'))
                               .targetInfos.map(t => t.targetId));
    await evaluate(client, sid, expression + ";void 0");
    const target = await until(
        async () => (await client.send('Target.getTargets'))
                        .targetInfos.find(t => !before.has(t.targetId) &&
                                               t.title === name),
        name);
    const psid = await attach(client, target.targetId);
    const cookie = await evaluate(client, psid, 'document.cookie');
    check(`${name} uses opener cookies`,
          cookie.includes(client === lane ? 'identity=popup-agent'
                                          : 'identity=ordinary'),
          cookie);
    await sleep(200);
    const shot = `${name}.png`,
          n = await until(() => native(name, shot), `native ${name}`);
    const expected = client === lane && mode === 'patched';
    check(`${name} native title`,
          n.title ===
              (expected ? `${name} - ${agent} - Shuttle` : `${name} - Shuttle`),
          n);
    check(`${name} browser chip color`,
          expected ? colors(n).length > 0 : colors(n).length === 0, colors(n));
    return {target, psid, n};
  }
  // Same click path as a login link; do not use a real login.
  const first =
      await popup('Popup-main', "document.querySelector('#popup').click()");
  check('popup matches tab group color',
        mode === 'baseline' ||
            colors(first.n).some(
                c => colors(group).some(g => JSON.stringify(g.rgb) ===
                                             JSON.stringify(c.rgb))),
        {popup : colors(first.n), group : colors(group)});
  const request = requests.find(r => r.url === '/Popup-main');
  check('first popup request uses agent cookies',
        request?.cookie === 'identity=popup-agent', request);
  await popup('Popup-redirect',
              "window.open('/oauth','oauth','popup,width=700,height=480')");
  await popup(
      'Popup-noopener',
      "window.open('/Popup-noopener','_blank','noopener,popup,width=700,height=480')");
  await popup(
      'Popup-blank',
      "const p=window.open('about:blank','blank','popup,width=700,height=480');p.location='/Popup-blank'");
  await popup(
      'Popup-nested',
      "window.open('/Popup-nested','nested','popup,width=700,height=480')",
      lane, first.psid);
  // A session-created tab remains in the same cookie jar when it opens a popup.
  const cdpTab = await lane.send('Target.createTarget',
                                 {url : url('CDP-opener'), newWindow : true});
  const csid = await attach(lane, cdpTab.targetId);
  await until(() => evaluate(lane, csid, 'document.title==="CDP-opener"'),
              'CDP opener');
  await popup('Popup-cdp',
              "window.open('/Popup-cdp','cdp','popup,width=700,height=480')",
              lane, csid);
  // Extension-created app popups suppress the location bar without this
  // feature.
  const extTab = await control.send(
      'Target.createTarget',
      {url : `chrome-extension://${extensionId}/probe.html`});
  const esid = await attach(control, extTab.targetId);
  await until(() => evaluate(control, esid, '!!globalThis.chrome?.tabs'),
              'extension API');
  const trusted =
      await lane.send('Target.createTarget', {url : url('Popup-trusted')});
  const tsid = await attach(lane, trusted.targetId);
  await until(() => evaluate(lane, tsid, 'document.title==="Popup-trusted"'),
              'trusted tab');
  const tabId =
      await evaluate(control, esid,
                     `chrome.tabs.query({}).then(ts=>ts.find(t=>t.url===${
                         JSON.stringify(url('Popup-trusted'))}).id)`);
  await evaluate(
      control, esid,
      `chrome.windows.create({tabId:${
          tabId},type:'popup',focused:false,width:700,height:520}).then(w=>w.id)`);
  await sleep(400);
  const tn = await until(() => native('Popup-trusted', 'Popup-trusted.png'),
                         'trusted popup');
  check('trusted popup title',
        tn.title === (mode === 'patched' ? `Popup-trusted - ${agent} - Shuttle`
                                         : 'Popup-trusted'),
        tn);
  check('trusted popup has browser identity row',
        mode === 'baseline' || colors(tn).length > 0, colors(tn));
  const trustedTarget = (await control.send('Target.getTargets'))
                            .targetInfos.find(t => t.title === 'Popup-trusted');
  const trustedSid = await attach(control, trustedTarget.targetId);
  const trustedCookie = await evaluate(control, trustedSid, 'document.cookie');
  check('trusted popup retains cookies',
        trustedCookie === 'identity=popup-agent', trustedCookie);
  check('trusted popup keeps original target',
        trustedTarget.targetId === trusted.targetId);
  check('app popup never reloads into ordinary cookies',
        requests.filter(r => r.url === '/Popup-trusted')
            .every(r => r.cookie === 'identity=popup-agent'),
        requests.filter(r => r.url === '/Popup-trusted'));
  if (mode === 'patched') {
    const appSid = await attach(lane, trustedTarget.targetId);
    await popup(
        'Popup-app-child',
        "window.open('/Popup-app-child','app-child','popup,width=700,height=480')",
        lane, appSid);
  }
  const win = await control.send('Browser.getWindowForTarget',
                                 {targetId : trustedTarget.targetId});
  await control.send(
      'Browser.setWindowBounds',
      {windowId : win.windowId, bounds : {width : 360, height : 500}});
  await sleep(250);
  const narrow = native('Popup-trusted', 'narrow.png');
  check('narrow popup retains identity chip',
        mode === 'baseline' || colors(narrow).length > 0, narrow);
  // Page titles cannot change the native identity suffix or chip.
  await evaluate(lane, first.psid, "document.title='Spoofed agent-other'");
  const spoof = await until(() => {
    const n = native('Spoofed agent-other', 'spoof.png');
    return n.title.includes('Spoofed') && n;
  }, 'spoof title');
  check('page title cannot replace identity',
        mode === 'baseline' ||
            spoof.title === `Spoofed agent-other - ${agent} - Shuttle`,
        spoof);
  if (mode === 'patched') {
    await tool('update-agent',
               {id : agent, name : 'Renamed popup agent', color : 'green'});
    const renamed = await until(() => {
      const n = native('Spoofed agent-other', 'renamed.png');
      return n.title ===
                 'Spoofed agent-other - Renamed popup agent - Shuttle' &&
             n;
    }, 'renamed chrome');
    check('open popup updates name and color',
          colors(renamed).some(c => c.rgb[1] > c.rgb[2] + 20), renamed);
    const appRenamed = await until(() => {
      const n = native('Popup-trusted', 'app-renamed.png');
      return n.title === 'Popup-trusted - Renamed popup agent - Shuttle' && n;
    }, 'renamed app chrome');
    check('app popup updates name and color',
          colors(appRenamed).some(c => c.rgb[1] > c.rgb[2] + 20), appRenamed);
  }
  // Ordinary profile popup must retain its own cookies and have no chip.
  const ordinary =
      await control.send('Target.createTarget', {url : 'about:blank'});
  const usid = await attach(control, ordinary.targetId);
  await control.send('Page.navigate', {url : url('Ordinary-opener')}, usid);
  await until(() =>
                  evaluate(control, usid, 'document.title==="Ordinary-opener"'),
              'ordinary opener');
  await evaluate(control, usid, "document.cookie='identity=ordinary; Path=/'");
  await popup(
      'Popup-ordinary',
      "window.open('/Popup-ordinary','ordinary','popup,width=700,height=480')",
      control, usid);
  const ordinaryTabId =
      await evaluate(control, esid,
                     `chrome.tabs.query({}).then(ts=>ts.find(t=>t.url===${
                         JSON.stringify(url('Ordinary-opener'))}).id)`);
  await evaluate(
      control, esid,
      `chrome.windows.create({tabId:${
          ordinaryTabId},type:'popup',focused:false,width:700,height:520}).then(w=>w.id)`);
  await sleep(300);
  const ordinaryApp = native('Ordinary-opener', 'ordinary-app.png');
  check('ordinary app popup has no extra identity',
        ordinaryApp.title === 'Ordinary-opener' &&
            colors(ordinaryApp).length === 0,
        ordinaryApp);
  const agentCookies = await lane.send('Storage.getCookies');
  check('ordinary popup leaves agent cookies isolated',
        agentCookies.cookies.filter(c => c.name === 'identity')
            .every(c => c.value === 'popup-agent'),
        agentCookies.cookies.map(c => ({name : c.name, value : c.value})));
} catch (e) {
  check('probe completed', false, e.stack);
} finally {
  try {
    if (lane)
      await tool('delete-agent', {id : agent});
  } catch (e) {
    check('delete test agent', false, e.message);
  }
  lane?.close();
  try {
    await control?.send('Browser.close');
  } catch {
  }
  for (const s of sockets)
    s.close();
  if (browser) {
    await until(() => browser.exitCode !== null, 'browser exit');
    check('browser exits cleanly', browser.exitCode === 0, browser.exitCode);
  }
  site.close();
  rmSync(path.join(dir, 'profile'), {recursive : true, force : false});
  writeFileSync(path.join(dir, 'results.json'),
                JSON.stringify({mode, checks, requests}, null, 2) + '\n');
}
process.exitCode = checks.some(c => !c.ok) ? 1 : 0;
