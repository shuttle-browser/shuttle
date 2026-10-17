#!/usr/bin/env python3
"""List source inputs on a target's ninja graph that don't exist (siso aborts on the first one before building).

After an upstream merge, upstream BUILD files can use files the fork pruned as unused ("UP" = the file exists on
branch `chromium`, restore it with `git checkout chromium -- <path>`).

Usage: scripts/check-ninja-inputs.py [out_dir] [target]   (defaults: BUILD_DIR = out/Sccache, chrome)
Run after `gn gen`, from anywhere. Limits: a small hand parser (whitespace-split paths, no `$`-escape or variable
resolution); it checks source inputs (`../../...`) only, not generated files. Exit: 0 = no missing inputs, 1 = missing inputs listed, 2 = can't check
(no build.ninja, target not in the graph, branch `chromium` missing).
"""
import os, re, sys, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shuttle_paths import SRC, BUILD_DIR  # noqa: E402


def die(msg):
    print(f"check-ninja-inputs: {msg}", file=sys.stderr)
    sys.exit(2)


if len(sys.argv) > 3:
    die("usage: check-ninja-inputs.py [out_dir] [target]")
out = os.path.join(SRC, sys.argv[1] if len(sys.argv) > 1 else BUILD_DIR)
start = sys.argv[2] if len(sys.argv) > 2 else 'chrome'
if not os.path.isfile(os.path.join(out, 'build.ninja')):
    die(f"no build.ninja in {out} (run gn gen first)")
os.chdir(out)
seen = set(); stack = ['build.ninja']; edges = {}
while stack:
    f = stack.pop()
    if f in seen:
        continue
    if not os.path.exists(f):
        die(f"ninja file {f} included from the graph is missing (stale or partial gn gen)")
    seen.add(f); t = open(f, errors='replace').read().replace('$\n', '')
    stack += re.findall(r'^(?:subninja|include) (\S+)', t, re.M)
    for m in re.finditer(r'^build (.+?)(?<!\$): (\S+)(.*)$', t, re.M):
        outs = m.group(1).split(); rest = m.group(3).split()
        ins = [x for x in rest if x not in ('|', '||', '|@')]
        for o in outs:
            if o == '|':
                continue
            edges.setdefault(o, []).extend(ins)
if start not in edges:
    die(f"target {start!r} not in the ninja graph of {out}")
todo = [start]; vis = set(); miss = set()
while todo:
    n = todo.pop()
    if n in vis:
        continue
    vis.add(n)
    if n in edges:
        todo += edges[n]
    elif n.startswith('../../'):
        p = n[6:]
        if not os.path.exists(os.path.join(SRC, p)):
            miss.add(p)
r = subprocess.run(['git', '-C', SRC, 'ls-tree', '-r', '--name-only', 'chromium'], capture_output=True, text=True)
if r.returncode:
    die(f"git ls-tree chromium failed: {r.stderr.strip()[:200]}")
up = set(r.stdout.split())
print('nodes', len(vis), 'missing', len(miss))
for p in sorted(miss):
    print(('UP   ' if p in up else 'NOUP ') + p)
sys.exit(1 if miss else 0)
