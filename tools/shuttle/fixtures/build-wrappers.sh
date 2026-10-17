#!/usr/bin/env bash
# build.sh / build-sccache.sh / build-linux.sh: caps, lock, disk check, args, preflight, quoting, exit status.
. "$(dirname "$0")/lib.sh"; fake_src
export SHUTTLE_ROOT="$FIXTURE_TMP"; L="$FIXTURE_TMP/calls"; out="$SHUTTLE_SRC/out"
stub gn 'echo "gn $*" >> '"$L"
stub caffeinate 'shift; exec "$@"'
stub script 'if [[ $1 == -q && $2 == -F ]]; then shift 3; exec "$@"; fi; [[ $4 == -c ]] && exec bash -c "$5"; exit 99'
stub autoninja 'printf "autoninja"; printf " [%s]" "$@"; echo; printf "autoninja" >> '"$L"'; printf " [%s]" "$@" >> '"$L"'; echo >> '"$L"'; sleep ${AUTONINJA_SLEEP:-0}; exit ${AUTONINJA_RC:-0}'
B="$SCRIPTS/build.sh"
expect 0 zsh "$B" dev;  expect_out "[-local_jobs] [4] [chrome]"; expect_out "under nice -n 19"
grep -q "^gn gen out/Dev" "$L" || { echo "gn gen not run for new args"; failures=$((failures + 1)); }
expect 0 zsh "$B" dev -local_jobs=6 -k 0; expect_out "[-local_jobs=6] [-k] [0] [chrome]"
[[ $OUT != *"[4]"* ]] || { echo "default -local_jobs added on top of an explicit one"; failures=$((failures + 1)); }
SHUTTLE_LOCAL_JOBS=3 expect 0 zsh "$B" dev; expect_out "[-local_jobs] [3]"
cp "$out/Dev/args.gn" args.before; : > "$L"
expect 2 zsh "$B" dev -local_jobs abc
expect 2 zsh "$B" dev -local_jobs 2 -local_jobs=3
SHUTTLE_MIN_FREE_GB=999999 expect 2 zsh "$B" dev
AUTONINJA_RC=7 expect 7 zsh "$B" dev
mkdir "$out/.Dev.build-lock" && echo $$ > "$out/.Dev.build-lock/pid"        # a live build owns out/Dev
expect 2 zsh "$B" dev; expect_out "another build"
rm -rf "$out/.Dev.build-lock"; mkdir "$out/.Dev.build-lock" && echo 999999 > "$out/.Dev.build-lock/pid"  # dead owner
expect 2 zsh "$B" dev; expect_out "stale lock"                                # never taken over automatically
rm -rf "$out/.Dev.build-lock"; mkdir "$out/.Dev.build-lock"                    # ownerless (pid not written yet)
expect 2 zsh "$B" dev
[[ -d "$out/.Dev.build-lock" ]] || { echo "someone else's lock was removed"; failures=$((failures + 1)); }
rm -rf "$out/.Dev.build-lock"
expect 0 zsh "$B" dev
[[ ! -e "$out/.Dev.build-lock" ]] || { echo "lock not released"; failures=$((failures + 1)); }
grep -q autoninja "$L" && [[ $(grep -c autoninja "$L") == 2 ]] || { echo "autoninja ran for a refused build:"; cat "$L"; failures=$((failures + 1)); }
cmp -s args.before "$out/Dev/args.gn" || { echo "args.gn changed by a refused build"; failures=$((failures + 1)); }
echo 'x = 1' > "$out/Dev/args.gn"
SHUTTLE_KEEP_ARGS=1 expect 2 zsh "$B" dev
[[ $(cat "$out/Dev/args.gn") == 'x = 1' ]] || { echo "SHUTTLE_KEEP_ARGS rewrote args.gn"; failures=$((failures + 1)); }
# TERM while the build runs: the lock stays until the build child is gone, then the wrapper exits non-zero
AUTONINJA_SLEEP=3 zsh "$B" dev > term.out 2>&1 & bpid=$!
sleep 1; kill -TERM $bpid; sleep 0.5
[[ -d "$out/.Dev.build-lock" ]] || { echo "lock released while the build child still runs"; failures=$((failures + 1)); }
wait $bpid; trc=$?
[[ $trc != 0 ]] || { echo "TERM'd build exited 0"; failures=$((failures + 1)); }
[[ ! -e "$out/.Dev.build-lock" ]] || { echo "lock left after TERM"; failures=$((failures + 1)); rm -rf "$out/.Dev.build-lock"; }
# receipt eligibility: only -local_jobs / -k builds are "plain" (a dry run must never certify a binary)
. "$SCRIPTS/lib/build-common.sh"
for args in "-n" "-t commands" "-d explain" "-local_jobs 3 -n"; do
  # shellcheck disable=SC2086
  shuttle_build_args 4 $args; [[ $SHUTTLE_BUILD_PLAIN == 0 ]] || { echo "'$args' counted as a plain build"; failures=$((failures + 1)); }
done
shuttle_build_args 4 -local_jobs=6 -k 0; [[ $SHUTTLE_BUILD_PLAIN == 1 ]] || { echo "plain build not eligible"; failures=$((failures + 1)); }
# build-sccache.sh: no build servers -> refuse before building; never stops the shared server
stub pgrep 'exit 0'
stub sccache 'echo "sccache $*" >> '"$L"'; [[ $1 == --dist-status ]] && echo "${DIST:-{\"NotConnected\":[null,\"x\"]}}"; exit 0'
: > "$L"
expect 3 zsh "$SCRIPTS/build-sccache.sh"; expect_out "no build servers"
grep -q autoninja "$L" && { echo "build ran without servers"; failures=$((failures + 1)); }
SHUTTLE_ALLOW_LOCAL=1 expect 0 zsh "$SCRIPTS/build-sccache.sh"; expect_out "[-local_jobs] [8]"
DIST='{"SchedulerStatus":["http://h:10600/",{"num_servers":2,"num_cpus":24,"in_progress":0}]}' expect 0 zsh "$SCRIPTS/build-sccache.sh" -local_jobs 12
expect_out "[-local_jobs] [12] [chrome]"
grep -q -- "--stop-server\|--start-server" "$L" && { echo "touched the shared sccache server"; failures=$((failures + 1)); }
SHUTTLE_SCCACHE_FRESH=1 expect 2 zsh "$SCRIPTS/build-sccache.sh"
# build-linux.sh: args with spaces survive script -c; child status propagates
expect 0 bash "$SCRIPTS/build-linux.sh" dev "-o" "a b"; expect_out "[a b] [-local_jobs] [10] [chrome]"
AUTONINJA_RC=5 expect 5 bash "$SCRIPTS/build-linux.sh" dev
done_fixture
