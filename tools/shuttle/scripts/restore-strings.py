#!/usr/bin/env python3
"""Restore <message> blocks deleted by prune-strings.py (e.g. needed by a newly built platform).
Usage (cwd-independent): restore-strings.py [--base REF] IDS_NAME ...
For each name, finds every variant of the message in the base tree's .grd/.grdp (e.g. the
use_titlecase / not use_titlecase pair) together with the <if expr> conditions enclosing it
there, and appends them, wrapped in exactly those conditions, just before the file's
</messages> (or </grit-part> for .grdp). Anchoring next to a neighbour message is NOT safe:
the neighbour may now sit in a different <if> block (that put Linux strings under
is_macosx once). Translations (.xtb) are keyed by message fingerprint and were not touched
by prune-strings, so they apply again. Check afterwards with scripts/grd-if-check.py (expects no output)."""
import os, re, subprocess, sys
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS  # noqa: E402
SRC = _SHUTTLE_SRC
args = sys.argv[1:]; base = 'chromium'
if args[:1] == ['--base']: base = args[1]; args = args[2:]
run = lambda *a: subprocess.run(a, cwd=SRC, capture_output=True, text=True).stdout
tok_re = re.compile(r'<if expr="([^"]*)">|</if>')

def if_stack(text, pos):
    stack = []
    for t in tok_re.finditer(text, 0, pos):
        if t.group(0) == '</if>': stack.pop()
        else: stack.append(t.group(1))
    return stack

for name in args:
    hits = run('git', 'grep', '-l', f'name="{name}"', base, '--', '*.grd', '*.grdp').split()
    if not hits: print('not in base:', name); continue
    path = hits[0].split(':', 1)[1]
    old = run('git', 'show', f'{base}:{path}')
    cur = open(os.path.join(SRC, path)).read()
    if f'name="{name}"' in cur: print('present:', name); continue
    one = re.compile(r'^[ \t]*<message\s[^>]*name="%s"[^>]*>.*?</message>' % name, re.M | re.S)
    blocks = []
    for m in one.finditer(old):
        b = m.group(0)
        for cond in reversed(if_stack(old, m.start())):
            b = f'      <if expr="{cond}">\n{b}\n      </if>'
        blocks.append(b)
    if not blocks: print('parse failed:', name, path); continue
    end = cur.rfind('</grit-part>') if path.endswith('.grdp') else cur.find('</messages>')
    if end < 0: print('no insertion point:', name, path); continue
    ls = cur.rfind('\n', 0, end) + 1
    cur = cur[:ls] + '\n'.join(blocks) + '\n' + cur[ls:]
    open(os.path.join(SRC, path), 'w').write(cur)
    print('restored', name, f'({len(blocks)} variant(s))', '->', path)
