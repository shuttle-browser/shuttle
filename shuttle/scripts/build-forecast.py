#!/usr/bin/env python3
"""Forecast the remaining work in a running Chromium build, grouped by area.

Usage: scripts/build-forecast.py [Dev|Release] [top_n]

Remaining steps are the compile, link and Rust outputs of //chrome:chrome's
dependency tree that don't exist on disk yet. Their cost is estimated from the
median step time per area in .ninja_log, which siso rewrites on each run, so
estimates sharpen as the run goes on. Read-only: safe while a build is running.
Siso's scheduling order isn't fixed, so this says how much heavy work is left,
not exactly when it runs.
"""
import os, re, statistics, subprocess, sys
from collections import defaultdict
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS  # noqa: E402

out_name = sys.argv[1] if len(sys.argv) > 1 else "Dev"
top_n = int(sys.argv[2]) if len(sys.argv) > 2 else 15
SRC = _SHUTTLE_SRC
OUT = os.path.join(SRC, "out", out_name)
JOBS = int(os.environ.get("JOBS", "6"))

def area(path):
    p = path.split("/")
    if "obj" in p:
        p = p[p.index("obj") + 1:]
    depth = 3 if p[0] in ("third_party", "components", "chrome", "content") else 2
    if p[:2] == ["third_party", "blink"]:
        depth = 4
    return "/".join(p[:min(depth, len(p) - 1)]) or p[0]

def kind(rule):
    for k in ("objcxx", "objc", "cxx", "cc", "asm", "solink", "alink", "link"):
        if rule == k or rule.endswith("_" + k):
            return k
    return "rust" if "rust" in rule else None

# 1. Targets in chrome's dependency tree -> their per-target ninja files.
env = dict(os.environ, PATH=os.environ["PATH"] + ":" + os.path.expanduser("~/depot_tools"))
labels = subprocess.run(["gn", "desc", f"out/{out_name}", "//chrome:chrome", "deps", "--all"],
                        cwd=SRC, env=env, capture_output=True, text=True, check=True).stdout.split()
labels.append("//chrome:chrome")
ninja_files = []
for lab in labels:
    m = re.match(r"//([^:]*):([^(]+)(?:\(//build/toolchain/[^:]+:([^)]+)\))?$", lab)
    if not m:
        continue
    d, name, tc = m.groups()
    rel = os.path.join("obj", d, name + ".ninja")
    if tc and tc != "clang_arm64":
        rel = os.path.join(tc, rel)
    ninja_files.append(os.path.join(OUT, rel))

# 2. Outputs of compile/link/rust steps; remaining = not on disk.
build_re = re.compile(r"^build (.+?): (\S+)")
remaining = defaultdict(lambda: defaultdict(int))
total = done = 0
for nf in ninja_files:
    try:
        lines = open(nf, errors="replace").read().splitlines()
    except FileNotFoundError:
        continue
    for line in lines:
        m = build_re.match(line)
        if not m:
            continue
        k = kind(m.group(2))
        if not k:
            continue
        out = m.group(1).split(" | ")[0].split()[0].replace("$:", ":")
        total += 1
        if os.path.exists(os.path.join(OUT, out)):
            done += 1
        else:
            remaining[area(out)][k] += 1

# 3. Median step time per area from this run's .ninja_log.
dur = defaultdict(list)
all_compile = []
for line in open(os.path.join(OUT, ".ninja_log")):
    if line.startswith("#"):
        continue
    s, e, _, out, _ = line.rstrip("\n").split("\t")
    secs = (int(e) - int(s)) / 1000
    if out.endswith(".o"):
        dur[area(out)].append(secs)
        all_compile.append(secs)
global_med = statistics.median(all_compile) if all_compile else 5.0
LINK_SECS = {"solink": 20, "link": 30, "alink": 3, "rust": 15}

rows = []
for a, kinds in remaining.items():
    n_compile = sum(v for k, v in kinds.items() if k not in LINK_SECS)
    med = statistics.median(dur[a]) if len(dur[a]) >= 5 else None
    cost = n_compile * (med or global_med) + sum(LINK_SECS[k] * v for k, v in kinds.items() if k in LINK_SECS)
    rows.append((cost, a, n_compile, sum(kinds.values()) - n_compile, med))
rows.sort(reverse=True)

total_cost = sum(r[0] for r in rows)
print(f"chrome tree: {total} compile/link/rust steps, {done} done, {total - done} left "
      f"({len(all_compile)} compiles timed this run, median {global_med:.1f}s)")
print(f"estimated CPU time left: {total_cost / 3600:.1f}h  ->  ~{total_cost / 3600 / JOBS:.1f}h wall at -j {JOBS}\n")
print(f"{'area':45} {'TUs':>6} {'links':>5} {'s/TU':>6} {'CPU-h':>6} {'share':>6}")
for cost, a, n, links, med in rows[:top_n]:
    print(f"{a:45} {n:6} {links:5} {(f'{med:.1f}' if med else '~' + f'{global_med:.0f}'):>6} "
          f"{cost / 3600:6.2f} {100 * cost / total_cost:5.1f}%")
rest = rows[top_n:]
if rest:
    c = sum(r[0] for r in rest)
    print(f"{'(' + str(len(rest)) + ' other areas)':45} {sum(r[2] for r in rest):6} {sum(r[3] for r in rest):5} "
          f"{'':>6} {c / 3600:6.2f} {100 * c / total_cost:5.1f}%")
