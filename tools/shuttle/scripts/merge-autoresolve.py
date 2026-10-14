#!/usr/bin/env python3
"""Classify (and optionally auto-resolve) zdiff3 conflict hunks after an upstream merge.

Run `git checkout --conflict=zdiff3 -- <files>` first so hunks carry the merge base.

Hunk kinds:
  ours-deleted   ours is empty: the fork deleted the region, upstream edited it -> keep the deletion.
  ours-deleted-big  same, but upstream added more than MAX_ADDED new lines -> review.
  ours-subset    ours only removed base lines (a removal batch), upstream edited the rest
                 -> theirs minus the lines ours removed. Upstream-added lines stay; a build
                 error points at any that reference removed code.
  theirs-deleted upstream deleted a region the fork edited -> needs a look (fork additions?).
  other          both sides rewrote the region -> manual.

Usage:
  scripts/merge-autoresolve.py stats <file-list>            per-kind counts
  scripts/merge-autoresolve.py show <kind> <file-list>      print hunks of one kind
  scripts/merge-autoresolve.py apply <kinds,..> <file-list> resolve those kinds in place;
                                                            prints files with no conflicts left
  scripts/merge-autoresolve.py pick <ours|theirs> <file-list>  take one side in every hunk
                                                            (non-conflicting upstream changes stay)
"""
import collections
import re
import sys

START, BASE, MID, END = "<<<<<<< ", "||||||| ", "=======", ">>>>>>> "


def parse(text):
    """Yield ('text', lines) and ('hunk', ours, base, theirs) segments."""
    lines = text.splitlines(keepends=True)
    out, buf, i = [], [], 0
    while i < len(lines):
        if lines[i].startswith(START):
            out.append(("text", buf))
            buf = []
            ours, base, theirs, cur = [], [], [], None
            cur = ours
            i += 1
            while not lines[i].startswith(END):
                ln = lines[i]
                if ln.startswith(BASE):
                    cur = base
                elif ln.rstrip("\r\n") == MID and cur is base:
                    cur = theirs
                else:
                    cur.append(ln)
                i += 1
            out.append(("hunk", ours, base, theirs))
        else:
            buf.append(lines[i])
        i += 1
    out.append(("text", buf))
    return out


def is_subsequence(small, big):
    it = iter(big)
    return all(any(x == y for y in it) for x in small)


MAX_ADDED = 5  # ours-deleted hunks where upstream added more lines than this go to review


def added(base, theirs):
    return sum((collections.Counter(theirs) - collections.Counter(base)).values())


def kind(ours, base, theirs):
    if not ours and base:
        return "ours-deleted" if added(base, theirs) <= MAX_ADDED else "ours-deleted-big"
    if not theirs and base:
        return "theirs-deleted"
    if base and len(ours) < len(base) and is_subsequence(ours, base):
        return "ours-subset"
    return "other"


SIDE = None  # set by `pick`: "ours" or "theirs" takes that side for every hunk


def resolve(k, ours, base, theirs):
    if SIDE == "ours":
        return ours
    if SIDE == "theirs":
        return theirs
    if k == "ours-deleted":
        return []
    if k == "ours-subset":
        removed = collections.Counter(base) - collections.Counter(ours)
        res = []
        for ln in theirs:
            if removed[ln] > 0:
                removed[ln] -= 1
            else:
                res.append(ln)
        return res
    return None


def main():
    global SIDE
    cmd, args = sys.argv[1], sys.argv[2:]
    if cmd == "pick":  # pick <ours|theirs> <file-list>: whole-hunk side for every hunk
        SIDE = args.pop(0)
        cmd = "apply"
        args.insert(0, "ours-deleted,ours-deleted-big,ours-subset,theirs-deleted,other")
    want = set(args.pop(0).split(",")) if cmd in ("show", "apply") else set()
    files = [l.strip() for l in open(args[0]) if l.strip()]
    counts, per_file = collections.Counter(), collections.Counter()
    for f in files:
        text = open(f, encoding="utf-8", errors="surrogateescape").read()
        segs = parse(text)
        new, left = [], 0
        for s in segs:
            if s[0] == "text":
                new.extend(s[1])
                continue
            _, ours, base, theirs = s
            k = kind(ours, base, theirs)
            counts[k] += 1
            if cmd == "show" and k in want:
                print(f"==== {f} [{k}]")
                print("--- ours\n" + "".join(ours) + "--- base\n" + "".join(base) + "--- theirs\n" + "".join(theirs))
            r = resolve(k, ours, base, theirs) if (cmd == "apply" and k in want) else None
            if r is None:
                left += 1
                new.append(START + "ours\n")
                new.extend(ours)
                new.append(BASE + "base\n")
                new.extend(base)
                new.append(MID + "\n")
                new.extend(theirs)
                new.append(END + "theirs\n")
            else:
                new.extend(r)
        per_file[f] = left
        if cmd == "apply":
            open(f, "w", encoding="utf-8", errors="surrogateescape").write("".join(new))
    if cmd == "stats":
        for k, n in counts.most_common():
            print(f"{n:6d} {k}")
    if cmd == "apply":
        for f in files:
            if per_file[f] == 0:
                print(f)


if __name__ == "__main__":
    main()
