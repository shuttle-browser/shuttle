# Trying Shuttle (trial guide)

For people and agent sessions testing Shuttle from their own projects. Shuttle is a Chromium fork built for AI agents: a built-in MCP server manages
**agents** (each with its own cookies, logins and storage, and a tab group showing its name and color),
and agents drive their tabs over the Chrome DevTools Protocol (CDP).

## 1. Make sure it runs

Shuttle runs on port **9222** (CDP + MCP); start it from the app, or from a checkout with:

```bash
src/shuttle/scripts/launch-shuttle.sh
```

It starts **without a window**; the first tab an agent opens creates one. Check it:

```bash
curl -s -X POST http://127.0.0.1:9222/mcp -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"ping"}'
```

Don't run Google Chrome with `--remote-debugging-port=9222` at the same time.

## 2. Connect Claude Code

Add to your project's `.mcp.json` (or `claude mcp add --transport http shuttle http://127.0.0.1:9222/mcp`):

```json
{ "mcpServers": { "shuttle": { "type": "http", "url": "http://127.0.0.1:9222/mcp" } } }
```

Approve the server at session start. If Shuttle was restarted with a new build, run `/mcp` and
reconnect (Claude Code caches tool schemas).

## 3. Tool flow

1. `create-agent {id, name?, color?, avatar?}` once per agent. `id` is kebab-case (`sw-box`); the
   agent keeps its logins across restarts. Colors: grey, blue, red, yellow, green, pink, purple, cyan,
   orange.
2. `open-session {agent}` returns a `sessionId` (and `cdpEndpoint`).
3. `allocate-tab {sessionId, url?, width?, height?}` returns `targetId` and `cdpUrl`. The tab runs in the
   background at full speed and joins the agent's tab group.
4. Drive the tab over CDP (see 4). `navigate-tab`, `set-tab-viewport` (e.g. 390x844 for mobile) and
   `capture-tab` (screenshot, works in the background) are MCP tools too.
5. `release-tab {targetId}` / `close-session {sessionId}` when done. `list-agents`, `list-sessions`
   show what's open; `update-agent` renames or recolors; `delete-agent` wipes the agent's storage.

Popups and `target=_blank` tabs a page opens stay in the agent's storage and session.

## 4. Driving a tab (clicks, typing, reading the page)

The MCP tools manage tabs; interaction goes over CDP:

- **Raw CDP:** open a WebSocket to the tab's `cdpUrl` (Node 22+ has a global `WebSocket`) and send
  `Runtime.evaluate`, `Input.dispatchMouseEvent`, `Input.insertText`, `Page.captureScreenshot`, ...
  The helper `scripts/mcp-call.mjs 9222 <tool> '<json>'` calls MCP tools from the shell.
- **Playwright:** `chromium.connectOverCDP('http://127.0.0.1:9222')`, then pick the page whose target id
  matches (`(await page.context().newCDPSession(page)).send('Target.getTargetInfo')`). Don't use
  `context.newPage()` or `context.cookies()`: they hit the default storage, not the agent's (scoped
  per-agent CDP is planned).

## 5. Watching and managing

- The **tab strip** shows one group per agent (its name and color). Dragging a tab into an agent's group
  hands it to that agent; renaming/recoloring the group updates the agent.
- **`chrome://agents`** (type it in the address bar) lists agents, sessions and tabs: create/edit/delete
  agents, end sessions, click a tab to bring it to the front, and **Open tab** to browse as an agent (for
  example to sign in to a site for it once, so it stays logged in).

## 6. Known limits (trial build)

- No auth on the MCP/CDP port (localhost only).
- All agents share one CDP endpoint; agents should only touch their own tabs (by `targetId`).
- A session whose tabs were all closed by hand stays open (empty) until `close-session`.
- One unexplained "Aw, Snap!" renderer crash was seen once; if you hit it, note the error code under the
  message, the URL and the time.
- Google may show a reCAPTCHA (`/sorry`) on some networks; that's IP-level, solve it once in the agent's
  tab and the agent keeps the cookie.
- Dark-mode `chrome://agents` toolbar still shows the Chrome logo (branding leftover).

## 7. Reporting

Open an issue on https://github.com/shuttle-browser/shuttle/issues (security issues: a private advisory, see
`SECURITY.md`): what you tried, tool calls and errors, URL, expected vs. actual, screenshots (`capture-tab`).
Useful categories: MCP tool ergonomics, CDP driving, site compatibility (logins, bot checks),
multi-agent isolation, tab-group UX, `chrome://agents`.
