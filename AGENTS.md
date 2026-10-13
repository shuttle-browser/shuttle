# Shuttle (fork root)

Shuttle (Shuttle Browser, by Silkweave) is a deep fork of Chromium for AI agents. This directory is the fork
(`github.com/shuttle-browser/shuttle`), checked out as `src/` of a gclient checkout whose root is the parent directory
(`.gclient`, `logs/`, `release/`, `sccache-cache/`). gclient requires the name `src`.

Read, in order:

1. `_docs/AGENTS-TEAM.md`: team, status, rules (private, git-ignored, only on the lead's machine)
2. `_docs/NEXT-SESSION.md`: your handoff
3. `tools/shuttle/AGENTS.md`: tooling, build, removal workflow, gotchas

Run tooling from `tools/shuttle/` (`scripts/...`). The fork is public: run `/dev:gatekeeper` + scoped gitleaks before pushing;
only our own additions are scanned, never Chromium's tree or history.

**Real path for builds.** Siso keys its state by absolute path. If this session's cwd is a symlink (e.g.
`…/shuttle/src`), `cd` to the real checkout (`pwd -P`) before any raw `gn` / `siso` / `autoninja` command; a build seen
under a second path drops the build cache (full rebuild, about a day). The `tools/shuttle/scripts/*` build scripts already
resolve the real path.

## Wrapup Config

- check: Mac `gn gen out/Sccache` passes (from the real path); if fork browser code changed since the last build, a green
  `out/Sccache` build and the CDP smoke test; Linux state confirmed with the Linux session (green or open errors listed)
- test: skip (unit/browser test targets are pruned)
- security: the fork is public: `/dev:gatekeeper` (`gatekeeper --audit` from this directory, our additions only via
  `.claude/gatekeeper.json`) + `gitleaks git --staged` / `--log-opts=github/main..HEAD`, always `GOMAXPROCS=2 nice -n 19
  taskpolicy -c background`, never Chromium's full tree/history
- push: yes, on the maintainer's go: fork `main` to `github` (SH-Lead only; tooling commits unnumbered `shuttle/: …`, browser
  changes = numbered fork commits); never commit `build/util/MINIBRANCH`
- version_bump: no
- publish: no
- docs: public `tools/shuttle/docs/` (TOOLING gotchas, REMOVAL-LOG per fork commit, LINUX, plans) + `tools/shuttle/AGENTS.md`;
  private `_docs/AGENTS-TEAM.md` Status
- next_session: always rewrite `_docs/NEXT-SESSION.md` (Linux session: its own file in `_docs/`): goal, read first,
  current state, ground rules, steps, done-when, open questions; running builds/shells listed last
- team: tell the Linux session what you committed/pushed; add the session's resets and findings to `_docs/AGENTS-FLOW.md`
- frontend_smoke: no (CDP smoke test is part of check)
- co_authored_by: no (global)
- changelog: no
