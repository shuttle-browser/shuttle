#!/usr/bin/env python3
# add-include.py <file> <header-path>: insert #include "<header>" in sorted position among same-top-dir includes
import sys
p,h=sys.argv[1],sys.argv[2]
inc=f'#include "{h}"'
L=open(p).read().split('\n')
if any(l.endswith(f'"{h}"') for l in L if l.startswith('#')): sys.exit(print("already",p))
top=h.split('/')[0]+'/'
idx=[i for i,l in enumerate(L) if l.startswith(('#include "','#import "')) and l.split('"')[1].startswith(top) and i>5]
if not idx: idx=[i for i,l in enumerate(L) if l.startswith(('#include "','#import "')) and i>5]
pos=next((i for i in idx if L[i].split('"')[1]>h), idx[-1]+1)
L.insert(pos,inc); open(p,'w').write('\n'.join(L)); print("added",h,"->",p,":",pos+1)
