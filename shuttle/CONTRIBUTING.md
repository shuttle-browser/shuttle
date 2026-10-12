# Contributing

Thanks for helping. The fork (github.com/shuttle-browser/shuttle) holds the browser source and, in `shuttle/`,
its tooling (build, prune, rebrand and smoke-test scripts, GN args, branding, docs).

## Issues and pull requests

- Open an issue first for anything larger than a fix: say what you want to change and why.
- Keep PRs small and focused; describe how you tested them. Scripts should stay runnable on macOS and Linux
  and must not hard-code machine names, absolute paths or credentials (machine-local settings go in
  `~/.shuttle/env`, see `docs/TOOLING.md`).
- Docs matter: update `docs/TOOLING.md` when you add a script or discover a gotcha, and `docs/REMOVAL-LOG.md`
  for removal batches.

## Running the smokes

Smokes run against a built browser (`out/Dev` of the fork) with a fresh profile, headless, and end with
`Browser.close`:

```sh
../out/Dev/Shuttle.app/Contents/MacOS/Shuttle --headless --user-data-dir=/tmp/ac-profile \
  --remote-debugging-port=9222 &      # Linux: out/Dev/shuttle
node scripts/smoke-cdp.mjs 9222
node scripts/smoke-mcp.mjs 9222
```

See the smoke table in `README.md` and `docs/LINUX.md` for the Linux wrappers (`scripts/linux-smoke.sh`).
Node 22+ is required (global `WebSocket`).

## Licensing

By contributing you agree that your contribution is licensed under the BSD 3-Clause license in `../LICENSE-SHUTTLE`.
