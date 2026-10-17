#!/usr/bin/env bash
# check-ninja-inputs.py: fail closed (missing graph/target = 2, missing input = 1, clean = 0).
. "$(dirname "$0")/lib.sh"; fake_src
o="$SHUTTLE_SRC/out/T"; mkdir -p "$o"; touch "$SHUTTLE_SRC/a.cc"
expect 2 python3 "$SCRIPTS/check-ninja-inputs.py" out/T                 # no build.ninja
printf 'subninja obj.ninja\n' > "$o/build.ninja"
printf 'build chrome: link obj/a.o\nbuild obj/a.o: cxx ../../a.cc\n' > "$o/obj.ninja"
expect 0 python3 "$SCRIPTS/check-ninja-inputs.py" out/T; expect_out "missing 0"
expect 2 python3 "$SCRIPTS/check-ninja-inputs.py" out/T nosuchtarget    # target not in graph
printf 'build chrome: link obj/a.o obj/b.o\nbuild obj/a.o: cxx ../../a.cc\nbuild obj/b.o: cxx ../../b.cc\n' > "$o/obj.ninja"
expect 1 python3 "$SCRIPTS/check-ninja-inputs.py" out/T; expect_out "NOUP b.cc"
rm "$o/obj.ninja"
expect 2 python3 "$SCRIPTS/check-ninja-inputs.py" out/T                 # subninja missing (partial gn gen)
done_fixture
