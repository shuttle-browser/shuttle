#!/usr/bin/env python3
"""Classify (and optionally auto-resolve) zdiff3 conflict hunks after an upstream merge.

Run `git checkout --conflict=zdiff3 -- <files>` first so hunks carry the merge base.

Hunk kinds (only the first two are ever auto-resolved; upstream lines that aren't in the base are never
accepted or dropped silently, those hunks stay conflicted for review):
  ours-deleted       ours is empty, upstream only deleted/reordered base lines -> keep the deletion.
  ours-subset        ours only removed base lines (a removal batch), upstream only deleted/reordered base lines,
                     and every line ours removed is unique in the base -> theirs minus the lines ours removed.
  ours-deleted-new   ours is empty, upstream added new lines -> review (`show`): new code in a region the fork
                     deleted (a security fix? a new caller?).
  ours-subset-new    ours removed lines, upstream added new lines (or a removed line is duplicated) -> review.
  theirs-deleted     upstream deleted a region the fork edited -> needs a look (fork additions?).
  other              both sides rewrote the region -> manual.
All files are parsed before any is written: an unterminated or non-zdiff3 hunk (no `|||||||` base) aborts with
exit 2 and leaves every file unchanged.

Usage:
  scripts/merge-autoresolve.py stats <file-list>            per-kind counts
  scripts/merge-autoresolve.py show <kind> <file-list>      print hunks of one kind
  scripts/merge-autoresolve.py apply <kinds,..> <file-list> resolve those kinds in place;
                                                            prints files with no conflicts left
  scripts/merge-autoresolve.py pick <ours|theirs> <file-list>  take one side in every hunk, any kind
                                                            (an explicit per-file decision; non-conflicting
                                                            upstream changes stay)
Exit: 0 = done, 2 = usage error or unparsable conflict markers (nothing written).
"""
import collections
import re
import sys

START, BASE, MID, END = "<<<<<<< ", "||||||| ", "=======", ">>>>>>> "


class ParseError(Exception):
    pass


def parse(text):
    """Return ('text', lines) and ('hunk', ours, base, theirs) segments; ParseError on malformed hunks."""
    lines = text.splitlines(keepends=True)
    out, buf, i = [], [], 0
    while i < len(lines):
        if lines[i].startswith(START):
            out.append(("text", buf))
            buf = []
            ours, base, theirs, cur = [], [], [], None
            cur = ours
            start, seen_base = i + 1, False
            i += 1
            while i < len(lines) and not lines[i].startswith(END):
                ln = lines[i]
                if ln.startswith(START):
                    raise ParseError(f"line {i + 1}: nested conflict start")
                if ln.startswith(BASE):
                    seen_base = True
                    cur = base
                elif ln.rstrip("\r\n") == MID and cur is base:
                    cur = theirs
                else:
                    cur.append(ln)
                i += 1
            if i >= len(lines):
                raise ParseError(f"line {start}: unterminated conflict hunk")
            if not seen_base or cur is not theirs:
                raise ParseError(f"line {start}: not a zdiff3 hunk (run git checkout --conflict=zdiff3)")
            out.append(("hunk", ours, base, theirs))
        else:
            buf.append(lines[i])
        i += 1
    out.append(("text", buf))
    return out


def is_subsequence(small, big):
    it = iter(big)
    return all(any(x == y for y in it) for x in small)


KINDS = ("ours-deleted", "ours-subset", "ours-deleted-new", "ours-subset-new", "theirs-deleted", "other")
AUTO = {"ours-deleted", "ours-subset"}


def novel(base, theirs):
    """Upstream lines not in the base (multiset): new code that must not be accepted or dropped silently."""
    return sum((collections.Counter(theirs) - collections.Counter(base)).values())


def kind(ours, base, theirs):
    if not ours and base:
        return "ours-deleted" if not novel(base, theirs) else "ours-deleted-new"
    if not theirs and base:
        return "theirs-deleted"
    if base and len(ours) < len(base) and is_subsequence(ours, base):
        bc = collections.Counter(base)
        removed = bc - collections.Counter(ours)
        if novel(base, theirs) or any(bc[ln] > 1 for ln in removed):
            return "ours-subset-new"
        return "ours-subset"
    return "other"


SIDE = None  # set by `pick`: "ours" or "theirs" takes that side for every hunk


def resolve(k, ours, base, theirs):
    if SIDE == "ours":
        return ours
    if SIDE == "theirs":
        return theirs
    if k not in AUTO:
        return None
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


def usage(msg):
    print(f"merge-autoresolve: {msg}\n" + __doc__.split("Usage:")[1].split("Exit:")[0], file=sys.stderr)
    sys.exit(2)


def main():
    global SIDE
    if len(sys.argv) < 3:
        usage("missing arguments")
    cmd, args = sys.argv[1], sys.argv[2:]
    if cmd not in ("stats", "show", "apply", "pick"):
        usage(f"unknown command {cmd!r}")
    if cmd == "pick":  # pick <ours|theirs> <file-list>: whole-hunk side for every hunk
        SIDE = args.pop(0)
        if SIDE not in ("ours", "theirs"):
            usage("pick takes ours or theirs")
        cmd = "apply"
        args.insert(0, ",".join(KINDS))
    want = set(args.pop(0).split(",")) if cmd in ("show", "apply") else set()
    if cmd == "apply" and SIDE is None and want - AUTO:
        usage(f"apply only resolves {','.join(sorted(AUTO))}; review the rest with show (got {','.join(sorted(want - AUTO))})")
    if cmd == "show" and want - set(KINDS):
        usage(f"unknown kind(s) {','.join(sorted(want - set(KINDS)))}")
    if len(args) != 1:
        usage("expected one file-list argument")
    files = [l.strip() for l in open(args[0]) if l.strip()]
    parsed = {}
    for f in files:
        try:
            parsed[f] = parse(open(f, encoding="utf-8", errors="surrogateescape").read())
        except ParseError as e:
            print(f"merge-autoresolve: {f}: {e}; nothing written", file=sys.stderr)
            sys.exit(2)
    counts, per_file, out = collections.Counter(), collections.Counter(), {}
    for f in files:
        segs = parsed[f]
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
        out[f] = "".join(new)
    if cmd == "apply":
        for f in files:
            open(f, "w", encoding="utf-8", errors="surrogateescape").write(out[f])
    if cmd == "stats":
        for k, n in counts.most_common():
            print(f"{n:6d} {k}")
    if cmd == "apply":
        for f in files:
            if per_file[f] == 0:
                print(f)


if __name__ == "__main__":
    main()
