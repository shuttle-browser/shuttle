#!/usr/bin/env bash
# linux-nsw-check.sh: talks only to the port in its own profile's DevToolsActivePort; a foreign browser answering on
# 9222 (or anywhere) is never called; a launch that never becomes ready ends the run without driving anything.
. "$(dirname "$0")/lib.sh"; fake_src
mkdir -p "$SHUTTLE_SRC/out/Dev"; L="$FIXTURE_TMP/calls"; : > "$L"
cat > "$SHUTTLE_SRC/out/Dev/shuttle" <<EOS
#!/usr/bin/env bash
echo "shuttle \$*" >> "$L"
for a; do [[ \$a == --user-data-dir=* ]] && p=\${a#*=}; done
[[ \${STUB_PORT:-} ]] && echo "\$STUB_PORT" > "\$p/DevToolsActivePort"
echo \$\$ > "$FIXTURE_TMP/browser.pid"
[[ \${STUB_HANG:-} ]] && exec sleep 300
sleep 2
EOS
chmod +x "$SHUTTLE_SRC/out/Dev/shuttle"
stub curl 'echo "curl $*" >> '"$L"'; echo "{}"; exit 0'           # every port answers, incl. a foreign 9222
stub node 'echo "node $*" >> '"$L"'; exit 0'
stub timeout 'shift; exec "$@"'; stub ss 'exit 0'; stub stat 'echo "2026-10-17 00:00:00"'
# never ready and long-lived: the run ends, nothing is driven, and the browser process itself is gone afterwards
STUB_HANG=1 SHUTTLE_NSW_READY_S=2 expect 1 bash "$SCRIPTS/linux-nsw-check.sh" headless; expect_out "no CDP endpoint"
grep -q '^node' "$L" && { echo "drove a browser after a failed launch"; failures=$((failures + 1)); }
bp=$(cat "$FIXTURE_TMP/browser.pid" 2>/dev/null)
[[ -n $bp ]] || { echo "stub browser never started"; failures=$((failures + 1)); }
if [[ -n $bp ]] && kill -0 "$bp" 2>/dev/null; then echo "browser $bp left running"; failures=$((failures + 1)); kill -9 "$bp"; fi
: > "$L"
STUB_PORT=4242 expect 1 bash "$SCRIPTS/linux-nsw-check.sh" headless                     # fails later (stub MCP), fine
grep -q '4242' "$L" || { echo "never used its own port"; failures=$((failures + 1)); }
grep -E '^(curl|node)' "$L" | grep -v 4242 | grep -q . && { echo "called something other than its own port:"; grep -E '^(curl|node)' "$L" | grep -v 4242; failures=$((failures + 1)); }
done_fixture
