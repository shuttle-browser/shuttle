#!/usr/bin/env python3
"""Report <message>s whose enclosing <if expr> stack differs from the base tree (branch
`chromium`). Run after restore-strings.py; expect no output."""
import re, subprocess, sys
import os, sys
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
files=subprocess.run(['git','ls-files','*.grd','*.grdp'],cwd=SRC,capture_output=True,text=True).stdout.split()
for f in files:
    if 'test' in f: continue
    cur=open(f'{SRC}/{f}',errors='ignore').read()
    base=subprocess.run(['git','show',f'chromium:{f}'],cwd=SRC,capture_output=True,text=True).stdout
    if not base: continue
    cs, bs = stacks(cur), stacks(base)
    for k,v in cs.items():
        if k in bs and bs[k]!=v:
            print(f'{f}\t{k}\tcur={list(v)}\tbase={list(bs[k])}')
