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
git branch -D -q chromium
expect 2 python3 "$SCRIPTS/gn-fix-loop.py" out/T --apply                  # no base branch
done_fixture
