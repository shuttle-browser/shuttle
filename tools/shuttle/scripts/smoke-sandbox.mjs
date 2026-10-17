// chrome://sandbox/ must report "You are adequately sandboxed." (Linux; the page does not exist on macOS: skip).
// Usage: node scripts/smoke-sandbox.mjs <port>   (run by scripts/smoke-all.sh)
import { browserWs, cdp, check, cleanup, evaluate, finish, skip, sleep } from './lib/smoke.mjs';

if (process.platform !== 'linux') await skip(`chrome://sandbox/ is not built on ${process.platform}`);
const b = await cdp(await browserWs());
const { targetId } = (await b.send('Target.createTarget', { url: 'chrome://sandbox/', background: true })).result;
cleanup(() => b.send('Target.closeTarget', { targetId }));
const sid = (await b.send('Target.attachToTarget', { targetId, flatten: true })).result.sessionId;
await sleep(2500);
const status = await evaluate(b, sid, 'document.getElementById("evaluation")?.innerText ?? document.body.innerText.slice(0, 200)');
check('chrome://sandbox/ adequately sandboxed', status === 'You are adequately sandboxed.', status);
await finish();
