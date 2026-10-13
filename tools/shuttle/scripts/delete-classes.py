#!/usr/bin/env python3
"""delete-classes.py <header> <cc> Class... : remove class decls from header and all out-of-line defs from cc."""
import re,sys
h,cc,classes=sys.argv[1],sys.argv[2],sys.argv[3:]
cre=re.compile(r'\b(%s)::'%'|'.join(map(re.escape,classes)))
def strip_header(s,c):
    m=re.search(r'(?m)^class %s\b[^;]*?\{'%re.escape(c),s)
    if not m: print('h: not found',c); return s
    i=m.start()
    while True:
        prev=s.rfind('\n',0,i-1)
        line=s[prev+1:i-1] if prev>=0 else ''
        if line.startswith('//'): i=prev+1
        else: break
    depth=0; j=m.end()-1
    while True:
        if s[j]=='{': depth+=1
        elif s[j]=='}':
            depth-=1
            if depth==0: break
        j+=1
    k=s.index(';',j)+1
    while k<len(s) and s[k]=='\n': k+=1
    return s[:i]+s[k:]
lines=open(cc).read().split('\n'); out=[]; i=0; n=0
skip_prefix=(' ','\t','}','#','/','namespace','using','static_assert')
while i<len(lines):
    l=lines[i]
    if l and not l.startswith(skip_prefix):
        j=i; sig=l
        while '{' not in lines[j] and not lines[j].rstrip().endswith(';') and j+1<len(lines):
            j+=1; sig+='\n'+lines[j]
        if cre.search(sig.split('(')[0] if '(' in sig else sig) or cre.search(sig.split('{')[0]):
            if lines[j].rstrip().endswith(';') and '{' not in lines[j]:
                k=j
            elif lines[j].rstrip().endswith('{}') or lines[j].rstrip().endswith('}'):
                k=j
            else:
                k=j+1
                while not lines[k].startswith('}'): k+=1
            while out and out[-1].startswith('//'): out.pop()
            i=k+1; n+=1
            while i<len(lines) and lines[i]=='' and (not out or out[-1]==''): i+=1
            continue
    if re.match(r'^// -*\s*(%s)\s*-*$'%'|'.join(map(re.escape,classes)),l): i+=1; continue
    out.append(l); i+=1
print('cc defs removed:',n)
hs=open(h).read()
for c in classes: hs=strip_header(hs,c)
open(h,'w').write(hs); open(cc,'w').write('\n'.join(out))
