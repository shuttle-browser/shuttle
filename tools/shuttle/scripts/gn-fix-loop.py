#!/usr/bin/env python3
"""Loop `gn gen <out>` and strip references to targets the fork pruned, one error at a time.

Policy (Toby, 2026-10-17): "removed by the fork" = present on branch `chromium`, gone from the working tree. That
can't tell a deliberate removal from an accidentally lost file/target, so run the dry run first and review every
listed edit (and `git diff`) before committing. Handles (only single-string list entries; anything else is
reported and the loop stops):
  - `Unable to load "<src>/X/BUILD.gn"` / `Can't load input file` at //F:L -> delete line L of F
  - `Unresolved dependencies` //T:t needs //N:n    -> delete //N:n entries inside target t's block in T/BUILD.gn
  - `Label not in deps` (allow_circular_includes_from) for a removed target -> delete that label inside the target
    whose line GN reported
Without --apply: one pass, prints the planned edit, writes nothing. With --apply: loops until gn gen passes, an error
it doesn't handle, or a refused edit; prints the files it touched (review `git diff`).

Usage: scripts/gn-fix-loop.py [out_dir] [--apply] [--max N]   (default out_dir: BUILD_DIR = out/Dev)
Exit: 0 = gn gen OK (or dry-run edit planned), 1 = stopped (error printed), 2 = usage/setup error.
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shuttle_paths import SRC, BUILD_DIR  # noqa: E402

ENV = dict(os.environ, PATH=os.environ["PATH"] + ":" + os.path.expanduser("~/depot_tools"))
ENTRY = re.compile(r'^\s*("[^"]+")\s*,?\s*(#.*)?$')
ONELINE = re.compile(r'^(\s*\w+\s*[+-]?=\s*)\[\s*("[^"]+")\s*,?\s*\]\s*$')


def git_ok(*a):
    return subprocess.run(["git", *a], cwd=SRC, capture_output=True).returncode == 0


def defines(text, name):
    return re.search(r'\(\s*"%s"\s*\)' % re.escape(name), text) is not None


def split_label(label):
    d, _, name = label.split("(")[0][2:].partition(":")
    return d, name or d.rsplit("/", 1)[-1]


def target_exists(label):
    d, name = split_label(label)
    bf = os.path.join(SRC, d, "BUILD.gn")
    return os.path.exists(bf) and defines(open(bf, errors="replace").read(), name)


def pruned_path(rel):
    """A file the fork removed: on branch `chromium`, not in the working tree."""
    return not os.path.exists(os.path.join(SRC, rel)) and git_ok("cat-file", "-e", f"chromium:{rel}")


def pruned_label(label):
    """A target the fork removed: defined on `chromium`, not defined now."""
    d, name = split_label(label)
    if target_exists(label):
        return False
    r = subprocess.run(["git", "show", f"chromium:{d}/BUILD.gn"], cwd=SRC, capture_output=True, text=True)
    return r.returncode == 0 and defines(r.stdout, name)


def entry_of(line):
    m = ENTRY.match(line)
    if m:
        return m.group(1)
    m = ONELINE.match(line)
    return m.group(2) if m else None


def label_forms(label, here=None):
    d, name = split_label(label)
    forms = {f'"//{d}:{name}"'}
    if d.rsplit("/", 1)[-1] == name:
        forms.add(f'"//{d}"')
    if here == d:
        forms.add(f'":{name}"')
    return forms


def braces(line):
    """Brace balance of a GN line, ignoring strings and comments."""
    code = re.sub(r'"(?:[^"\\]|\\.)*"', '""', line).split("#", 1)[0]
    return code.count("{") - code.count("}")


def span(lines, i):
    depth = 0
    for j in range(i, len(lines)):
        depth += braces(lines[j])
        if depth <= 0:
            return i, j
    return None


def block(lines, name):
    """(start, end) of the target `<kind>("name") {...}`; None if absent (template-generated name) or defined more
    than once (conditional duplicates: ambiguous, edit by hand)."""
    head = re.compile(r'^\s*[\w.]+\(\s*"%s"\s*\)\s*\{' % re.escape(name))
    hits = [i for i, l in enumerate(lines) if head.match(l)]
    return span(lines, hits[0]) if len(hits) == 1 else None


def enclosing(lines, ln):
    """(start, end) of the innermost target block `<kind>("...") {` that contains line index ln, or None."""
    head = re.compile(r'^\s*[\w.]+\(\s*"[^"]+"\s*\)\s*\{')
    for i in range(ln, -1, -1):
        if head.match(lines[i]):
            b = span(lines, i)
            if b and b[0] <= ln <= b[1]:
                return b
    return None


def write(path, lines):
    tmp = path + ".gnfix.tmp"
    with open(tmp, "w") as f:
        f.write("\n".join(lines))
    os.replace(tmp, path)


def plan(text):
    """-> (edits = [(file, [line indexes], description)], refusal message or None)."""
    m = re.search(r'ERROR at //([^:]+):(\d+):\d+: (Unable to load "([^"]+)"|Can\'t load input file)', text)
    if m:
        rel, ln = m.group(1), int(m.group(2))
        lines = open(os.path.join(SRC, rel)).read().split("\n")
        if not entry_of(lines[ln - 1]):
            return [], f"{rel}:{ln} is not a single list entry: {lines[ln - 1].strip()}"
        missing = m.group(4)
        if missing is None:  # Can't load input file: the path is in the entry
            e = entry_of(lines[ln - 1]).strip('"')
            missing = e[2:] if e.startswith("//") else os.path.normpath(os.path.join(os.path.dirname(rel), e))
        missing = os.path.relpath(missing, SRC) if os.path.isabs(missing) else missing
        if not pruned_path(missing):
            return [], f"{rel}:{ln}: {missing} was not removed by the fork (not on `chromium`, or still present)"
        return [(rel, [ln - 1], f"load   {rel}:{ln} {lines[ln - 1].strip()}")], None
    if "Unresolved dependencies" in text:
        edits = []
        for tdir, tname, ndir, nname in sorted(set(re.findall(
                r'^//([^:(]*):([^(]+)\([^)]*\)\n  needs //([^:(]+):([^(]+)\(', text, re.M))):
            dep = f"//{ndir}:{nname}"
            if not pruned_label(dep):
                return [], f"//{tdir}:{tname} needs {dep}, which the fork didn't remove (typo, DEPS, new upstream target?)"
            rel = os.path.join(tdir, "BUILD.gn")
            lines = open(os.path.join(SRC, rel)).read().split("\n")
            b = block(lines, tname)
            if b is None:
                return [], f"target {tname!r} not found literally in {rel} (template-generated?): edit by hand"
            forms = label_forms(dep, tdir)
            idx = [i for i in range(b[0], b[1] + 1) if entry_of(lines[i]) in forms]
            if not idx:
                return [], f"{dep} not a single-entry line inside //{tdir}:{tname}: edit by hand"
            edits.append((rel, idx, f"unres  //{tdir}:{tname} -= {dep}"))
        return edits, None if edits else "Unresolved dependencies, but no pair parsed"
    m = re.search(r'ERROR at //([^:]+):(\d+):\d+: Label not in deps\.[\s\S]*?The label "(//[^"]+)"', text)
    if m:
        rel, ln, lab = m.group(1), int(m.group(2)), m.group(3).split("(")[0]
        if not pruned_label(lab):
            return [], f"{rel}:{ln}: {lab} wasn't removed by the fork"
        lines = open(os.path.join(SRC, rel)).read().split("\n")
        b = enclosing(lines, ln - 1)
        if b is None:
            return [], f"{rel}:{ln} is not inside a literal target block: edit by hand"
        idx = [i for i in range(b[0], b[1] + 1) if entry_of(lines[i]) in label_forms(lab)]
        return ([(rel, idx, f"circ   {rel}:{b[0] + 1} -= {lab}")], None) if idx else ([], f"{lab} not a single entry in the target at {rel}:{b[0] + 1}")
    return [], "an error gn-fix-loop doesn't handle"


def main():
    argv = sys.argv[1:]
    apply = "--apply" in argv
    try:
        mx = int(argv[argv.index("--max") + 1]) if "--max" in argv else 300
    except (IndexError, ValueError):
        print("usage: gn-fix-loop.py [out_dir] [--apply] [--max N]", file=sys.stderr)
        return 2
    pos = [a for i, a in enumerate(argv) if not a.startswith("--") and (i == 0 or argv[i - 1] != "--max")]
    out = pos[0] if pos else BUILD_DIR
    if not git_ok("rev-parse", "-q", "--verify", "chromium^{commit}"):
        print("branch `chromium` (base tree) not found", file=sys.stderr)
        return 2
    touched, total = set(), 0
    for it in range(mx):
        p = subprocess.run(["gn", "gen", out], cwd=SRC, env=ENV, capture_output=True, text=True)
        text = p.stdout + p.stderr
        if p.returncode == 0:
            print(f"gn gen OK after {it} iterations, {total} lines removed")
            break
        edits, refusal = plan(text)
        if refusal:
            print(f"--- stopped after {it} iterations, {total} lines removed: {refusal}; gn says:")
            print(text[:4000])
            break
        for rel, idx, desc in edits:
            print(desc if apply else "would " + desc)
        if not apply:
            print("(dry run; pass --apply)")
            return 0
        per_file = {}  # all indexes refer to the file before this iteration's edits
        for rel, idx, _ in edits:
            per_file.setdefault(rel, set()).update(idx)
        for rel, idx in per_file.items():
            path = os.path.join(SRC, rel)
            lines = open(path).read().split("\n")
            write(path, [l for i, l in enumerate(lines) if i not in idx])
            touched.add(rel)
            total += len(idx)
    else:
        print(f"--- max iterations reached, {total} lines removed")
    if touched:
        print("touched (review git diff):\n  " + "\n  ".join(sorted(touched)))
    return 0 if p.returncode == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
