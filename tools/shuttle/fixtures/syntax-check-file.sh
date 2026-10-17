#!/usr/bin/env bash
# syntax-check-file.sh: a compiler failure or an unknown source must exit non-zero (it used to print and exit 0).
# requires: zsh
. "$(dirname "$0")/lib.sh"; fake_src
export SHUTTLE_BUILD_DIR=out/T SHUTTLE_SYNCHECK_SLOTS=1
o="$SHUTTLE_SRC/$SHUTTLE_BUILD_DIR"; mkdir -p "$o"; touch "$o/build.ninja"
echo ok > "$SHUTTLE_SRC/good.cc"; echo BAD > "$SHUTTLE_SRC/bad.cc"
# fake compiler: "error:" + rc 1 when the source says BAD
stub fakecc 'for a; do [[ $a == *.cc ]] && f=$a; done; grep -q BAD "$f" && { echo "$f:1:1: error: boom"; exit 1; }; exit 0'
stub ninja "cat <<J
[{\"directory\": \"$o\", \"file\": \"../../good.cc\", \"command\": \"fakecc -MMD -MF x.d -c ../../good.cc -o obj/good.o\"},
 {\"directory\": \"$o\", \"file\": \"../../bad.cc\", \"command\": \"fakecc -MMD -MF y.d -c ../../bad.cc -o obj/bad.o\"}]
J"
expect 0 zsh "$SCRIPTS/syntax-check-file.sh" good.cc
expect 1 zsh "$SCRIPTS/syntax-check-file.sh" bad.cc; expect_out "error: boom"
expect 1 zsh "$SCRIPTS/syntax-check-file.sh" missing.cc; expect_out "no command"
expect 2 zsh "$SCRIPTS/syntax-check-file.sh"
SHUTTLE_BUILD_DIR=out/Nope expect 2 zsh "$SCRIPTS/syntax-check-file.sh" good.cc
done_fixture
