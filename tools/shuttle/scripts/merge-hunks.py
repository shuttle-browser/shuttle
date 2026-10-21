#!/usr/bin/env python3
"""Compact zdiff3 hunk viewer/resolver for upstream merges (first used for 156).

  merge-hunks.py show <file> [n]        numbered hunks: kind, ours/theirs as diffs against the merge base, 3 lines context
  merge-hunks.py take <file> <n> <ours|theirs|base|none>  replace hunk n with that side ("none" = empty)
  merge-hunks.py left <file>...         remaining hunk counts
Hunk numbers shift after a take: re-run show (or take from the highest number down).
For a custom result, edit the file directly (replace the whole <<<<<<< .. >>>>>>> block).
"""
import difflib
import importlib.util
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    "m", os.path.join(HERE, "merge-autoresolve.py"))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


def read(f):
    return open(f, encoding="utf-8", errors="surrogateescape").read()


def write(f, segs):
    out = []
    for s in segs:
        if s[0] == "text":
            out += s[1]
        else:
            ours, base, theirs = s[1], s[2], s[3]
            out.append("<<<<<<< HEAD\n")
            out += ours
            out.append("||||||| merged common ancestors\n")
            out += base
            out.append("=======\n")
            out += theirs
            out.append(">>>>>>> chromium\n")
    open(f, "w", encoding="utf-8", errors="surrogateescape").write("".join(out))


def diff(base, side):
    if not side and base:
        return [f"    (region deleted: all {len(base)} base lines removed)\n"]
    d = list(difflib.unified_diff(base, side, n=1, lineterm=""))[2:]
    res = []
    for ln in d:
        if not ln.endswith("\n"):
            ln += "\n"
        res.append("    " + ln)
    return res or ["    (same as base)\n"]


def show(f, only=None):
    segs = m.parse(read(f))
    n, lineno = 0, 1
    out = []
    for i, s in enumerate(segs):
        if s[0] == "text":
            lineno += len(s[1])
            continue
        n += 1
        if only is None or n == only:
            ours, base, theirs = s[1], s[2], s[3]
            k = m.kind(ours, base, theirs)
            before = segs[i - 1][1][-3:] if i > 0 else []
            after = segs[i + 1][1][:3] if i + 1 < len(segs) else []
            out.append(f"--- hunk {n} @ line {lineno} kind={k} base={len(base)} ours={len(ours)} theirs={len(theirs)}\n")
            out += ["  ctx| " + x for x in before]
            out.append("  OURS (fork) vs base:\n")
            out += diff(base, ours)
            out.append("  THEIRS (upstream 156) vs base:\n")
            out += diff(base, theirs)
            out += ["  ctx| " + x for x in after]
        lineno += len(s[1]) + len(s[2]) + len(s[3]) + 4
    sys.stdout.write("".join(out))


def take(f, n, side):
    segs = m.parse(read(f))
    idx = [i for i, s in enumerate(segs) if s[0] == "hunk"]
    if not 1 <= n <= len(idx):
        sys.exit(f"{f}: no hunk {n} (has {len(idx)})")
    s = segs[idx[n - 1]]
    lines = {"ours": s[1], "base": s[2], "theirs": s[3], "none": []}[side]
    segs[idx[n - 1]] = ("text", lines)
    write(f, segs)
    print(f"{f}: hunk {n} -> {side}; {len(idx) - 1} left")


def main(a):
    if len(a) >= 2 and a[0] == "show":
        show(a[1], int(a[2]) if len(a) > 2 else None)
    elif len(a) == 4 and a[0] == "take":
        take(a[1], int(a[2]), a[3])
    elif len(a) >= 2 and a[0] == "left":
        for f in a[1:]:
            print(sum(1 for s in m.parse(read(f)) if s[0] == "hunk"), f)
    else:
        sys.exit(__doc__)


main(sys.argv[1:])
