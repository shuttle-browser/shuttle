#!/usr/bin/env python3
"""Delete IDS_* messages that no code uses from the fork's .grd/.grdp files.

Usage: scripts/prune-strings.py [--apply] [--scope DIR ...]

"Used" = the IDS_ name appears (as a whole word) in any tracked non-grit file
(.grd/.grdp/.xtb/.xlf excluded) in the user dirs (all of chrome components ui content extensions
services device pdf printing headless third_party/blink), or is reachable through token pasting
(`IDS_FOO_##x` -> every IDS_FOO_* stays). Messages are removed from grd(p) files under SCOPE
(default: chrome components ui content extensions services device). Empty `<if>` blocks are
removed; a .grdp left without messages is deleted along with its `<part>` line and its line in
`*.grd.gritdeps`. Translations (.xtb) are left alone; grit ignores unused ones.
Re-run until it reports 0 (removing a message never frees another, so one pass is normally enough).
"""
import argparse, os, re, subprocess, sys
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS  # noqa: E402

SRC = _SHUTTLE_SRC
USERS = ["chrome", "components", "ui", "content", "extensions", "services", "device", "pdf",
         "printing", "headless", "third_party/blink", "tools/metrics"]
SCOPE = ["chrome", "components", "ui", "content", "extensions", "services", "device"]
MSG = re.compile(r'(?:^[ \t]*<!--(?:(?!-->).)*-->[ \t]*\n)*^[ \t]*<message\b[^>]*\bname="(IDS_[A-Z0-9_]+)"'
                 r'.*?</message>[ \t]*\n', re.S | re.M)
EMPTY_IF = re.compile(r'^[ \t]*<if\b[^>]*>\s*</if>[ \t]*\n', re.M)


def git(*a):
    """git stdout; any failure is fatal (a failed grep would make every message look unused). grep rc 1 = no match."""
    r = subprocess.run(["git", *a], cwd=SRC, capture_output=True, text=True)
    if r.returncode != 0 and not (a[0] == "grep" and r.returncode == 1 and not r.stderr):
        sys.exit(f"git {' '.join(a[:3])} ... failed (rc {r.returncode}): {r.stderr.strip()[:300]}")
    return r.stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--scope", nargs="*", default=SCOPE)
    a = ap.parse_args()
    excl = [":!*.grd", ":!*.grdp", ":!*.xtb", ":!*.xlf", ":!shuttle"]  # shuttle/ = this tooling (docs name IDS_*)
    used = set(git("grep", "-h", "-o", "-w", "-E", "IDS_[A-Z0-9_]+", "--", *USERS, *excl).split())
    if not used:
        sys.exit("no IDS_ uses found: refusing (wrong SRC or grep scope?)")
    prefixes = set(re.findall(r"\b(IDS_[A-Z0-9_]*?)_?##", git("grep", "-h", "-E", "IDS_[A-Z0-9_]*##", "--", *USERS, *excl)))
    def is_used(n):
        return n in used or any(n.startswith(p) for p in prefixes)

    grds = [f for f in git("ls-files", "--", *[f"{d}/**.grd" for d in a.scope],
                           *[f"{d}/**.grdp" for d in a.scope]).split()]
    total, emptied, edits = 0, [], {}
    for g in grds:
        text = open(os.path.join(SRC, g), encoding="utf-8").read()
        if "<message" not in text:
            continue
        removed = [0]
        def sub(m):
            if is_used(m.group(1)):
                return m.group(0)
            removed[0] += 1
            return ""
        new = MSG.sub(sub, text)
        while True:
            n2 = EMPTY_IF.sub("", new)
            if n2 == new:
                break
            new = n2
        if removed[0]:
            total += removed[0]
            edits[g] = new
            if g.endswith(".grdp") and "<message" not in new:
                emptied.append(g)
    print(f"unused messages: {total} in {len(edits)} files; grdp files left empty: {len(emptied)}")
    for g in emptied:
        print("  empty:", g)
    if not a.apply:
        return
    for g, t in edits.items():
        if g not in emptied:
            open(os.path.join(SRC, g), "w", encoding="utf-8").write(t)
    # Delete emptied grdps: their <part> lines and gritdeps entries.
    for g in emptied:
        base = os.path.basename(g)
        for parent in git("grep", "-l", "-F", base, "--", "*.grd", "*.grdp", "*.gritdeps", "*.gn", "*.gni").split():
            p = os.path.join(SRC, parent)
            lines = open(p, encoding="utf-8").read().split("\n")
            pdir = os.path.dirname(parent)
            out = []
            for l in lines:
                m = re.search(r'<part file="([^"]+)"', l)
                if m and os.path.normpath(os.path.join(pdir, m.group(1))) == g:
                    continue
                if parent.endswith(".gritdeps") and l.strip() == g:  # repo-relative paths
                    continue
                out.append(l)
            if out != lines:
                open(p, "w", encoding="utf-8").write("\n".join(out))
        subprocess.run(["git", "rm", "-q", "-f", "--", g], cwd=SRC, check=True)
    # Second pass for <if> blocks emptied by removed <part>s.
    for parent in set(edits) - set(emptied):
        p = os.path.join(SRC, parent)
        t = open(p, encoding="utf-8").read()
        t2 = EMPTY_IF.sub("", t)
        if t2 != t:
            open(p, "w", encoding="utf-8").write(t2)


if __name__ == "__main__":
    main()
