#!/usr/bin/env bash
# build.sh / build-linux.sh: caps, lock, disk check, dev out dir, staleness, args (SHUTTLE_GN_EXTRA, SHUTTLE_OUT_DIR), quoting, exit status.
# requires: zsh
. "$(dirname "$0")/lib.sh"; fake_src
export SHUTTLE_ROOT="$FIXTURE_TMP"; L="$FIXTURE_TMP/calls"; out="$SHUTTLE_SRC/out"
# the disk check is tested on its own (SHUTTLE_MIN_FREE_GB=999999 below); a small temp fs (WSL /tmp tmpfs) must not
# trip it in every other case
export SHUTTLE_MIN_FREE_GB=0
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
expect 2 zsh "$B" sccache; expect_out "use release|dev"                      # only the two local kinds
# SHUTTLE_GN_EXTRA: appended last to args.gn; a missing file refuses before touching anything
printf 'use_thin_lto = true\n' > extra.gn
SHUTTLE_GN_EXTRA="$PWD/extra.gn" expect 0 zsh "$B" dev
[[ $(tail -1 "$out/Dev/args.gn") == 'use_thin_lto = true' ]] || { echo "SHUTTLE_GN_EXTRA not appended"; failures=$((failures + 1)); }
SHUTTLE_GN_EXTRA="$PWD/missing.gn" expect 2 zsh "$B" dev; expect_out "no such file"
SHUTTLE_OUT_DIR=LinuxExtra SHUTTLE_GN_EXTRA="$PWD/extra.gn" expect 0 bash "$SCRIPTS/build-linux.sh" dev
[[ $(tail -1 "$out/LinuxExtra/args.gn") == 'use_thin_lto = true' ]] || { echo "SHUTTLE_GN_EXTRA not appended by build-linux.sh"; failures=$((failures + 1)); }
: > "$L"; SHUTTLE_OUT_DIR=LinuxMissing SHUTTLE_GN_EXTRA="$PWD/missing.gn" expect 2 bash "$SCRIPTS/build-linux.sh" dev; expect_out "no such file"
[[ ! -e "$out/LinuxMissing" ]] && ! grep -q autoninja "$L" || { echo "build-linux.sh touched out/ for a missing SHUTTLE_GN_EXTRA"; failures=$((failures + 1)); }
# SHUTTLE_OUT_DIR: other out dir name, same args
SHUTTLE_OUT_DIR=Other expect 0 zsh "$B" dev
[[ -f "$out/Other/args.gn" ]] || { echo "SHUTTLE_OUT_DIR ignored by build.sh"; failures=$((failures + 1)); }
SHUTTLE_OUT_DIR=LinuxOther expect 0 bash "$SCRIPTS/build-linux.sh" dev
[[ -f "$out/LinuxOther/args.gn" ]] || { echo "SHUTTLE_OUT_DIR ignored by build-linux.sh"; failures=$((failures + 1)); }
# build-linux.sh: args with spaces survive script -c; child status propagates
expect 0 bash "$SCRIPTS/build-linux.sh" dev "-o" "a b"; expect_out "[a b] [-local_jobs] [10] [chrome]"
AUTONINJA_RC=5 expect 5 bash "$SCRIPTS/build-linux.sh" dev
# dev out dir: SHUTTLE_BUILD_DIR naming another dev build refuses a plain `build.sh dev` (would build out/Dev)
: > "$L"; SHUTTLE_BUILD_DIR=out/Sccache expect 2 zsh "$B" dev; expect_out "SHUTTLE_BUILD_DIR=out/Sccache"
! grep -q autoninja "$L" || { echo "autoninja ran despite the SHUTTLE_BUILD_DIR mismatch"; failures=$((failures + 1)); }
SHUTTLE_BUILD_DIR=out/Sccache expect 2 bash "$SCRIPTS/build-linux.sh" dev
SHUTTLE_BUILD_DIR=out/Sccache SHUTTLE_OUT_DIR=Dev expect 0 zsh "$B" dev
SHUTTLE_BUILD_DIR=out/Dev expect 0 zsh "$B" dev
# staleness: last green on another Chromium version, or too many changed files, refuses unless SHUTTLE_ALLOW_FULL=1
mkdir -p "$SHUTTLE_SRC/chrome"; printf 'MAJOR=156\nMINOR=0\nBUILD=8078\nPATCH=48\n' > "$SHUTTLE_SRC/chrome/VERSION"
head=$(git -C "$SHUTTLE_SRC" rev-parse HEAD)
printf 'source_sha=%s\nchromium_version=155.0.8059.87\n' "$head" > "$out/Dev/shuttle-last-green.txt"
: > "$L"; expect 2 zsh "$B" dev; expect_out "last green on Chromium 155.0.8059.87"
! grep -q autoninja "$L" || { echo "autoninja ran for a stale out dir"; failures=$((failures + 1)); }
expect 2 bash "$SCRIPTS/build-linux.sh" dev; expect_out "SHUTTLE_ALLOW_FULL"
SHUTTLE_ALLOW_FULL=1 expect 0 zsh "$B" dev
SHUTTLE_CLEAN=1 expect 0 zsh "$B" dev
printf 'source_sha=%s\nchromium_version=156.0.8078.48\n' "$head" > "$out/Dev/shuttle-last-green.txt"
expect 0 zsh "$B" dev
touch "$SHUTTLE_SRC/f1" "$SHUTTLE_SRC/f2" "$SHUTTLE_SRC/f3"
git -C "$SHUTTLE_SRC" add f1 f2 f3 && git -C "$SHUTTLE_SRC" -c user.name=f -c user.email=f@f commit -q -m three
SHUTTLE_STALE_FILES=2 expect 2 zsh "$B" dev; expect_out "3 files changed since"
SHUTTLE_STALE_FILES=3 expect 0 zsh "$B" dev
rm "$out/Dev/shuttle-last-green.txt"
printf 'source_sha=%s\nchromium_version=155.0.8059.87\n' "$head" > "$out/Dev/shuttle-build-receipt.txt"  # fallback
expect 2 zsh "$B" dev; expect_out "last green on Chromium 155"
done_fixture
