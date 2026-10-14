#!/usr/bin/env python3
"""List source inputs on a target's ninja graph that don't exist (siso aborts on the first one before building).

After an upstream merge, upstream BUILD files can use files the fork pruned as unused ("UP" = the file exists on
branch `chromium`, restore it with `git checkout chromium -- <path>`).

Usage: scripts/check-ninja-inputs.py [out_dir] [target]   (defaults: BUILD_DIR = out/Sccache, chrome)
Run after `gn gen`, from anywhere.
"""
import os,re,sys,subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shuttle_paths import SRC, BUILD_DIR  # noqa: E402
os.chdir(os.path.join(SRC, sys.argv[1] if len(sys.argv)>1 else BUILD_DIR))
src=SRC
seen=set(); stack=['build.ninja']; edges={}
def unesc(s): return s.replace('$ ',' ').replace('$:',':')
while stack:
    f=stack.pop()
    if f in seen or not os.path.exists(f): continue
    seen.add(f); t=open(f,errors='replace').read().replace('$\n','')
    stack+=re.findall(r'^(?:subninja|include) (\S+)',t,re.M)
    for m in re.finditer(r'^build (.+?)(?<!\$): (\S+)(.*)$',t,re.M):
        outs=m.group(1).split(); rest=m.group(3).split()
        ins=[x for x in rest if x not in ('|','||','|@')]
        for o in outs:
            if o=='|': continue
            edges.setdefault(o,[]).extend(ins)
start=sys.argv[2] if len(sys.argv)>2 else 'chrome'
todo=[start]; vis=set(); miss=set()
while todo:
    n=todo.pop()
    if n in vis: continue
    vis.add(n)
    if n in edges: todo+=edges[n]
    elif n.startswith('../../'):
        p=n[6:]
        if not os.path.exists(os.path.join(src,p)): miss.add(p)
up=set(subprocess.run(['git','-C',src,'ls-tree','-r','--name-only','chromium'],capture_output=True,text=True).stdout.split())
print('nodes',len(vis),'missing',len(miss))
for p in sorted(miss): print(('UP   ' if p in up else 'NOUP ')+p)
