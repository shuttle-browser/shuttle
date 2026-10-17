#!/usr/bin/env bash
# prune.py --apply: refuses used-set rules (Mac-derived "unused", F4) and a dirty tree, before touching anything.
. "$(dirname "$0")/lib.sh"; fake_src
cd "$SHUTTLE_SRC" || exit 1
mkdir -p old && echo x > old/a.cc && echo y > keep.cc && git add -A && git -c user.name=f -c user.email=f@f commit -q -m files
printf 'dir old\n' > used-set.txt; printf 'remove old\n' > remove-only.txt
expect 1 python3 "$SCRIPTS/prune.py" --apply "$PWD/used-set.txt"; expect_out "refusing --apply"
[[ -f old/a.cc ]] || { echo "old/a.cc deleted despite the refusal"; failures=$((failures + 1)); }
echo z >> keep.cc
expect 1 python3 "$SCRIPTS/prune.py" --apply "$PWD/remove-only.txt"; expect_out "uncommitted changes"
git checkout -q keep.cc
expect 0 python3 "$SCRIPTS/prune.py" --apply "$PWD/remove-only.txt"
[[ ! -e old/a.cc && -f keep.cc ]] || { echo "remove-only manifest didn't apply as expected"; failures=$((failures + 1)); }
done_fixture
