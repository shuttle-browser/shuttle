// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Usage: node scripts/check-popup-cookies.mjs <binary> <evidence-dir>
// Owns its browser, temporary profile and port. Uses neutral loopback pages.
import {spawn} from 'node:child_process';
import {createHash} from 'node:crypto';
import {mkdirSync, mkdtempSync, openSync, readFileSync, rmSync,
        writeFileSync, closeSync} from 'node:fs';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const [binary, evidenceDir] = process.argv.slice(2);
if (!binary || !evidenceDir) throw Error('binary and evidence directory required');
if (process.env.SHUTTLE_POPUP_TEST_DISPLAY &&
    process.env.DISPLAY !== process.env.SHUTTLE_POPUP_TEST_DISPLAY) {
  throw Error('DISPLAY must match the dedicated SHUTTLE_POPUP_TEST_DISPLAY');
}
mkdirSync(evidenceDir, {recursive: true});
const profile = mkdtempSync(path.join(os.tmpdir(), 'shuttle-popup-cookies-'));
const extension = fileURLToPath(new URL('../test-data/popup-extension', import.meta.url));
const manifest = JSON.parse(readFileSync(path.join(extension, 'manifest.json')));
const extensionId = [...createHash('sha256').update(Buffer.from(manifest.key, 'base64'))
    .digest('hex').slice(0, 32)].map(n => String.fromCharCode(97 + parseInt(n, 16))).join('');
