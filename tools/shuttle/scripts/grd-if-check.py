#!/usr/bin/env python3
"""Report <message>s whose enclosing <if expr> stack differs from the base tree (branch
`chromium`). Run after restore-strings.py; expect no output. Exit 1 if anything differs, 2 on git errors."""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shuttle_paths import SRC  # noqa: E402
def stacks(text):
    out={}; stack=[]
    for m in re.finditer(r'<if expr="([^"]*)">|</if>|<message name="(IDS_[A-Z0-9_]+)"', text):
        g=m.group(0)
        if g=='</if>': stack.pop()
        elif g.startswith('<if'): stack.append(m.group(1))
        else: out.setdefault(m.group(2), tuple(stack))
    return out
if subprocess.run(['git','rev-parse','-q','--verify','chromium^{commit}'],cwd=SRC,capture_output=True).returncode:
    print('branch `chromium` (base tree) not found', file=sys.stderr); sys.exit(2)
r=subprocess.run(['git','ls-files','*.grd','*.grdp'],cwd=SRC,capture_output=True,text=True)
if r.returncode or not r.stdout.strip():
    print(f'git ls-files failed or found no grd files in {SRC}', file=sys.stderr); sys.exit(2)
files=r.stdout.split(); diffs=0
for f in files:
    if 'test' in f: continue
    cur=open(f'{SRC}/{f}',errors='ignore').read()
    base=subprocess.run(['git','show',f'chromium:{f}'],cwd=SRC,capture_output=True,text=True).stdout
    if not base: continue
    cs, bs = stacks(cur), stacks(base)
    for k,v in cs.items():
        if k in bs and bs[k]!=v:
            print(f'{f}\t{k}\tcur={list(v)}\tbase={list(bs[k])}'); diffs+=1
sys.exit(1 if diffs else 0)
