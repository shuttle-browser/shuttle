#!/usr/bin/env bash
# smoke-all.sh with a stub browser (exits early / writes DevToolsActivePort and serves nothing / hangs): non-zero,
# no temp dir left, no child left. Probes: usage -> 2, closed port and a silent endpoint -> 1 within the deadline.
. "$(dirname "$0")/lib.sh"
export TMPDIR="$FIXTURE_TMP/t" SHUTTLE_SMOKE_READY_S=2; mkdir -p "$TMPDIR"
B="$FIXTURE_TMP/browser"
cat > "$B" <<'EOF'
#!/usr/bin/env bash
for a; do [[ $a == --user-data-dir=* ]] && p=${a#*=}; done; mkdir -p "$p"; echo $$ > "$STUB_PIDS/$$"
case $STUB in exit) exit 1 ;; port) echo 1 > "$p/DevToolsActivePort"; exec sleep 7357 ;; hang) exec sleep 7357 ;; esac
EOF
chmod +x "$B"
R="$SCRIPTS/smoke-all.sh"; export STUB_PIDS="$FIXTURE_TMP/pids"; mkdir -p "$STUB_PIDS"
expect 2 bash "$R"
expect 2 bash "$R" "$FIXTURE_TMP/nonexistent"
expect 2 bash "$R" "$B" --mode bogus
expect 2 bash "$R" "$B" --with nope
for s in exit port hang; do
  STUB=$s expect 1 bash "$R" "$B"; expect_out "launch"
  [[ -z $(ls -A "$TMPDIR") ]] || { echo "$s: temp dir left: $(ls "$TMPDIR")"; failures=$((failures + 1)); rm -rf "${TMPDIR:?}"/*; }
  for f in "$STUB_PIDS"/*; do [[ -f $f ]] || continue; sp=$(cat "$f"); rm -f "$f"
    ! kill -0 "$sp" 2>/dev/null || { echo "$s: stub browser $sp still running"; failures=$((failures + 1)); kill -9 "$sp"; }
  done
done
P="$SCRIPTS/smoke-cdp.mjs"
expect 2 node "$P"
SHUTTLE_SMOKE_DEADLINE_MS=5000 expect 1 node "$P" 1
# a listener that accepts and never answers: the probe deadline must end it
node -e 'const s = require("http").createServer(() => {}).listen(0, "127.0.0.1", () => console.log(s.address().port))' > port.txt &
srv=$!; for _ in 1 2 3 4 5 6 7 8 9 10; do [[ -s port.txt ]] && break; sleep 0.2; done
start=$SECONDS; SHUTTLE_SMOKE_DEADLINE_MS=2000 expect 1 node "$SCRIPTS/smoke-mcp.mjs" "$(cat port.txt)" --phase1
(( SECONDS - start < 15 )) || { echo "probe ignored its deadline"; failures=$((failures + 1)); }
kill "$srv"
# focus measurement: empty or unparsable lsappinfo output is "not measurable", never a pass
out=$(node --input-type=module -e "process.argv[2] = '1'; const { parseFront } = await import('$SCRIPTS/lib/smoke.mjs');
  console.log([parseFront(''), parseFront('  \\n'), parseFront('garbage'), parseFront('\"LSDisplayName\"=\"Preview\"')].join(','))")
[[ $out == '?,?,?,Preview' ]] || { echo "parseFront: $out"; failures=$((failures + 1)); }
# X11 (xprop): no active window = 'none' (Shuttle not in front), garbage = '?' (not measurable), class list parsed
out=$(node --input-type=module -e "process.argv[2] = '1'; const { parseX11Active: a, parseX11Class: c } = await import('$SCRIPTS/lib/smoke.mjs');
  console.log([a('_NET_ACTIVE_WINDOW(WINDOW): window id # 0x0'), a('_NET_ACTIVE_WINDOW(WINDOW): window id # 0x1a00003'), a(''),
    c('WM_CLASS(STRING) = \"shuttle\", \"Shuttle\"'), c('WM_CLASS:  not found.')].join('|'))")
[[ $out == 'none|0x1a00003|?|shuttle,Shuttle|?' ]] || { echo "parseX11: $out"; failures=$((failures + 1)); }
done_fixture
