#!/usr/bin/env bash
# check-scripts.sh on a host without zsh (Linux build host): zsh scripts are listed unverified, not FAIL (it used to
# fail every zsh file with "command not found"), lib/*.sh still get bash -n, and the summary says "not a pass".
. "$(dirname "$0")/lib.sh"
SHUTTLE_CHECK_ZSH="$FIXTURE_TMP/no-such-zsh" expect 0 bash "$SCRIPTS/check-scripts.sh" --syntax-only
expect_out "unverified (no zsh): scripts/build.sh"
expect_out "not a pass"
[[ $OUT != *"FAIL"* ]] || { echo "FAIL lines without zsh:"; echo "$OUT" | grep FAIL; failures=$((failures + 1)); }
done_fixture
