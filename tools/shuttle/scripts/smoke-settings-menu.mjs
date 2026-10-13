// Lists the Settings side-menu entries via CDP. Usage: node scripts/smoke-settings-menu.mjs
// (browser on port 9333; launch with `open -g` as described in smoke-cdp.mjs).
const port=9333; const v=await (await fetch(`http://127.0.0.1:${port}/json/version`)).json();
const ws=new WebSocket(v.webSocketDebuggerUrl); await new Promise(r=>ws.addEventListener('open',r,{once:true}));
let id=0; const p=new Map(); ws.addEventListener('message',e=>{const m=JSON.parse(e.data); if(m.id&&p.has(m.id)){p.get(m.id)(m);p.delete(m.id);}});
const send=(method,params={},sessionId)=>new Promise(r=>{const i=++id;p.set(i,r);ws.send(JSON.stringify({id:i,method,params,...(sessionId?{sessionId}:{})}));});
const t=await send('Target.createTarget',{url:'chrome://settings/',background:true}); const a=await send('Target.attachToTarget',{targetId:t.result.targetId,flatten:true});
await new Promise(r=>setTimeout(r,3000));
const expr=`(()=>{const ui=document.querySelector('settings-ui');const menu=ui.shadowRoot.querySelector('settings-menu');return [...menu.shadowRoot.querySelectorAll('a[role=menuitem]')].filter(a=>!a.hidden).map(a=>a.id||a.textContent.trim()).join(', ');})()`;
const r=await send('Runtime.evaluate',{expression:expr,returnByValue:true},a.result.sessionId);
console.log(JSON.stringify(r.result.result.value ?? r.result));
ws.close();
