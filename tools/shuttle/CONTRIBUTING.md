# Contributing

Thanks for helping. The fork (github.com/shuttle-browser/shuttle) holds the browser source and, in `tools/shuttle/`,
its tooling (build, prune, rebrand and smoke-test scripts, GN args, branding, docs).

## Issues and pull requests

- Open an issue first for anything larger than a fix: say what you want to change and why.
- Keep PRs small and focused; describe how you tested them. Scripts should stay runnable on macOS and Linux
  and must not hard-code machine names, absolute paths or credentials (machine-local settings go in
  `~/.shuttle/env`, see `docs/TOOLING.md`).
- Docs matter: update `docs/TOOLING.md` when you add a script or discover a gotcha, and `docs/REMOVAL-LOG.md`
  for removal batches.

## Running the smokes

Smokes run against a built browser. `scripts/smoke-all.sh` launches it headless with its own temp profile, runs
the probes, closes it and prints PASS/FAIL/SKIP per probe:

```sh
scripts/smoke-all.sh ../../out/Release/Shuttle.app      # Linux: ../../out/Dev/shuttle [--mode x11|wayland]
```

See the smoke table in `README.md`; Linux modes in `docs/LINUX.md`.
Node 22+ is required (global `WebSocket`).

## Licensing

By contributing you agree that your contribution is licensed under the BSD 3-Clause license in `../LICENSE-SHUTTLE`.
