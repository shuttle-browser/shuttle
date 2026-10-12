#!/usr/bin/env python3
"""Loop `gn gen <out>` (default BUILD_DIR = out/Sccache); while it fails with
`ERROR at //dir/BUILD.gn:L:C: Unable to load "<src>/X/BUILD.gn"` (a dep on a deleted directory)
and line L of that file is a single string entry, delete the line and retry. Then hands over
to gn-fix-unresolved.py-style handling (run it next). Stops on any other error and prints it.
Only touches lines GN actually evaluates, unlike fix-gn-refs.py (no churn in other-platform
branches). Review `git diff` afterwards."""
import os, re, subprocess, sys
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS, BUILD_DIR  # noqa: E402
SRC = _SHUTTLE_SRC
out = sys.argv[1] if len(sys.argv) > 1 else BUILD_DIR
env = dict(os.environ, PATH=os.environ['PATH'] + ':' + os.path.expanduser('~/depot_tools'))
pat = re.compile(r'^ERROR at //([^:]+):(\d+):\d+: (Unable to load|Can\'t load input file)', re.M)
single = re.compile(r'^\s*(\w+\s*[+-]?=\s*\[\s*)?"[^"]+"\s*,?\s*(\]\s*)?$')
for it in range(400):
    p = subprocess.run(['gn', 'gen', out], cwd=SRC, env=env, capture_output=True, text=True)
    o = p.stdout + p.stderr
    if p.returncode == 0:
        print('gn gen OK after', it, 'fixes'); break
    m = pat.search(o)
    if not m:
        print(o[:4000]); break
    f, ln = m.group(1), int(m.group(2))
    lines = open(os.path.join(SRC, f)).read().split('\n')
    l = lines[ln - 1]
    if not single.match(l):
        print(f'STOP {f}:{ln}: not a single entry: {l.strip()}\n' + o[:2500]); break
    if re.match(r'^\s*\w+\s*[+-]?=\s*\[', l) and not l.rstrip().endswith(']'):
        print(f'STOP {f}:{ln}: opens a list: {l.strip()}'); break
    print(f'del {f}:{ln}: {l.strip()}')
    del lines[ln - 1]
    open(os.path.join(SRC, f), 'w').write('\n'.join(lines))
