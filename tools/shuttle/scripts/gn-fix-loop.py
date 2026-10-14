#!/usr/bin/env python3
"""Loop `gn gen <out>` and strip references to removed targets, one error kind at a time.

Handles (deleting only single-string list entries, only lines GN actually evaluates):
  - `Unable to load "<src>/X/BUILD.gn"` at //F:L   -> delete line L of F (a dep on a deleted directory)
  - `Unresolved dependencies` pairs                -> delete the dep from the needing target's BUILD.gn
  - `Label not in deps` (allow_circular_includes_from) for a target that no longer exists
                                                   -> delete that label line in F
Stops on success, on any other error (printed), or when an iteration changes nothing.
Successor of gn-fix-loads.py + gn-fix-unresolved.py (used after the 155 merge). Review `git diff` afterwards.

Usage: scripts/gn-fix-loop.py [out_dir] [--max N]   (default out_dir: BUILD_DIR = out/Sccache)
"""
import os
import re
import subprocess
import sys

from shuttle_paths import SRC, BUILD_DIR  # noqa: E402

ENV = dict(os.environ, PATH=os.environ["PATH"] + ":" + os.path.expanduser("~/depot_tools"))
ENTRY = re.compile(r'^\s*("[^"]+")\s*,?\s*(#.*)?$')
ONELINE = re.compile(r'^(\s*\w+\s*[+-]?=\s*)\[\s*("[^"]+")\s*,?\s*\]\s*$')


def target_exists(label):
    d, _, name = label[2:].partition(":")
    name = name or d.rsplit("/", 1)[-1]
    bf = os.path.join(SRC, d, "BUILD.gn")
    if not os.path.exists(bf):
        return False
    return re.search(r'\(\s*"%s"\s*\)' % re.escape(name), open(bf, errors="replace").read()) is not None


def delete_lines(path, pred):
    lines = open(path).read().split("\n")
    keep = [l for l in lines if not pred(l)]
    if len(keep) != len(lines):
        open(path, "w").write("\n".join(keep))
    return len(lines) - len(keep)


def is_entry(l, forms):
    m = ENTRY.match(l) or ONELINE.match(l)
    return bool(m) and m.group(m.lastindex if m.re is ENTRY else 2) in forms if m else False


def entry_of(l):
    m = ENTRY.match(l)
    if m:
        return m.group(1)
    m = ONELINE.match(l)
    return m.group(2) if m else None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    out = args[0] if args else BUILD_DIR
    mx = int(sys.argv[sys.argv.index("--max") + 1]) if "--max" in sys.argv else 300
    total = 0
    for it in range(mx):
        p = subprocess.run(["gn", "gen", out], cwd=SRC, env=ENV, capture_output=True, text=True)
        text = p.stdout + p.stderr
        if p.returncode == 0:
            print(f"gn gen OK after {it} iterations, {total} lines removed")
            return 0
        changed = 0
        m = re.search(r'ERROR at //([^:]+):(\d+):\d+: Unable to load "[^"]+/BUILD\.gn"', text)
        if m:
            f, ln = os.path.join(SRC, m.group(1)), int(m.group(2))
            lines = open(f).read().split("\n")
            if entry_of(lines[ln - 1]):
                print(f"load   {m.group(1)}:{ln} {lines[ln - 1].strip()}")
                del lines[ln - 1]
                open(f, "w").write("\n".join(lines))
                changed = 1
        elif "Unresolved dependencies" in text:
            pairs = set(re.findall(r'^//([^:(]*):[^(]+\([^)]*\)\n  needs //([^:(]+):([^(]+)\(', text, re.M))
            for tdir, ndir, nname in pairs:
                bf = os.path.join(SRC, tdir, "BUILD.gn")
                forms = {f'"//{ndir}:{nname}"'}
                if ndir.rsplit("/", 1)[-1] == nname:
                    forms.add(f'"//{ndir}"')
                if ndir == tdir:
                    forms.add(f'":{nname}"')
                n = delete_lines(bf, lambda l: entry_of(l) in forms)
                if n:
                    print(f"unres  {tdir}/BUILD.gn -= //{ndir}:{nname}")
                changed += n
        else:
            m = re.search(r'ERROR at //([^:]+):\d+:\d+: Label not in deps\.[\s\S]*?The label "(//[^"]+)"', text)
            if m and not target_exists(m.group(2).split("(")[0]):
                lab = m.group(2).split("(")[0]
                forms = {f'"{lab}"'}
                d, _, name = lab[2:].partition(":")
                if name and d.rsplit("/", 1)[-1] == name:
                    forms.add(f'"//{d}"')
                changed = delete_lines(os.path.join(SRC, m.group(1)), lambda l: entry_of(l) in forms)
                if changed:
                    print(f"circ   {m.group(1)} -= {lab}")
        if not changed:
            print(f"--- stopped after {it} iterations, {total} lines removed; gn says:")
            print(text[:4000])
            return 1
        total += changed
    print(f"--- max iterations reached, {total} lines removed")
    return 1


if __name__ == "__main__":
    sys.exit(main())