const checks = [], requests = [], sockets = [];
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
function check(name, ok, detail) {
  checks.push({name, ok, detail});
  console.log(`${ok ? 'ok' : 'FAIL'} ${name}: ${JSON.stringify(detail ?? '')}`);
}
async function until(fn, label) {
  for (let i = 0; i < 100; i++) {
    const value = await fn();
    if (value) return value;
    await sleep(100);
  }
  throw Error(`timeout: ${label}`);
}
async function connect(url) {
  const ws = new WebSocket(url), pending = new Map();
  let nextId = 0;
  sockets.push(ws);
  ws.onmessage = event => {
    const message = JSON.parse(event.data), call = pending.get(message.id);
    if (!call) return;
    pending.delete(message.id);
    clearTimeout(call.timer);
    message.error ? call.reject(Error(JSON.stringify(message.error))) : call.resolve(message.result);
  };
  await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
  return {
    send(method, params = {}, sessionId) {
      return new Promise((resolve, reject) => {
        const id = ++nextId;
        const timer = setTimeout(() => {
          pending.delete(id);
          reject(Error(`timeout: ${method}`));
        }, 15000);
        pending.set(id, {resolve, reject, timer});
        ws.send(JSON.stringify({id, method, params, ...(sessionId ? {sessionId} : {})}));
      });
    },
  };
}
async function attach(client, targetId) {
  return (await client.send('Target.attachToTarget', {targetId, flatten: true})).sessionId;
}
async function evaluate(client, sessionId, expression) {
  const result = await client.send('Runtime.evaluate', {
    expression, returnByValue: true, awaitPromise: true, userGesture: true,
  }, sessionId);
  if (result.exceptionDetails) throw Error(JSON.stringify(result.exceptionDetails));
  return result.result?.value;
}
let browser, control, lane, port;
async function tool(name, args = {}) {
  const response = await (await fetch(`http://127.0.0.1:${port}/mcp`, {
    method: 'POST', headers: {'content-type': 'application/json'},
    body: JSON.stringify({jsonrpc: '2.0', id: 1, method: 'tools/call',
      params: {name, arguments: args}}),
  })).json();
  if (response.error || response.result?.isError) throw Error(JSON.stringify(response));
  return response.result.structuredContent;
}
const site = http.createServer((req, res) => {
  requests.push({url: req.url, cookie: req.headers.cookie || ''});
  res.setHeader('content-type', 'text/html');
  res.end(`<title>${req.url.slice(1)}</title><h1>Neutral popup cookie test</h1>`);
});
await new Promise(resolve => site.listen(0, '127.0.0.1', resolve));
const url = name => `http://127.0.0.1:${site.address().port}/${name}`;
const agent = 'popup-cookie-probe';
const log = openSync(path.join(evidenceDir, 'browser.log'), 'w');
const deadline = setTimeout(() => browser?.kill('SIGTERM'), 180000);
deadline.unref();
try {
  browser = spawn(path.resolve(binary), [
    `--user-data-dir=${profile}`, '--remote-debugging-port=0',
    '--no-first-run', '--no-default-browser-check', '--disable-background-networking',
    '--disable-component-update', '--disable-gpu', '--disable-dev-shm-usage',
    `--load-extension=${extension}`, `--disable-extensions-except=${extension}`,
    ...(process.env.SHUTTLE_POPUP_TEST_DISPLAY ? ['--ozone-platform=x11'] : ['--headless']),
    'about:blank',
  ], {stdio: ['ignore', log, log]});
  browser.on('error', error => check('browser starts', false, error.message));
  await until(() => {
    if (browser.exitCode !== null) throw Error(`browser exited: ${browser.exitCode}`);
    try {
      port = Number(readFileSync(path.join(profile, 'DevToolsActivePort'), 'utf8').split('\n')[0]);
      return port;
    } catch { return false; }
  }, 'DevToolsActivePort');
  const info = await (await fetch(`http://127.0.0.1:${port}/json/version`)).json();
  control = await connect(info.webSocketDebuggerUrl);
  const ordinary = await control.send('Target.createTarget', {url: url('Ordinary-tab')});
  const ordinarySession = await attach(control, ordinary.targetId);
  await until(() => evaluate(control, ordinarySession, "document.title === 'Ordinary-tab'"), 'ordinary tab');
  await evaluate(control, ordinarySession, "document.cookie = 'identity=ordinary; Path=/'");
  await tool('create-agent', {id: agent, name: 'Popup cookie probe', color: 'blue'});
  const session = await tool('open-session', {agent});
  lane = await connect(session.cdpEndpoint);
  await lane.send('Storage.setCookies', {cookies: [
    {name: 'identity', value: 'agent', url: url(''), path: '/'},
  ]});
  const extensionTab = await control.send('Target.createTarget', {
    url: `chrome-extension://${extensionId}/probe.html`,
  });
  const extensionSession = await attach(control, extensionTab.targetId);
  await until(() => evaluate(control, extensionSession, '!!globalThis.chrome?.tabs'), 'extension API');
  const ext = expression => evaluate(control, extensionSession, expression);
  const tabFor = title => ext(`chrome.tabs.query({}).then(tabs => tabs.find(t => t.title === ${JSON.stringify(title)}))`);
  async function newAgentTab(title) {
    // Navigate a blank tab so this probe does not depend on first-request branding changes.
    const target = await lane.send('Target.createTarget', {url: 'about:blank'});
    const sid = await attach(lane, target.targetId);
    await lane.send('Page.navigate', {url: url(title)}, sid);
    await until(() => evaluate(lane, sid, `document.title === ${JSON.stringify(title)}`), title);
    return {targetId: target.targetId, sid};
  }
  const original = await newAgentTab('Moved-agent-tab');
  const agentTab = await until(async () => {
    const tab = await tabFor('Moved-agent-tab');
    return tab?.groupId >= 0 && tab;
  }, 'agent group');
  check('source tab is grouped for the agent', agentTab.groupId >= 0, agentTab.groupId);
  const popup = await ext(`chrome.windows.create({tabId:${agentTab.id},type:'popup',focused:false,width:700,height:480})`);
  // The observer runs after the move. Give a replacement navigation time to finish.
  await sleep(1000);
  const moved = (await control.send('Target.getTargets')).targetInfos.find(t => t.title === 'Moved-agent-tab');
  const movedSid = await attach(control, moved.targetId);
  check('app popup keeps original target', moved.targetId === original.targetId, moved.targetId);
  const movedCookie = await evaluate(control, movedSid, 'document.cookie');
  check('app popup retains agent cookies', movedCookie === 'identity=agent', movedCookie);
  const moveRequests = requests.filter(r => r.url === '/Moved-agent-tab');
  check('app popup never reloads in ordinary storage',
      moveRequests.length > 0 && moveRequests.every(r => r.cookie === 'identity=agent'), moveRequests);
  const sessionTargets = (await lane.send('Target.getTargets')).targetInfos;
  check('moved tab remains in its agent session', sessionTargets.some(t => t.targetId === original.targetId));

  const before = new Set((await control.send('Target.getTargets')).targetInfos.map(t => t.targetId));
  await evaluate(control, movedSid, "window.open('/Popup-child','child','popup,width=600,height=400'); void 0");
  const child = await until(async () => (await control.send('Target.getTargets')).targetInfos
      .find(t => !before.has(t.targetId) && t.title === 'Popup-child'), 'child popup');
  const childSid = await attach(control, child.targetId);
  check('child popup inherits agent cookies',
      (await evaluate(control, childSid, 'document.cookie')) === 'identity=agent');
  check('child popup first request has agent cookies',
      requests.find(r => r.url === '/Popup-child')?.cookie === 'identity=agent');

  // Round trip: back from the popup into the normal window regroups it for the agent, same tab and storage.
  await ext(`chrome.tabs.move(${agentTab.id}, {windowId: ${agentTab.windowId}, index: -1})`);
  const back = await until(async () => {
    const tab = await tabFor('Moved-agent-tab');
    return tab?.windowId === agentTab.windowId && tab.groupId >= 0 && tab;
  }, 'regrouped after the round trip');
  check('round trip back into the normal window regroups the tab', back.groupId >= 0, back.groupId);
  const backTarget = (await control.send('Target.getTargets')).targetInfos.find(t => t.title === 'Moved-agent-tab');
  check('round trip keeps the original target', backTarget?.targetId === original.targetId, backTarget?.targetId);
  check('round trip keeps agent cookies', (await evaluate(control, movedSid, 'document.cookie')) === 'identity=agent');
  check('round trip keeps the tab in its agent session',
      (await lane.send('Target.getTargets')).targetInfos.some(t => t.targetId === original.targetId));

  // Explicit ungrouping in a normal window still means taking the tab back.
  const ungrouped = await newAgentTab('Explicit-ungroup');
  const ungroupTab = await until(async () => {
    const tab = await tabFor('Explicit-ungroup');
    return tab?.groupId >= 0 && tab;
  }, 'second agent group');
  await ext(`chrome.tabs.ungroup(${ungroupTab.id})`);
  const takenBack = await until(async () => (await control.send('Target.getTargets')).targetInfos
      .find(t => t.title === 'Explicit-ungroup' && t.targetId !== ungrouped.targetId), 'ordinary replacement');
  const ordinarySid = await attach(control, takenBack.targetId);
  check('normal-window ungroup still takes the tab back',
      (await evaluate(control, ordinarySid, 'document.cookie')) === 'identity=ordinary');
  check('ordinary profile keeps its own cookies',
      (await evaluate(control, ordinarySession, 'document.cookie')) === 'identity=ordinary');

  // Back into a popup, so the close check below covers a leased tab that sits in an app popup.
  const finalPopup = await ext(`chrome.windows.create({tabId:${agentTab.id},type:'popup',focused:false,width:700,height:480})`);
  await until(async () => (await tabFor('Moved-agent-tab'))?.windowId === finalPopup.id, 'tab back in a popup');
  await tool('close-session', {sessionId: session.sessionId});
  await until(async () => !(await control.send('Target.getTargets')).targetInfos
      .some(t => t.targetId === original.targetId), 'session closes moved tab');
  check('session close releases moved agent tab',
      !(await ext(`chrome.tabs.query({windowId:${finalPopup.id}}).catch(() => [])`)).length);
} catch (error) {
  check('probe completed', false, error.stack);
} finally {
  try { if (port) await tool('delete-agent', {id: agent}); } catch {}
  try { await control?.send('Browser.close'); } catch {}
  for (const socket of sockets) socket.close();
  if (browser?.pid) {
    try { await until(() => browser.exitCode !== null, 'browser exit'); }
    catch { browser.kill('SIGTERM'); await new Promise(resolve => browser.once('exit', resolve)); }
    check('browser exits cleanly', browser.exitCode === 0, browser.exitCode);
  }
  clearTimeout(deadline);
  closeSync(log);
  await new Promise(resolve => site.close(resolve));
  rmSync(profile, {recursive: true});
  writeFileSync(path.join(evidenceDir, 'results.json'), JSON.stringify({checks, requests}, null, 2) + '\n');
}
process.exitCode = checks.some(c => !c.ok) ? 1 : 0;
