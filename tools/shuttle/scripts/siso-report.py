#!/usr/bin/env python3
"""Build-time report from siso metrics (one or more runs of the same out dir, e.g. an interrupted + resumed build).

Usage: scripts/siso-report.py [--title T] [--cores N] [--top 15] out/Release/siso_metrics.0.json out/Release/siso_metrics.json
Prints Markdown: per-run wall time / steps / CPU time, then a breakdown by module, step kind and toolchain.

Metrics per step (siso_metrics.json, JSON lines): run_nanos (the command's own run time; duration_nanos also
counts queue_nanos, the wait for a free local slot, so it is not used), utime_nanos + stime_nanos (CPU time),
weighted_duration_nanos (run time / steps running at the same time: the sum over all steps ~ build wall time, i.e.
the share of wall clock a group used), gn_target (empty for some generated steps: then the output path is used),
action (non-default toolchains are a prefix, e.g. clang_x64_cxx), outputs. Steps with "no_exec" (copies, stamps)
count as steps with ~0 time.
Stdlib only.
"""
import argparse
import json
import re
from collections import defaultdict

MODULES = [  # first match wins; gn_target path prefix -> module
    ('third_party/blink/', 'blink'), ('v8', 'v8'), ('third_party/devtools-frontend/', 'devtools-frontend'),
    ('third_party/dawn/', 'dawn'), ('third_party/skia/', 'skia'), ('third_party/angle/', 'angle'),
    ('third_party/swiftshader/', 'swiftshader'), ('third_party/rust/', 'rust crates'),
    ('third_party/ffmpeg/', 'ffmpeg'), ('third_party/webrtc/', 'webrtc'), ('third_party/boringssl/', 'boringssl'),
    ('third_party/icu/', 'icu'), ('third_party/perfetto/', 'perfetto'), ('third_party/abseil-cpp/', 'abseil'),
    ('third_party/libc++/', 'libc++'), ('buildtools/', 'libc++'), ('build/rust/', 'rust std'),
    ('third_party/', 'third_party (other)'), ('skia', 'skia'), ('content/', 'content'), ('chrome/', 'chrome'),
    ('components/', 'components'), ('services/', 'services'), ('net/', 'net'), ('ui/', 'ui'), ('gpu/', 'gpu'),
    ('media/', 'media'), ('cc/', 'cc'), ('base/', 'base'), ('mojo/', 'mojo'), ('extensions/', 'extensions'),
    ('storage/', 'storage'), ('sql/', 'sql'), ('device/', 'device'), ('tools/', 'tools'), ('crypto', 'crypto'),
    ('url', 'url'), ('ipc', 'ipc'), ('sandbox/', 'sandbox'), ('printing/', 'printing'), ('pdf/', 'pdf'),
    ('build/', 'build'), ('google_apis/', 'google_apis'),
]


SUFFIXES = ('cxx', 'cc', 'objcxx', 'objc', 'asm', 'alink', 'solink_module', 'solink', 'link', 'rust_rlib', 'rust_bin',
            'rust_macro', 'rust_staticlib', 'rust_cdylib', 'rust_dylib', 'copy', 'stamp', 'copy_bundle_data',
            'compile_xcassets')


def split_action(a):
    """'clang_x64_v8_arm64_cxx' -> ('clang_x64_v8_arm64', 'cxx'); rule actions name their toolchain inside."""
    for suf in SUFFIXES:
        if a == suf:
            return '', suf
        if a.endswith('_' + suf):
            return a[:-len(suf) - 1], suf
    m = re.search(r'_build_toolchain_(\w+?)__rule$', a)
    return (m.group(1).replace('mac_clang_arm64', ''), 'action') if m else ('', 'action')


def target_path(s):
    """Source directory of a step: from its output (obj/<dir>/..., gen/<dir>/..., optionally under a toolchain dir),
    because siso credits many compiles to the final link target (gn_target chrome:chrome_framework_shared_library)."""
    out = (s.get('outputs') or [''])[0].split('/')
    if out and out[0].startswith('clang_'):
        out = out[1:]
    if out and out[0] in ('obj', 'gen'):
        return '/'.join(out[1:])
    t = s.get('gn_target') or ''
    return t.split(':', 1)[0].split('(', 1)[0].lstrip('/')


def module(path):
    for pre, name in MODULES:
        if path.startswith(pre) or path == pre.rstrip('/'):
            return name
    return 'other'


def kind(s):
    k = split_action(s.get('action') or '')[1]
    if k in ('cxx', 'cc', 'objcxx', 'objc', 'asm'):
        return 'compile C/C++/ObjC'
    if k.startswith('rust_'):
        return 'compile Rust'
    if k in ('link', 'solink', 'solink_module'):
        return 'link'
    if k == 'alink':
        return 'archive (.a)'
    if k in ('stamp', 'copy', 'copy_bundle_data'):
        return 'stamp/copy'
    return 'mojom codegen' if 'mojom' in target_path(s) else 'action (scripts/codegen)'


