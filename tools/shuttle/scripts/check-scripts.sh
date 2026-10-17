#!/usr/bin/env bash
# check-scripts.sh: self-check for the tooling. Run before every tooling commit.
#  1. syntax per interpreter: zsh -n (zsh shebang), bash -n + shellcheck -S warning if installed (bash/sh),
#     in-memory compile (Python, no .pyc), node --check (.mjs); .ps1 is listed unverified (host-only).
#  2. fixtures: every tools/shuttle/fixtures/*.sh, each in its own temp dir with an isolated env (see fixtures/lib.sh).
# Exit 0 = all green, 1 = something failed. `check-scripts.sh --fixtures-only` / `--syntax-only` run one half.
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd -P)"; tools="$(dirname "$here")"
fail=0; n=0
bad() { echo "FAIL $*"; fail=1; }
if [[ ${1:-} != --fixtures-only ]]; then
  sc=$(command -v shellcheck || true); [[ -n $sc ]] || echo "note: shellcheck not installed (brew install shellcheck)"
  while IFS= read -r f; do
    n=$((n + 1)); first=$(head -1 "$f")
    case "$f" in
      *.py) python3 -c 'import sys; compile(open(sys.argv[1]).read(), sys.argv[1], "exec")' "$f" 2>&1 || bad "$f" ;;
      *.mjs) node --check "$f" 2>&1 || bad "$f" ;;
      *.ps1) echo "unverified (host-only): ${f#"$tools"/}" ;;
      */lib/paths.sh) { bash -n "$f" && zsh -n "$f"; } 2>&1 || bad "$f"
                      [[ -z $sc ]] || "$sc" -S warning -s bash "$f" || bad "$f (shellcheck)" ;;
      *)
        if [[ $first == *zsh* ]]; then zsh -n "$f" 2>&1 || bad "$f"
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
    d=$(mktemp -d "${TMPDIR:-/tmp}/sh-fixture.XXXXXX")
    if out=$(cd "$d" && FIXTURE_TMP="$d" SCRIPTS="$here" bash "$t" 2>&1); then echo "ok   $(basename "$t")"
    else bad "$(basename "$t")"; echo "$out" | sed 's/^/     /'; fi
    rm -rf "$d"
  done
fi
exit $fail
