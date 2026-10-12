#!/usr/bin/env python3
"""Run on the Linux build host against a *base* (unpruned) checkout.
Usage: linux-read-set.py <out_dir> [root_target ...]  > linux-read.txt
Needs `gn gen --ide=json <out_dir>` first. Prints repo-relative paths the build of the root
targets (default //chrome:chrome) reads: sources/inputs/public/data of every reachable target,
all .gn/.gni files GN loaded, and files referenced by reachable .grd/.grdp (recursively)."""
import json, os, re, sys
out = sys.argv[1]; roots = sys.argv[2:] or ['//chrome:chrome']
src = os.path.abspath(os.path.join(out, '..', '..'))
p = json.load(open(os.path.join(out, 'project.json')))
T = p['targets']
def base(label): return label.split('(')[0]
idx = {}
for k in T: idx.setdefault(base(k), []).append(k)
seen, stack = set(), [k for r in roots for k in idx.get(r, [])]
while stack:
    k = stack.pop()
    if k in seen: continue
    seen.add(k)
    t = T[k]
    for d in t.get('deps', []) + t.get('public_deps', []) + t.get('data_deps', []):
        if d in T: stack.append(d)
files = set()
def add(x):
    if x.startswith('//') and not x.startswith('//out/'): files.add(x[2:])
for k in seen:
    t = T[k]
    for key in ('sources', 'inputs', 'public', 'data'):
        for x in t.get(key, []) or []:
            if isinstance(x, str): add(x)
    for x in (t.get('script'),):
        if x: add(x)
for x in p.get('build_settings', {}).get('gen_input_files', []): add(x)
ref = re.compile(r'(?:file|path)="([^"$]+)"')
todo = [f for f in files if f.endswith(('.grd', '.grdp'))]
while todo:
    g = todo.pop(); full = os.path.join(src, g)
    if not os.path.isfile(full): continue
    for m in ref.findall(open(full, errors='ignore').read()):
        r = os.path.normpath(os.path.join(os.path.dirname(g), m))
        if r.startswith('..'): continue
        if r not in files:
            files.add(r)
            if r.endswith(('.grd', '.grdp')): todo.append(r)
# expand directories referenced as data
final = set()
for f in files:
    full = os.path.join(src, f)
    if os.path.isdir(full):
        for dp, _, fs in os.walk(full):
            for x in fs: final.add(os.path.relpath(os.path.join(dp, x), src))
    else:
        final.add(f)
print('\n'.join(sorted(final)))
print(f'targets={len(seen)} files={len(final)}', file=sys.stderr)
