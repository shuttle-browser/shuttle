#!/usr/bin/env python3
"""Prune statistics: the fork (`main`) against its upstream snapshot (`chromium`).

Usage: scripts/prune-stats.py [--fork REV] [--upstream REV] [--depth N] [--out FILE]

Compares the two git trees (no checkout needed): files and bytes removed, added and changed, overall, per directory
(a tree for a treemap, down to --depth levels) and per kind (tests, platform code, product code, ...). Also counts the
removal groups in tools/shuttle/prune/*.txt (one comment-headed block = one feature or area).

Only what is in the fork's git tree counts. Dependencies that gclient fetches (DEPS) are in neither tree; the ones the
fork stopped fetching are listed by DEPS diff, not here. Bytes are blob sizes (source text, data and binaries).
"""

import argparse
import json
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from shuttle_paths import SRC  # noqa: E402

SHUTTLE_SRC = Path(SRC)

PLATFORM_DIRS = re.compile(
    r'(^|/)(android|ios|chromeos|ash|lacros|fuchsia|win|windows|cros)(/|_|$)')
TEST_PATH = re.compile(
    r'(^|/)(test|tests|testing|test_data|testdata|web_tests|perf_tests|'
    r'unittests?|browser_tests?|interactive_ui_tests|fuzzers?)(/|$)|'
    r'_(unit|browser|interactive_ui|api|perf|fuzz)?tests?\.|_fuzzer\.|'
    r'_test_util|_test_support|test_utils?\.|mock_|fake_')
DOC_PATH = re.compile(r'(^|/)docs?/|\.md$')


def tree(rev):
    """{path: (size, sha)} for the files (blobs) of `rev`."""
    out = subprocess.run(['git', '-C', str(SHUTTLE_SRC), 'ls-tree', '-r', '-l', '-z', rev],
                         check=True, capture_output=True).stdout
    files = {}
    for entry in out.split(b'\0'):
        if not entry:
            continue
        meta, path = entry.split(b'\t', 1)
        mode, kind, sha, size = meta.split()
        if kind != b'blob':
            continue  # gitlinks (submodule-style DEPS pins) have no size
        files[path.decode('utf-8', 'surrogateescape')] = (int(size), sha.decode())
    return files


def kind_of(path):
    if path.startswith('tools/shuttle/'):
        return 'shuttle tooling'
    if TEST_PATH.search(path):
        return 'tests and test data'
    if PLATFORM_DIRS.search(path):
        return 'other platforms (Android, iOS, ChromeOS, Windows, Fuchsia)'
    if DOC_PATH.search(path):
        return 'docs'
    return 'product code and resources'


def prune_groups(src):
    """Comment-headed blocks of `remove` lines in the prune manifests."""
    groups = []
    for manifest in sorted((src / 'tools/shuttle/prune').glob('*.txt')):
        title, has_remove = None, False
        for line in manifest.read_text().splitlines() + ['']:
            stripped = line.strip()
            if stripped.startswith('#'):
                if has_remove:
                    groups.append({'manifest': manifest.name, 'title': title})
                    has_remove = False
                    title = None
                if title is None and not stripped.startswith(('# Prune manifest', '# remove-only')):
                    title = stripped.lstrip('# ').strip()
            elif stripped and not stripped.startswith('#'):
                has_remove = True
            elif not stripped and has_remove:
                groups.append({'manifest': manifest.name, 'title': title})
                has_remove, title = False, None
    return [g for g in groups if g['title']]


def node():
    return {'upstream_files': 0, 'upstream_bytes': 0, 'removed_files': 0, 'removed_bytes': 0,
            'children': {}}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--fork', default='main')
    ap.add_argument('--upstream', default='chromium')
    ap.add_argument('--depth', type=int, default=3)
    ap.add_argument('--out')
    args = ap.parse_args()

    upstream, fork = tree(args.upstream), tree(args.fork)
    removed = {p: v for p, v in upstream.items() if p not in fork}
    added = {p: v for p, v in fork.items() if p not in upstream}
    changed = [p for p, v in fork.items() if p in upstream and upstream[p][1] != v[1]]

    root = node()
    for path, (size, _) in upstream.items():
        parts = path.split('/')[:-1][:args.depth]
        gone = path in removed
        cur = root
        for level in [None] + parts:
            if level is not None:
                cur = cur['children'].setdefault(level, node())
            cur['upstream_files'] += 1
            cur['upstream_bytes'] += size
            if gone:
                cur['removed_files'] += 1
                cur['removed_bytes'] += size

    def finish(n, name):
        out = {'name': name, **{k: v for k, v in n.items() if k != 'children'}}
        kids = [finish(c, k) for k, c in sorted(n['children'].items())]
        if kids:
            out['children'] = kids
        return out

    kinds = defaultdict(lambda: {'files': 0, 'bytes': 0})
    for path, (size, _) in removed.items():
        k = kinds[kind_of(path)]
        k['files'] += 1
        k['bytes'] += size

    up_bytes = sum(s for s, _ in upstream.values())
    rm_bytes = sum(s for s, _ in removed.values())
    groups = prune_groups(SHUTTLE_SRC)
    result = {
        'upstream': args.upstream,
        'fork': args.fork,
        'upstream_rev': subprocess.run(['git', '-C', str(SHUTTLE_SRC), 'rev-parse', '--short', args.upstream],
                                       check=True, capture_output=True, text=True).stdout.strip(),
        'fork_rev': subprocess.run(['git', '-C', str(SHUTTLE_SRC), 'rev-parse', '--short', args.fork],
                                   check=True, capture_output=True, text=True).stdout.strip(),
        'totals': {
            'upstream_files': len(upstream),
            'upstream_bytes': up_bytes,
            'fork_files': len(fork),
            'fork_bytes': sum(s for s, _ in fork.values()),
            'removed_files': len(removed),
            'removed_bytes': rm_bytes,
            'removed_files_pct': round(100 * len(removed) / len(upstream), 1),
            'removed_bytes_pct': round(100 * rm_bytes / up_bytes, 1),
            'added_files': len(added),
            'added_bytes': sum(s for s, _ in added.values()),
            'changed_upstream_files': len(changed),
            'prune_groups': len(groups),
            'prune_manifests': len({g['manifest'] for g in groups}),
        },
        'removed_by_kind': dict(sorted(kinds.items(), key=lambda kv: -kv[1]['bytes'])),
        'prune_groups': groups,
        'tree': finish(root, 'src'),
    }
    text = json.dumps(result, indent=1)
    if args.out:
        Path(args.out).write_text(text + '\n')
    else:
        print(text)
    t = result['totals']
    print(f"removed {t['removed_files']:,} of {t['upstream_files']:,} files ({t['removed_files_pct']} %), "
          f"{t['removed_bytes'] / 1e6:,.0f} of {t['upstream_bytes'] / 1e6:,.0f} MB ({t['removed_bytes_pct']} %); "
          f"added {t['added_files']:,} files; changed {t['changed_upstream_files']:,} upstream files; "
          f"{t['prune_groups']} prune groups", file=sys.stderr)


if __name__ == '__main__':
    main()
