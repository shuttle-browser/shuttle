#!/usr/bin/env python3
"""Strip dangling references to removed code from BUILD.gn / .gni files.

Usage: scripts/fix-gn-refs.py [--apply] [--base REF] [scope ...]   (scope: dirs, default: whole tree)

A string in a GN file is "dead" if it names a path (label dir or source file,
absolute `//...` or relative to the file) under a directory that has had files
deleted (staged deletions vs HEAD), and either:
  - the path no longer exists, or
  - it is a label whose BUILD.gn no longer defines the target.
Lines consisting of exactly one dead string entry (`"x",`) are deleted with
--apply. Everything else that mentions a dead path (imports, multi-entry lines,
$root_gen_dir paths) is reported for manual fixing.
"""
import argparse, functools, os, re, subprocess, sys
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS  # noqa: E402

SRC = _SHUTTLE_SRC

def git(*a):
    return subprocess.run(["git", *a], cwd=SRC, capture_output=True, text=True, check=True).stdout

# --base REF (or FIX_GN_BASE): also count files deleted between REF and HEAD (committed
# deletions), e.g. `--base chromium` on the Linux host after restoring platform files.
_base = os.environ.get("FIX_GN_BASE")
if "--base" in sys.argv:
    i = sys.argv.index("--base"); _base = sys.argv[i + 1]; del sys.argv[i:i + 2]
deleted = set(git("diff", "--cached", "--name-only", "--diff-filter=D", "HEAD").splitlines())
deleted |= set(git("diff", "--name-only", "--diff-filter=D").splitlines())
if _base:
    deleted |= set(git("diff", "--name-only", "--diff-filter=D", "--no-renames", _base, "HEAD").splitlines())
deleted_dirs = set()
for f in deleted:
    parts = f.split("/")
    for i in range(1, len(parts)):
        deleted_dirs.add("/".join(parts[:i]))

# Dirs kept only for their buildflags target; every other label in them is dead.
FLAGS_ONLY = {"components/lens", "components/compose"}

@functools.lru_cache(maxsize=None)
def gone(path):
    """path, or an ancestor, was deleted by this change and no longer exists."""
    parts = [x for x in path.split("/") if x]
    for i in range(len(parts), 0, -1):
        p = "/".join(parts[:i])
        if (p in deleted or p in deleted_dirs) and not os.path.exists(os.path.join(SRC, p)):
            return True
    return False

def is_dead(ref, gn_file):
    if (ref.startswith("/") and not ref.startswith("//")) or ref.startswith("$") or "{" in ref or "*" in ref or " " in ref:
        return False
    if ref.startswith("//"):
        path = ref[2:]
    elif ref.startswith(("./", "../", ":")) or "/" in ref or re.match(r"^[\w.-]+:[\w.-]+$", ref) or re.search(r"\.[a-z]{1,5}$", ref):
        path = os.path.normpath(os.path.join(os.path.dirname(gn_file), ref.split(":")[0])) + \
            (":" + ref.split(":", 1)[1] if ":" in ref else "")
    else:
        return False
    label = None
    if ":" in path:
        path, label = path.split(":", 1)
    path = path.rstrip("/")
    if not path or path.startswith(".."):
        return False
    if gone(path):
        return True
    if path in FLAGS_ONLY and (ref.startswith("//") or label):
        return (label or os.path.basename(path)) != "buildflags"
    if path in deleted_dirs and (ref.startswith("//") or label) and \
            os.path.isdir(os.path.join(SRC, path)) and not os.path.exists(os.path.join(SRC, path, "BUILD.gn")):
        return True
    return False

STR = re.compile(r'"([^"\n]*)"')
ONLY = re.compile(r'^\s*"([^"\n]*)"\s*,?\s*(#.*)?$')
# `x += [ "dead" ]` / `x = [ "dead" ]` on one line, or `[ "dead" ]` continuing `x +=`
ASSIGN1 = re.compile(r'^(\s*)([\w.]+)\s*(\+?=)\s*\[\s*"([^"\n]*)"\s*,?\s*\]\s*$')
LIST1 = re.compile(r'^\s*\[\s*"([^"\n]*)"\s*,?\s*\]\s*$')
ASSIGN_OPEN = re.compile(r'^(\s*)([\w.]+)\s*(\+?=)\s*$')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("scope", nargs="*")
    a = ap.parse_args()
    files = [f for f in git("ls-files", "--", *(a.scope or ["."])).splitlines()
             if f.endswith(("BUILD.gn", ".gni")) and not f.startswith("shuttle/") and f not in deleted and os.path.exists(os.path.join(SRC, f))]
    removed = manual = 0
    for f in files:
        lines = open(os.path.join(SRC, f)).read().split("\n")
        out, changed = [], False
        for n, line in enumerate(lines, 1):
            m = ONLY.match(line)
            m1 = ASSIGN1.match(line)
            m2 = LIST1.match(line)
            if m and is_dead(m.group(1), f):
                removed += 1
                changed = True
                if a.apply:
                    continue
                print(f"del {f}:{n}: {line.strip()}")
            elif m1 and is_dead(m1.group(4), f):
                removed += 1
                changed = True
                print(f"del {f}:{n}: {line.strip()}")
                if a.apply:
                    if m1.group(3) == "=":  # keep the variable defined
                        out.append(f"{m1.group(1)}{m1.group(2)} = []")
                    continue
            elif m2 and is_dead(m2.group(1), f) and out and ASSIGN_OPEN.match(out[-1]):
                removed += 1
                changed = True
                print(f"del {f}:{n-1}-{n}: {out[-1].strip()} {line.strip()}")
                if a.apply:
                    prev = ASSIGN_OPEN.match(out.pop())
                    if prev.group(3) == "=":
                        out.append(f"{prev.group(1)}{prev.group(2)} = []")
                    continue
            else:
                dead = [s for s in STR.findall(line) if is_dead(s, f)]
                if dead:
                    manual += 1
                    print(f"FIX {f}:{n}: {line.strip()}")
            out.append(line)
        if a.apply and changed:
            open(os.path.join(SRC, f), "w").write("\n".join(out))
    print(f"\n{removed} dead single-entry lines {'removed' if a.apply else 'found'}, {manual} lines need manual fixes", file=sys.stderr)

if __name__ == "__main__":
    main()
