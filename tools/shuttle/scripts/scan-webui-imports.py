#!/usr/bin/env python3
"""List WebUI TS/HTML/CSS files that import something deleted by the uncommitted change.

Usage: scripts/scan-webui-imports.py   (gn gen can't see these; run before building)
"""
import re,os,subprocess,collections,sys
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS  # noqa: E402
os.chdir(_SHUTTLE_SRC)
def git(*a): return subprocess.run(['git',*a],capture_output=True,text=True,check=True).stdout.split()
deleted=set(git('diff','--cached','--name-only','--diff-filter=D','HEAD'))|set(git('diff','--name-only','--diff-filter=D'))
deleted_dirs=set()
for f in deleted:
    p=f.split('/')
    for i in range(1,len(p)): deleted_dirs.add('/'.join(p[:i]))
def gone(path):
    p=path
    while p and p!='.':
        if (p in deleted or p in deleted_dirs) and not os.path.exists(p): return True
        p=os.path.dirname(p)
    return False
files=subprocess.run(['git','ls-files','--','chrome/browser/resources','ui/webui/resources','components/*/resources','chrome/browser/ui/webui'],capture_output=True,text=True).stdout.split()
files=[f for f in files if f.endswith(('.ts','.html','.css')) and '/test' not in f and os.path.exists(f)]
imp=re.compile(r'''(?:import|export)\s[^'"]*?['"]([^'"]+)['"]|import\(\s*['"]([^'"]+)['"]|@import\s+['"]([^'"]+)['"]|<link[^>]+href=['"]([^'"]+)['"]|src=['"]([^'"]+\.js)['"]''')
hits=collections.defaultdict(list)
for f in files:
    s=open(f,errors='replace').read()
    for m in imp.finditer(s):
        spec=next(x for x in m.groups() if x)
        if spec.startswith('chrome://resources/'): base='ui/webui/resources/'+spec[len('chrome://resources/'):]
        elif spec.startswith('//resources/'): base='ui/webui/resources/'+spec[len('//resources/'):]
        elif spec.startswith('.'): base=os.path.normpath(os.path.join(os.path.dirname(f),spec))
        else: continue
        stem=re.sub(r'\.(js|css)$','',base)
        stem=re.sub(r'\.mojom-webui$','',stem)
        if gone(stem) or gone(stem+'.ts') or gone(stem+'.mojom') or gone(os.path.dirname(stem)):
            hits[f].append(spec)
for f,v in sorted(hits.items()): print(f, len(v), v[0])
print(len(hits),'files',file=sys.stderr)
