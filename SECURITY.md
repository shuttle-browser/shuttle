# Security policy

## Reporting a vulnerability

Please report security issues privately through GitHub:
**[Report a vulnerability](https://github.com/shuttle-browser/shuttle/security/advisories/new)**
(Security tab → "Report a vulnerability"). Don't open a public issue.

Include the Shuttle version (`chrome://version`), your OS, steps to reproduce, and the impact. We aim to reply
within a few days.

## Scope

- **Shuttle's own code**: the MCP server and agent/session model (`chrome/browser/shuttle/`), per-session CDP
  endpoints, `chrome://agents`, the bundled agent-browser integration, and our build and release scripts.
- **Chromium vulnerabilities** that also affect Google Chrome should go to the
  [Chromium security team](https://www.chromium.org/Home/chromium-security/reporting-security-bugs/). Tell us too if
  Shuttle is affected differently.

## Supported versions and updates

Shuttle is a beta. Only the latest release gets fixes. There is **no auto-update yet**: watch
[Releases](https://github.com/shuttle-browser/shuttle/releases). Shuttle follows Chromium stable security releases,
with a delay while we merge and rebuild.

## Threat model notes

- The CDP/MCP port (default 9222) listens on 127.0.0.1 and has no authentication: any local process can drive the
  browser, as with Chrome's `--remote-debugging-port`. Web pages can't reach it: requests with a foreign `Origin`
  or a non-local `Host` header are rejected.
- A per-session CDP endpoint only sees that session's tabs and its agent's storage. It is a convenience boundary
  between cooperating agents, not a sandbox against hostile local code.
