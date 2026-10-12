#!/usr/bin/env python3
"""Loop `gn gen out/Dev`; for each "Unresolved dependencies" pair, delete the dep line from the
needing target's BUILD.gn. Stops when gn gen passes or an error is not an unresolved dep.
Use after prune.py + fix-gn-refs.py. Review `git diff` afterwards."""
import re,subprocess,os,sys
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS  # noqa: E402
SRC=_SHUTTLE_SRC
env=dict(os.environ,PATH=os.environ['PATH']+':'+os.path.expanduser('~/depot_tools'))
for it in range(15):
    p=subprocess.run(['gn','gen','out/Dev'],cwd=SRC,env=env,capture_output=True,text=True)
    out=p.stdout+p.stderr
    if p.returncode==0: print('gn gen OK'); break
    pairs=re.findall(r'^//([^:(]*):[^(]+\([^)]*\)\n  needs //([^:(]+):([^(]+)\(',out,re.M)
    if not pairs: print(out[:3000]); break
    changed=0
    for tdir,ndir,nname in set(pairs):
        bf=os.path.join(SRC,tdir,'BUILD.gn')
        if not os.path.exists(bf): print('no',bf); continue
        forms={f'"//{ndir}:{nname}"'}
        if ndir.rsplit('/',1)[-1]==nname: forms.add(f'"//{ndir}"')
        if ndir==tdir: forms.add(f'":{nname}"')
        src=open(bf).read().split('\n'); new=[]
        for l in src:
            s=l.strip().rstrip(',')
            if s in forms: changed+=1; continue
            # single-line list: deps += [ "x" ]
            m=re.match(r'^(\s*\w+\s*[+-]?=\s*)\[\s*("[^"]+")\s*\]\s*$',l)
            if m and m.group(2) in forms: changed+=1; continue
            new.append(l)
        open(bf,'w').write('\n'.join(new))
    print('iter',it,'pairs',len(set(pairs)),'removed lines',changed)
    if not changed: print(out[:3000]); break
