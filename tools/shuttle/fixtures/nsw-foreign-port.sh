#!/usr/bin/env bash
# linux-nsw-check.sh: with something else answering on 9222 it must not launch, call MCP or close anything.
. "$(dirname "$0")/lib.sh"; fake_src
mkdir -p "$SHUTTLE_SRC/out/Dev"; L="$FIXTURE_TMP/calls"
printf '#!/usr/bin/env bash\necho shuttle "$@" >> %q\n' "$L" > "$SHUTTLE_SRC/out/Dev/shuttle"; chmod +x "$SHUTTLE_SRC/out/Dev/shuttle"
stub curl 'echo "curl $*" >> '"$L"'; echo "{}"; exit 0'      # a foreign browser answers on 9222
stub node 'echo "node $*" >> '"$L"'; exit 0'
stub stat 'echo "2026-10-17 00:00:00"'
expect 2 bash "$SCRIPTS/linux-nsw-check.sh" headless; expect_out "already answers"
! grep -q -e '^shuttle' -e '^node' "$L" || { echo "drove the foreign browser:"; cat "$L"; failures=$((failures + 1)); }
done_fixture
