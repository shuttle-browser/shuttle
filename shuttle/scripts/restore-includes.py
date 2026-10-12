#!/usr/bin/env python3
"""Restore files from the base tree plus, transitively, every quoted #include they need that
exists in the base but not in the working tree. For "file not found" build errors where the
header is included unconditionally (often only used under another platform's #if).
Usage: restore-includes.py [--base REF] path ...   (paths repo-relative, cwd-independent)"""
import os, re, subprocess, sys
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS  # noqa: E402
SRC = _SHUTTLE_SRC
args = sys.argv[1:]; base = 'chromium'
if args[:1] == ['--base']: base = args[1]; args = args[2:]
run = lambda *a: subprocess.run(a, cwd=SRC, capture_output=True, text=True)
inc = re.compile(r'^\s*#\s*(?:include|import)\s+"([^"]+)"', re.M)
todo, done = list(args), set()
while todo:
    p = todo.pop()
    if p in done: continue
    done.add(p)
    if not os.path.exists(os.path.join(SRC, p)):
        r = run('git', 'checkout', base, '--', p)
        if r.returncode: print('not in base:', p); continue
        print('restored', p)
    for i in inc.findall(open(os.path.join(SRC, p), errors='ignore').read()):
        if not os.path.exists(os.path.join(SRC, i)): todo.append(i)
