#!/usr/bin/env bash
# prune-locales.py: an edit that breaks a grd is refused before anything is written or deleted (dry run too).
. "$(dirname "$0")/lib.sh"; fake_src
cd "$SHUTTLE_SRC" || exit 1
mkdir -p r
cat > r/ok.grd <<'G'
<grit><outputs>
  <output filename="ok_en-US.pak" type="data_package" lang="en-US" />
  <output filename="ok_de.pak" type="data_package" lang="de" />
</outputs></grit>
G
# a nested one-line language <if>: the regex stops at the first </if>, so the stripped text no longer parses
cat > r/bad.grd <<'G'
<grit><translations>
  <file path="t/x_de.xtb" lang="de" />
  <if expr="'de' == lang"><if expr="x"><part file="a.grdp" /></if></if>
</translations></grit>
G
echo '<x/>' > r/x_de.xtb; git add -A && git -c user.name=f -c user.email=f@f commit -q -m grd
cp r/ok.grd ok.before; cp r/bad.grd bad.before
expect 1 python3 "$SCRIPTS/prune-locales.py" --src "$SHUTTLE_SRC"; expect_out "nothing written"
expect 1 python3 "$SCRIPTS/prune-locales.py" --src "$SHUTTLE_SRC" --apply
cmp -s r/ok.grd ok.before && cmp -s r/bad.grd bad.before || { echo "grd written despite the refusal"; failures=$((failures + 1)); }
git ls-files --error-unmatch r/x_de.xtb >/dev/null 2>&1 || { echo "xtb deleted despite the refusal"; failures=$((failures + 1)); }
git checkout -q -- . && git rm -q r/bad.grd && git -c user.name=f -c user.email=f@f commit -q -m drop
expect 0 python3 "$SCRIPTS/prune-locales.py" --src "$SHUTTLE_SRC" --apply
grep -q 'lang="de"' r/ok.grd && { echo "valid edit not applied"; failures=$((failures + 1)); }
done_fixture