def toolchain(s):
    tc = split_action(s.get('action') or '')[0]
    tc = tc.replace('linux_clang', 'clang')
    return {'': 'mac arm64 (target)', 'clang_x64': 'linux x64 (host tools)',
            'clang_x64_v8_arm64': 'v8 snapshot host (x64 -> arm64)',
            'clang_x64_for_rust_host_build_tools': 'rust host build tools'}.get(tc, tc)


def run_ns(s):
    return s.get('run_nanos', 0)


def cpu_ns(s):
    return s.get('utime_nanos', 0) + s.get('stime_nanos', 0)


def load(path):
    steps, meta = [], {}
    with open(path) as f:
        for line in f:
            try:
                d = json.loads(line)
            except ValueError:
                continue
            if 'step_id' in d:
                steps.append(d)
            elif 'duration_nanos' in d and 'build_id' in d:
                meta['total_nanos'] = d['duration_nanos']   # last build-level record = whole build
    return steps, meta


def fmt_s(ns):
    s = ns / 1e9
    return f'{s / 3600:.2f} h' if s >= 3600 else f'{s / 60:.1f} min' if s >= 60 else f'{s:.0f} s'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('files', nargs='+')
    ap.add_argument('--title', default='Build time report')
    ap.add_argument('--cores', type=int, default=0, help='vCPUs, for the utilization column')
    ap.add_argument('--top', type=int, default=12)
    a = ap.parse_args()

    allsteps, runs = [], []
    for p in a.files:
        st, meta = load(p)
        # build-level records come first (manifest regen); an interrupted run has no final one: use the last step end
        wall = max([meta.get('total_nanos', 0)] + [s.get('action_end_nanos', 0) for s in st])
        cpu = sum(cpu_ns(s) for s in st)
        runs.append((p, len(st), wall, cpu))
        allsteps += st

    print(f'# {a.title}\n')
    print('| run | steps | wall time | CPU time (user+sys) |' + (' avg busy vCPUs |' if a.cores else ''))
    print('|---|---:|---:|---:|' + ('---:|' if a.cores else ''))
    tw = tc = 0
    for p, n, w, c in runs:
        tw += w
        tc += c
        busy = f' {c / w:.1f} of {a.cores} |' if a.cores and w else ''
        print(f'| `{p.rsplit("/", 1)[-1]}` | {n:,} | {fmt_s(w)} | {fmt_s(c)} |{busy}')
    busy = f' {tc / tw:.1f} of {a.cores} |' if a.cores and tw else ''
    print(f'| **combined** | **{len(allsteps):,}** | **{fmt_s(tw)}** | **{fmt_s(tc)}** |{busy}\n')

    def table(title, keyf, top=None):
        g = defaultdict(lambda: [0, 0, 0, 0, ''])   # steps, cpu, weighted, longest run, its output
        for s in allsteps:
            k = keyf(s)
            e = g[k]
            d = run_ns(s)
            e[0] += 1
            e[1] += cpu_ns(s)
            e[2] += s.get('weighted_duration_nanos', 0)
            if d > e[3]:
                e[3], e[4] = d, (s.get('outputs') or [''])[0]
        rows = sorted(g.items(), key=lambda kv: -kv[1][1])
        tot_w = sum(v[2] for v in g.values()) or 1
        tot_c = sum(v[1] for v in g.values()) or 1
        print(f'## {title}\n')
        print('| group | steps | CPU time | share of CPU | share of wall (siso weighted) | longest step |')
        print('|---|---:|---:|---:|---:|---|')
        shown = rows[:top] if top else rows
        for k, (n, c, w, lo, lo_out) in shown:
            print(f'| {k} | {n:,} | {fmt_s(c)} | {100 * c / tot_c:.1f}% | {100 * w / tot_w:.1f}% | {fmt_s(lo)} `{lo_out.rsplit("/", 1)[-1][:48]}` |')
        if top and len(rows) > top:
            rest = rows[top:]
            n = sum(v[0] for _, v in rest)
            c = sum(v[1] for _, v in rest)
            w = sum(v[2] for _, v in rest)
            print(f'| ({len(rest)} more) | {n:,} | {fmt_s(c)} | {100 * c / tot_c:.1f}% | {100 * w / tot_w:.1f}% | |')
        print()

    table('By module', lambda s: module(target_path(s)), a.top)
    table('By step kind', kind)
    table('By toolchain', toolchain)
    print('"CPU time" is user+sys of the group\'s commands; on a CPU-bound build "share of CPU" is the best estimate '
          'of its share of wall time. "Share of wall (siso weighted)" is siso\'s run-time/concurrency split, which '
          'under-weights long compiles; kept for reference. "Longest step" is one command\'s run time (no queueing).')


if __name__ == '__main__':
    main()
