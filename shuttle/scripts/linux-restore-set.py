#!/usr/bin/env python3
"""Compute which files to restore from the agent-chrome base for the Linux build.

Inputs (all paths repo-relative, one per line):
  --missing          files in the base tree but not in the fork (git diff --diff-filter=D base HEAD)
  --feature-deleted  files deleted by the feature batches (A, B1, sweeps, B2a, B2c, tests)
  --linux-read       files a base-tree Linux `chrome` build reads (from linux-read-set on the
                     Linux host: GN sources/inputs of targets reachable from //chrome:chrome,
                     .gn/.gni files, grd-referenced files)
  --manifests        prune manifests of the feature batches (their `dir`/`remove` rules)

Output: restore list on stdout, a report on stderr.

A missing file is restored when the Linux build reads it, unless it belongs to a removed
feature: it lies under a feature manifest's dir/remove rule, or its directory is gone at HEAD
and had files deleted by a feature batch, or a sibling with the same stem (minus a platform
suffix such as _linux, _x11, _ozone, _aura, _posix, _gtk) was deleted by a feature batch.
"""
import argparse, os, re, sys

PLAT_SUFFIX = re.compile(
    r'_(linux|x11|ozone|wayland|aura|aura_linux|desktop_linux|posix|nix|gtk|dbus|chromeos|'
    r'win|mac|android|views|desktop)(?=[._])')


def read(p):
    with open(p) as f:
        return [l.strip() for l in f if l.strip() and not l.startswith('#')]


def stem(path):
    d, b = os.path.split(path)
    b = re.sub(r'\.(cc|h|mm|ts|html|css|js|mojom|gn|grdp|json)$', '', b)
    b = re.sub(r'_(unittest|browsertest|interactive_uitest|test)$', '', b)
    return d, PLAT_SUFFIX.sub('', b)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--missing', required=True)
    ap.add_argument('--feature-deleted', required=True)
    ap.add_argument('--linux-read', required=True)
    ap.add_argument('--head-files', required=True, help='git ls-files at HEAD')
    ap.add_argument('--manifests', nargs='*', default=[])
    a = ap.parse_args()

    missing = set(read(a.missing))
    feat = set(read(a.feature_deleted))
    linux = set(read(a.linux_read))
    head_dirs = {os.path.dirname(p) for p in read(a.head_files)}

    rules = []
    for m in a.manifests:
        for l in read(m):
            parts = l.split(None, 1)
            if len(parts) == 2 and parts[0] in ('dir', 'remove'):
                rules.append(parts[1].rstrip('/'))
    rules.sort()

    feat_dirs = {os.path.dirname(p) for p in feat}
    feat_stems = {stem(p) for p in feat}

    def under_rule(p):
        return any(p == r or p.startswith(r + '/') for r in rules)

    cand = sorted(missing & linux)
    restore, why = [], {'rule': 0, 'gone_dir': 0, 'stem': 0}
    for p in cand:
        d = os.path.dirname(p)
        if under_rule(p):
            why['rule'] += 1
            continue
        if d not in head_dirs and d in feat_dirs:
            why['gone_dir'] += 1
            continue
        if stem(p) in feat_stems:
            why['stem'] += 1
            continue
        restore.append(p)

    print('\n'.join(restore))
    print(f'missing={len(missing)} linux-read={len(linux)} candidates={len(cand)} '
          f'restore={len(restore)} excluded={why}', file=sys.stderr)


if __name__ == '__main__':
    main()
