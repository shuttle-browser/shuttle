#!/usr/bin/env bash
# check-scripts.sh: self-check for the tooling. Run before every tooling commit.
#  1. syntax per interpreter: zsh -n (zsh shebang), bash -n + shellcheck -S warning if installed (bash/sh),
#     in-memory compile (Python, no .pyc), node --check (.mjs); .ps1 is listed unverified (host-only).
#  2. fixtures: every tools/shuttle/fixtures/*.sh, each in its own temp dir with an isolated env (see fixtures/lib.sh).
#     A fixture with a `# requires: <cmd> ...` line is listed unverified when a command is missing.
# Without zsh (Linux hosts): zsh scripts are listed unverified, lib/*.sh still get bash -n. Unverified items are
# printed and counted but don't fail the run: run it on the Mac before a tooling commit.
# Exit 0 = all green, 1 = something failed. `check-scripts.sh --fixtures-only` / `--syntax-only` run one half.
# Env: SHUTTLE_CHECK_ZSH (zsh binary, default zsh; the fixture sets a missing one).
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd -P)"; tools="$(dirname "$here")"
fail=0; n=0; unv=0
zsh=${SHUTTLE_CHECK_ZSH:-zsh}; command -v "$zsh" >/dev/null || zsh=
unverified() { echo "unverified ($1): $2"; unv=$((unv + 1)); }
bad() { echo "FAIL $*"; fail=1; }
if [[ ${1:-} != --fixtures-only ]]; then
  hint="brew install shellcheck"; [[ $(uname) == Linux ]] && hint="sudo apt install shellcheck"
  sc=$(command -v shellcheck || true); [[ -n $sc ]] || echo "note: shellcheck not installed ($hint)"
  [[ -n $zsh ]] || echo "note: zsh not installed: zsh scripts are not syntax-checked here"
  while IFS= read -r f; do
    n=$((n + 1)); first=$(head -1 "$f")
    case "$f" in
      *.py) python3 -c 'import sys; compile(open(sys.argv[1]).read(), sys.argv[1], "exec")' "$f" 2>&1 || bad "$f" ;;
      *.mjs) node --check "$f" 2>&1 || bad "$f" ;;
      *.ps1) echo "unverified (host-only): ${f#"$tools"/}" ;;
      */scripts/lib/*.sh) { bash -n "$f" && { [[ -z $zsh ]] || "$zsh" -n "$f"; }; } 2>&1 || bad "$f"
                      [[ -z $sc ]] || "$sc" -S warning -s bash "$f" || bad "$f (shellcheck)" ;;
      *)
        if [[ $first == *zsh* ]]; then
          if [[ -n $zsh ]]; then "$zsh" -n "$f" 2>&1 || bad "$f"; else unverified "no zsh" "${f#"$tools"/}"; fi
        elif [[ $first == *bash* || $first == *bin/sh* ]]; then
          bash -n "$f" 2>&1 || bad "$f"; [[ -z $sc ]] || "$sc" -S warning -x "$f" || bad "$f (shellcheck)"
        else bad "$f: unknown interpreter ($first)"; fi ;;
    esac
  done < <(find "$tools/scripts" "$tools/fixtures" -type f ! -path '*/__pycache__/*' ! -name '*.md' | sort)
  echo "syntax: $n files checked"
fi
if [[ ${1:-} != --syntax-only ]]; then
  for t in "$tools"/fixtures/*.sh; do
    [[ $(basename "$t") == lib.sh ]] && continue
    miss=; for c in $(sed -n 's/^# requires: //p' "$t"); do
      if [[ $c == zsh ]]; then [[ -n $zsh ]] || miss+=" $c"; else command -v "$c" >/dev/null || miss+=" $c"; fi
    done
    [[ -z $miss ]] || { unverified "no$miss" "$(basename "$t")"; continue; }
    d=$(mktemp -d "${TMPDIR:-/tmp}/sh-fixture.XXXXXX")
    if out=$(cd "$d" && FIXTURE_TMP="$d" SCRIPTS="$here" bash "$t" 2>&1); then echo "ok   $(basename "$t")"
    else bad "$(basename "$t")"; echo "$out" | sed 's/^/     /'; fi
    rm -rf "$d"
  done
fi
(( unv == 0 )) || echo "unverified: $unv (missing tools on this host; not a pass)"
exit $fail
