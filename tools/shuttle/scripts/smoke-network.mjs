#!/usr/bin/env node
// Network-mode smoke (opt-in: scripts/smoke-all.sh --with network launches a browser with --shuttle-listen=0.0.0.0).
// Usage: node scripts/smoke-network.mjs <port> <userDataDir> [lanIp]
// Checks: loopback works without a token; the LAN address needs the token (header or ?token=), relaxes the Host
// check for token holders, keeps the Origin check; MCP endpoints carry the token and work over the LAN; the token
// file is 0600. Fails when no LAN address can be determined.
import { execSync } from 'node:child_process'
import fs from 'node:fs'
import http from 'node:http'
import path from 'node:path'
import { args, check, cleanup, fail, finish, port, sleep } from './lib/smoke.mjs'

const [udd, lanArg] = args
if (!udd) { console.error('usage: smoke-network.mjs <port> <userDataDir> [lanIp]'); process.exit(2) }
let lan = lanArg
try { lan ||= execSync("ipconfig getifaddr en0 2>/dev/null || hostname -I | awk '{print $1}'").toString().trim() } catch {}
if (!lan) await fail('no LAN address (pass lanIp)')
const tokenFile = path.join(udd, 'ShuttleAccessToken')
for (let i = 0; i < 30 && !fs.existsSync(tokenFile); i++) await sleep(500)  // a cold first start can take >5 s
const token = fs.readFileSync(tokenFile, 'utf8').trim()
const get = async (url, headers = {}) => { try { const r = await fetch(url, { headers, signal: AbortSignal.timeout(15000) }); return { status: r.status, text: await r.text() } } catch (e) { return { status: 0, text: String(e) } } }
const mcp = async (base, method, params, headers = {}) => {
  const r = await fetch(`${base}/mcp`, { method: 'POST', signal: AbortSignal.timeout(15000), headers: { 'content-type': 'application/json', accept: 'application/json, text/event-stream', ...headers }, body: JSON.stringify({ jsonrpc: '2.0', id: 1, method, params }) })
  return { status: r.status, body: await r.text() }
}
const wsOpen = (url, headers) => new Promise((res) => {
  const ws = new WebSocket(url, headers ? { headers } : undefined)
  const t = setTimeout(() => { res({ ok: false, why: 'timeout' }); try { ws.close() } catch {} }, 5000)
  ws.onopen = () => { ws.send(JSON.stringify({ id: 1, method: 'Target.getTargets' })) }
  ws.onmessage = (e) => { clearTimeout(t); const m = JSON.parse(e.data); res({ ok: !!m.result, targets: m.result?.targetInfos?.length ?? 0 }); ws.close() }
  ws.onerror = () => { clearTimeout(t); res({ ok: false, why: 'error' }) }
})

const local = `http://127.0.0.1:${port}`, remote = `http://${lan}:${port}`
const auth = { authorization: `Bearer ${token}` }
console.log(`LAN address ${lan}, token ${token.length} chars`)

const mode = (fs.statSync(tokenFile).mode & 0o777).toString(8)
check('token file is 0600', mode === '600', mode)
check('loopback /json/version without token', (await get(`${local}/json/version`)).status === 200)
check('LAN /json/version without token → 401', (await get(`${remote}/json/version`)).status === 401)
check('LAN with wrong token → 401', (await get(`${remote}/json/version`, { authorization: 'Bearer ' + 'x'.repeat(token.length) })).status === 401)
check('LAN with Bearer token → 200', (await get(`${remote}/json/version`, auth)).status === 200)
check('LAN with ?token= → 200', (await get(`${remote}/json/version?token=${token}`)).status === 200)
const rawGet = (host, headers) => new Promise((res) => {
  http.get({ host: lan, port, path: '/json/version', headers: { host, ...headers } }, (r) => { r.resume(); res(r.statusCode) }).on('error', () => res(0))
})
check('LAN token holder with a host name (Host check relaxed)', (await rawGet(`shuttle-box:${port}`, auth)) === 200)
check('LAN host name without token → 401', (await rawGet(`shuttle-box:${port}`, {})) === 401)
check('LAN token holder from a web Origin → 403', (await mcp(remote, 'initialize', { protocolVersion: '2025-06-18', capabilities: {}, clientInfo: { name: 'smoke', version: '1' } }, { ...auth, origin: 'https://evil.example' })).status === 403)
check('LAN /mcp without token → 401', (await mcp(remote, 'tools/list', {})).status === 401)

const init = await mcp(remote, 'initialize', { protocolVersion: '2025-06-18', capabilities: {}, clientInfo: { name: 'smoke', version: '1' } }, auth)
check('LAN /mcp initialize with token', init.status === 200 && init.body.includes('shuttle'), String(init.status))
const call = async (name, args) => {
  const r = await mcp(remote, 'tools/call', { name, arguments: args }, auth)
  const m = JSON.parse(r.body.replace(/^data: /m, '').trim().split('\n').pop())
  return JSON.parse(m.result?.content?.[0]?.text ?? '{}')
}
await call('create-agent', { id: 'smoke-net', name: 'Net smoke' })
cleanup(() => call('delete-agent', { id: 'smoke-net' }))
const sess = await call('open-session', { agent: 'smoke-net' })
cleanup(() => call('close-session', { sessionId: sess.sessionId }))
const ep = sess.cdpEndpoint ?? ''
check('cdpEndpoint carries the token', ep.includes(`token=${token}`), ep.replace(token, '<token>'))
check('cdpEndpoint uses the LAN host', ep.startsWith(`ws://${lan}:${port}/devtools/session/`))
const tab = await call('allocate-tab', { sessionId: sess.sessionId, url: 'about:blank' })
check('allocate-tab cdpUrl carries the token', (tab.cdpUrl ?? '').includes(`token=${token}`))
check('session CDP over LAN with token', (await wsOpen(ep)).ok)
check('session CDP over LAN without token fails', !(await wsOpen(ep.replace(/[?&]token=[^&]+/, ''))).ok)
check('session CDP over LAN with Bearer header', (await wsOpen(ep.replace(/[?&]token=[^&]+/, ''), auth)).ok)
const snap = await mcp(remote, 'tools/call', { name: 'snapshot', arguments: { targetId: tab.targetId } }, auth)
check('page tool (agent-browser sidecar) works in network mode', snap.status === 200 && !/isError":true/.test(snap.body), snap.body.slice(0, 120))
await finish()
