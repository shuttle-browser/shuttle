#!/usr/bin/env bash
# Mojo message-ID salt per major: release builds refuse when config/mojom-salt.txt doesn't name chrome/VERSION's
# MAJOR (a forgotten bump after a major merge would keep last major's IDs); args-release.gn points at the file.
. "$(dirname "$0")/lib.sh"; fake_src
. "$SCRIPTS/lib/build-common.sh"
C="$FIXTURE_TMP/cfg"; mkdir -p "$C" "$SHUTTLE_SRC/chrome"
printf 'MAJOR=156\nMINOR=0\nBUILD=8078\nPATCH=36\n' > "$SHUTTLE_SRC/chrome/VERSION"
printf '# c\nshuttle MAJOR=155\n' > "$C/mojom-salt.txt"
expect 2 shuttle_build_salt "$SHUTTLE_SRC" "$C"; expect_out "MAJOR=155, chrome/VERSION MAJOR=156"
printf '# c\nshuttle MAJOR=156\n' > "$C/mojom-salt.txt"
expect 0 shuttle_build_salt "$SHUTTLE_SRC" "$C"
rm "$C/mojom-salt.txt"; expect 2 shuttle_build_salt "$SHUTTLE_SRC" "$C"
T="$(dirname "$SCRIPTS")"
grep -q '^mojom_message_id_salt_path = "//tools/shuttle/config/mojom-salt.txt"$' "$T/config/args-release.gn" \
  || { echo "args-release.gn lacks the salt path"; failures=$((failures + 1)); }
grep -q '^shuttle MAJOR=[0-9][0-9]*$' "$T/config/mojom-salt.txt" || { echo "config/mojom-salt.txt malformed"; failures=$((failures + 1)); }
for s in build.sh build-linux.sh; do
  grep -q 'shuttle_build_salt "$SRC" "$cfg"' "$SCRIPTS/$s" || { echo "$s doesn't check the salt"; failures=$((failures + 1)); }
done
done_fixture
