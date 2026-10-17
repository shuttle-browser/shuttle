#!/usr/bin/env bash
# gn-fix-loop.py: delete only entries of targets the fork pruned, only inside the reported target; dry run writes nothing.
. "$(dirname "$0")/lib.sh"; fake_src
cd "$SHUTTLE_SRC" || exit 1
mkdir -p gone a && printf 'group("x") {\n}\n' > gone/BUILD.gn && git add -A && git -c user.name=f -c user.email=f@f commit -q -m chromium-base
git branch -f chromium && git rm -q -r gone
cat > a/BUILD.gn <<'GN'
group("t") {
  deps = [
    "//gone:x",
    "//typo:y",
  ]
}
group("u") {
  deps = [ "//gone:x" ]
}
GN
cp a/BUILD.gn before.gn
TC='(//build/toolchain/mac:clang_arm64)'
# fake gn: reports t's first remaining unresolved dep
stub gn "grep -q '^    \"//gone:x\",' a/BUILD.gn && { printf 'ERROR Unresolved dependencies.\n//a:t$TC\n  needs //gone:x$TC\n'; exit 1; }
grep -q '\"//typo:y\"' a/BUILD.gn && { printf 'ERROR Unresolved dependencies.\n//a:t$TC\n  needs //typo:y$TC\n'; exit 1; }
exit 0"
expect 0 python3 "$SCRIPTS/gn-fix-loop.py" out/T;  expect_out "would unres  //a:t -= //gone:x"
cmp -s a/BUILD.gn before.gn || { echo "dry run modified a/BUILD.gn"; failures=$((failures + 1)); }
expect 1 python3 "$SCRIPTS/gn-fix-loop.py" out/T --apply; expect_out "which the fork didn't remove"
want=$(grep -v '^    "//gone:x",$' before.gn)
[[ $(cat a/BUILD.gn) == "$want" ]] || { echo "unexpected edit:"; diff before.gn a/BUILD.gn; failures=$((failures + 1)); }
# circular-include repair stays inside the reported target; duplicate target definitions are refused
cat > a/BUILD.gn <<'GN'
group("t") {
  allow_circular_includes_from = [
    "//gone:x",  # "{" in a comment
  ]
}
group("u") {
  allow_circular_includes_from = [
    "//gone:x",
  ]
}
GN
cp a/BUILD.gn before.gn
stub gn "grep -q 'gone:x\",  #' a/BUILD.gn && { printf 'ERROR at //a/BUILD.gn:3:5: Label not in deps.\nThe label \"//gone:x$TC\"\n'; exit 1; }; exit 0"
expect 0 python3 "$SCRIPTS/gn-fix-loop.py" out/T --apply
want=$(sed '3d' before.gn)
[[ $(cat a/BUILD.gn) == "$want" ]] || { echo "circular fix touched more than target t:"; diff before.gn a/BUILD.gn; failures=$((failures + 1)); }
printf 'group("t") {\n  deps = [\n    "//gone:x",\n  ]\n}\nif (is_win) {\n  group("t") {\n  }\n}\n' > a/BUILD.gn
cp a/BUILD.gn before.gn
stub gn "grep -q '//gone:x' a/BUILD.gn && { printf 'ERROR Unresolved dependencies.\n//a:t$TC\n  needs //gone:x$TC\n'; exit 1; }; exit 0"
expect 1 python3 "$SCRIPTS/gn-fix-loop.py" out/T --apply; expect_out "edit by hand"
cmp -s a/BUILD.gn before.gn || { echo "ambiguous target edited"; failures=$((failures + 1)); }
git branch -D -q chromium
expect 2 python3 "$SCRIPTS/gn-fix-loop.py" out/T --apply                  # no base branch
done_fixture
