#!/usr/bin/env bash
# merge-autoresolve.py: never accept or drop upstream-new lines silently; malformed hunks abort before any write.
. "$(dirname "$0")/lib.sh"
M="$SCRIPTS/merge-autoresolve.py"
hunk() { printf '<<<<<<< ours\n%b||||||| base\n%b=======\n%b>>>>>>> theirs\n' "$1" "$2" "$3"; }
# a: fork deleted, upstream only dropped a line            -> auto (deletion kept)
# b: fork deleted, upstream added a new line               -> stays conflicted (ours-deleted-new)
# c: fork removed y, upstream reordered                    -> auto: theirs minus y
# d: fork removed y, upstream added a new line z           -> stays conflicted (ours-subset-new; used to keep z)
{ echo top; hunk '' 'a1\na2\n' 'a1\n'; hunk '' 'b1\n' 'b1\nNEW\n'; hunk 'x\n' 'x\ny\n' 'y\nx\n'; hunk 'x\n' 'x\ny\n' 'x\ny\nz\n'; } > f.txt
echo f.txt > list
expect 0 python3 "$M" stats list
expect_out "ours-deleted-new"; expect_out "ours-subset-new"
expect 2 python3 "$M" apply ours-deleted,ours-subset-new list          # non-auto kinds refused
cp f.txt before.txt
expect 0 python3 "$M" apply ours-deleted,ours-subset list
want=$(printf 'top\n'; hunk '' 'b1\n' 'b1\nNEW\n'; printf 'x\n'; hunk 'x\n' 'x\ny\n' 'x\ny\nz\n')
[[ $(cat f.txt) == "$want" ]] || { echo "apply result differs:"; diff <(echo "$want") f.txt; failures=$((failures + 1)); }
# malformed: second file has a non-zdiff3 hunk -> exit 2, first file untouched
cp before.txt f.txt; printf '<<<<<<< ours\nq\n=======\nr\n>>>>>>> theirs\n' > g.txt; printf 'f.txt\ng.txt\n' > list2
expect 2 python3 "$M" apply ours-deleted,ours-subset list2
cmp -s f.txt before.txt || { echo "f.txt was modified despite the parse error"; failures=$((failures + 1)); }
printf '<<<<<<< ours\nq\n||||||| base\n' > g.txt                      # unterminated
expect 2 python3 "$M" stats list2
expect 2 python3 "$M" bogus list
done_fixture
