#!/usr/bin/env bash
# Sourced by fixtures (run by scripts/check-scripts.sh with FIXTURE_TMP = a fresh temp dir, SCRIPTS = scripts/).
# Isolates the env: no ~/.shuttle/env, stub dir first on PATH. Helpers: stub <name> <body>, expect <rc> <cmd...>.
set -uo pipefail
export HOME="$FIXTURE_TMP/home" SHUTTLE_CONF="$FIXTURE_TMP/conf" PATH="$FIXTURE_TMP/bin:$PATH"
mkdir -p "$HOME" "$SHUTTLE_CONF" "$FIXTURE_TMP/bin"
unset SHUTTLE_SRC SHUTTLE_ROOT SHUTTLE_BUILD_DIR
stub() { printf '#!/usr/bin/env bash\n%s\n' "$2" > "$FIXTURE_TMP/bin/$1"; chmod +x "$FIXTURE_TMP/bin/$1"; }
failures=0
expect() {  # expect <rc> <cmd...>: run, compare the exit code, keep output in $OUT
  local want=$1; shift
  OUT=$("$@" 2>&1); local got=$?
  if [[ $got != "$want" ]]; then echo "expected rc $want, got $got: $*"; echo "$OUT" | sed 's/^/  | /'; failures=$((failures + 1)); fi
}
expect_out() { [[ $OUT == *"$1"* ]] || { echo "output lacks '$1':"; echo "$OUT" | sed 's/^/  | /'; failures=$((failures + 1)); }; }
done_fixture() { exit $(( failures > 0 )); }
# fake_src: a git repo with branch `chromium`, exported as SHUTTLE_SRC
fake_src() {
  export SHUTTLE_SRC="$FIXTURE_TMP/src"; mkdir -p "$SHUTTLE_SRC"
  git -C "$SHUTTLE_SRC" init -q -b main && git -C "$SHUTTLE_SRC" -c user.name=f -c user.email=f@f commit -q --allow-empty -m base \
    && git -C "$SHUTTLE_SRC" branch chromium
}
