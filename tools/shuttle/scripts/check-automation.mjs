// Checks fork commit 16 (always-on remote debugging, no automation UI). Opt-in probe:
// scripts/smoke-all.sh --with automation launches its own browser WITHOUT --remote-debugging-port and with
// --enable-automation (CDP comes up on 9222, or a free port when 9222 is taken) and reads DevToolsActivePort.
// Usage: node scripts/check-automation.mjs <port> [userDataDir]
// The infobar check needs browser UI: in headless (no UI height) it is reported skipped.
import { readFileSync } from 'node:fs';
import { args, browserWs, cdp, check, cleanup, evaluate, finish, json, port, skipCheck, sleep } from './lib/smoke.mjs';

const userDataDir = args[0];
const version = await json('/json/version');
check('server answers /json/version', version?.Browser, version?.Browser);
if (userDataDir) {
  let first = '';
  try { first = readFileSync(`${userDataDir}/DevToolsActivePort`, 'utf8').split('\n')[0]; } catch {}
  check('DevToolsActivePort written', first === String(port), JSON.stringify(first));
}
const b = await cdp(await browserWs());
check('browser websocket connects without approval', true);

const t = (await b.send('Target.createTarget', { url: 'data:text/html,<title>automation</title>a' })).result.targetId;
cleanup(() => b.send('Target.closeTarget', { targetId: t }));
const sessionId = (await b.send('Target.attachToTarget', { targetId: t, flatten: true })).result.sessionId;
await sleep(2000);
check('navigator.webdriver === false', (await evaluate(b, sessionId, 'navigator.webdriver')) === false);
await b.send('Emulation.setAutomationOverride', { enabled: true }, sessionId);
check('still false after Emulation.setAutomationOverride', (await evaluate(b, sessionId, 'navigator.webdriver')) === false);

// Infobars live in the browser UI above the page, so they shrink innerHeight. Tab strip +
// toolbar alone take ~80-95 px; an infobar adds ~40 px (old code: one per tab on override).
const chromeHeight = await evaluate(b, sessionId, 'window.outerHeight - window.innerHeight');
if (chromeHeight === 0) skipCheck('no infobar', 'no browser UI (headless)');
else check('no infobar (browser UI height < 110 px)', chromeHeight > 0 && chromeHeight < 110, `${chromeHeight} px`);

// chrome://inspect loads without JS errors and hides the remote-debugging toggle.
const t2 = (await b.send('Target.createTarget', { url: 'about:blank', background: true })).result.targetId;
cleanup(() => b.send('Target.closeTarget', { targetId: t2 }));
const s2 = (await b.send('Target.attachToTarget', { targetId: t2, flatten: true })).result.sessionId;
await b.send('Runtime.enable', {}, s2);
const start = b.events.length;
await b.send('Page.navigate', { url: 'chrome://inspect/#devices' }, s2);
await sleep(2500);
const errors = b.events.slice(start).filter((m) => m.sessionId === s2 && m.method === 'Runtime.exceptionThrown')
  .map((m) => m.params.exceptionDetails.text);
const inspectTitle = await evaluate(b, s2, 'document.title');
check('chrome://inspect loads without JS errors', inspectTitle && errors.length === 0, `${inspectTitle} ${errors.join('; ')}`);
await finish();
