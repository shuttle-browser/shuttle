#!/usr/bin/env python3
"""Delete code Shuttle never builds, driven by a prune manifest.

Rule `remove <path>` deletes unconditionally (feature removal; ignores "used").

Usage: scripts/prune.py [--apply [--allow-used-set]] [manifest ...]   (default: prune/platforms.txt)

Gate (maintenance review F4): rules other than `remove` delete what *this machine's* build doesn't read, so on the
Mac they see Linux-only files as dead. `--apply` therefore refuses them unless `--allow-used-set` is given for a
reviewed, deliberate run (never during a stable respin); remove-only manifests (explicit reviewed lists) apply as
before. `--apply` also needs a clean tree (it uses `git rm -f`).

Without --apply it's a dry run: it prints counts per rule and writes the full
file list to /tmp/sh-prune-<manifest>.txt.

Safety: a file is deleted only if it matches a manifest rule and isn't read by
any build config we use. "Used" is the union of:
  - BUILD_DIR (out/Sccache, $SHUTTLE_BUILD_DIR): `siso query inputs chrome`, plus the full deps log
    (`siso query deps`, every header actually included), plus
    build.ninja.d (all loaded gn files);
  - a temporary official-build config (gn only, no deps log).
Run it after a successful Dev build so the deps log is current. Then rebuild:
the rebuild should be a no-op.
"""
import argparse, os, re, shutil, subprocess, sys
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS, BUILD_DIR  # noqa: E402

SRC = _SHUTTLE_SRC
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SISO = os.path.join(SRC, "third_party/siso/cipd/siso")
ENV = dict(os.environ, PATH=os.environ["PATH"] + ":" + os.path.expanduser("~/depot_tools"))
def official_args():
    """Release config from config/args-*.gn, with PGO off (profiles may be absent)."""
    cfg = os.path.join(REPO, "config")
    text = open(os.path.join(cfg, "args-common.gn")).read() + open(os.path.join(cfg, "args-release.gn")).read()
    lines = [l.split("#")[0].strip() for l in text.splitlines()]
    return " ".join(l for l in lines if l) + " chrome_pgo_phase=0"

def run(*cmd, **kw):
    return subprocess.run(cmd, cwd=SRC, env=ENV, capture_output=True, text=True, check=True, **kw).stdout

def current_outputs(out_dir):
    """All outputs of the current build graph (build.ninja + subninjas)."""
    outs, todo, seen = set(), [os.path.join(SRC, out_dir, "build.ninja")], set()
    while todo:
        nf = todo.pop()
        if nf in seen:
            continue
        seen.add(nf)
        try:
            text = open(nf, errors="replace").read()
        except OSError:
            continue
        for line in text.splitlines():
            if line.startswith("build "):
                head = line[6:].split(": ", 1)[0].split(" | ")[0]
                outs.update(t.replace("$:", ":").replace("$ ", " ") for t in head.split())
            elif line.startswith(("subninja ", "include ")):
                todo.append(os.path.join(SRC, out_dir, line.split(None, 1)[1].strip()))
    return outs

def used_files(out_dir, deps_log=True):
    used = set()
    for line in run(SISO, "query", "inputs", "-C", out_dir, "chrome").splitlines():
        used.add(os.path.normpath(os.path.join(out_dir, line)))
    # `query inputs` misses many discovered headers; the deps log has them all.
    if deps_log and not os.path.exists(os.path.join(SRC, out_dir, ".siso_deps")):
        sys.exit(f"{out_dir}/.siso_deps missing: no deps log, the used set would miss every discovered header "
                 f"(build {out_dir} first)")
    if deps_log:
        p = subprocess.Popen([SISO, "query", "deps", "-C", out_dir], cwd=SRC, env=ENV,
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        live = current_outputs(out_dir)  # skip stale entries of targets no longer built
        keep_entry = False
        for line in p.stdout:
            if line.startswith("    "):
                if keep_entry:
                    used.add(os.path.normpath(os.path.join(out_dir, line.strip())))
            elif line.strip():
                keep_entry = line.split(":", 1)[0] in live
        if p.wait():
            sys.exit(f"siso query deps -C {out_dir} failed (rc {p.returncode})")
    with open(os.path.join(SRC, out_dir, "build.ninja.d")) as f:
        for tok in f.read().split():
            if not tok.endswith(":"):
                used.add(os.path.normpath(os.path.join(out_dir, tok)))
    return used

def load_rules(path):
    rules = {"remove": [], "dir": [], "segment": set(), "suffix": set(), "ext": set(), "keep": [], "deadtree": [], "deadfiles": []}
    for line in open(path):
        line = line.split("#")[0].strip()
        if line:
            kind, arg = line.split(None, 1)
            (rules[kind].add if isinstance(rules[kind], set) else rules[kind].append)(arg.strip("/"))
    return rules

TESTISH = re.compile(r"(^|_)(test|tests|testing|test_support|test_utils|fuzzers?|testdata|test_data)($|_)")

KEEP_NAMES = re.compile(r"^(BUILD\.gn|.*\.gni|OWNERS|DIR_METADATA|COMMON_METADATA|DEPS|README.*|.*\.md|LICENSE.*|PRESUBMIT.*\.py|\.gitignore|.*\.txt|.*\.py|.*\.sh)$")
TESTISH_FILE = re.compile(r"(test|fuzz|mock|fake|stub|benchmark|perftest)", re.I)

# Resource pipelines (grit scaled images, rollup, TS/WebUI) read undeclared
# inputs, so resources are only ever deleted by explicit `dir` rules.
RESOURCE_FILE = re.compile(r"\.(png|svg|ico|icon|jpg|jpeg|gif|webp|ts|js|mjs|css|html|json|xtb|grd|grdp|xml|wasm|ttf|woff2?|mp3|wav|lottie)$", re.I)

def dead_dirs(rules, tracked, used):
    """Maximal directories under a deadtree root that no build config reads.
    Test-named directories never anchor a dead tree (they may serve kept code)."""
    used_count = {}
    for f in tracked:
        if f in used:
            parts = f.split("/")
            for i in range(1, len(parts)):
                d = "/".join(parts[:i])
                used_count[d] = used_count.get(d, 0) + 1
    dead = set()
    for f in tracked:
        parts = f.split("/")
        for root in rules["deadtree"]:
            rp = root.split("/")
            if parts[:len(rp)] != rp:
                continue
            for i in range(len(rp) + 1, len(parts)):
                if TESTISH.search(parts[i - 1]):
                    break  # never anchor at or below a test directory
                d = "/".join(parts[:i])
                if used_count.get(d, 0) == 0:
                    dead.add(d)
                    break
    return dead

def matcher(rules, dead=frozenset()):
    suffix_re = re.compile(r"_(%s)(_[^/]*)?\.[^./]+$" % "|".join(map(re.escape, rules["suffix"]))) if rules["suffix"] else None
    def under(f, d):
        return f == d or f.startswith(d + "/")
    def match(f):
        if any(under(f, k) for k in rules["keep"]):
            return None
        for d in rules["remove"]:
            if under(f, d):
                return "remove " + d
        for d in rules["dir"]:
            if under(f, d):
                return "dir " + d
        if RESOURCE_FILE.search(f):
            return None
        if rules["deadfiles"] and any(under(f, r) for r in rules["deadfiles"]):
            name = f.rsplit("/", 1)[-1]
            if not KEEP_NAMES.search(name) and not TESTISH_FILE.search(f):
                return "deadfiles ." + (name.rsplit(".", 1)[-1] if "." in name else "")
        if dead:
            parts = f.split("/")
            for i in range(1, len(parts)):
                d = "/".join(parts[:i])
                if d in dead:
                    return "deadtree " + "/".join(parts[:min(i, 3)])
        parts = f.split("/")
        for seg in parts[:-1]:
            if seg in rules["segment"]:
                return "segment " + seg
        name = parts[-1]
        if suffix_re:
            m = suffix_re.search(name)
            if m:
                return "suffix " + m.group(1)
        ext = name.rsplit(".", 1)[-1] if "." in name else ""
        if ext in rules["ext"]:
            return "ext " + ext
        return None
    return match

def grd_refs(used):
    """Files referenced by used .grd/.grdp (incl. generated ones) and DEPS hooks."""
    extra = set()
    # grit reads translations (.xtb) and some parts without declaring them as
    # inputs; protect everything a used .grd/.grdp references.
    ref_re = re.compile(r'(?:path|file)="([^"$]+)"')
    pending = [f for f in used if f.endswith((".grd", ".grdp"))]
    seen = set()
    while pending:
        g = pending.pop()
        if g in seen:
            continue
        seen.add(g)
        try:
            text = open(os.path.join(SRC, g), errors="replace").read()
        except OSError:
            continue
        for ref in ref_re.findall(text):
            f = os.path.normpath(os.path.join(os.path.dirname(g), ref))
            extra.add(f)
            if f.endswith(".grdp"):
                pending.append(f)
    # Generated .grd/.grdp files (written at build time) reference sources as
    # ${root_src_dir}/... and aren't declared inputs either.
    for root, _, files in os.walk(os.path.join(SRC, BUILD_DIR, "gen")):
        for fn in files:
            if fn.endswith((".grd", ".grdp")):
                text = open(os.path.join(root, fn), errors="replace").read()
                for ref in re.findall(r'(?:path|file)="\$\{root_src_dir\}/([^"]+)"', text):
                    extra.add(os.path.normpath(ref))
    deps_text = open(os.path.join(SRC, "DEPS")).read()
    extra |= {m for m in re.findall(r"'src/([^']+\.py)'", deps_text)}  # gclient hooks
    # CIPD `version_file`s: gclient reads them while parsing DEPS, even when the dep's condition is false (155+).
    extra |= set(re.findall(r"'version_file':\s*'([^']+)'", deps_text))

    return extra

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--allow-used-set", action="store_true",
                    help="allow --apply of rules other than `remove` (deletes what this machine's build doesn't read)")
    ap.add_argument("manifests", nargs="*", default=[os.path.join(REPO, "prune/platforms.txt")])
    a = ap.parse_args()

    all_rules = [load_rules(m) for m in a.manifests]
    remove_only = all(not any(v for k, v in r.items() if k not in ("remove", "keep")) for r in all_rules)
    if a.apply and not remove_only and not a.allow_used_set:
        sys.exit("refusing --apply: the manifest has rules other than `remove`, which delete files this machine's "
                 "build doesn't read (Mac-only used set). Convert the dry-run list into a reviewed `remove` "
                 "manifest, or pass --allow-used-set deliberately.")
    if a.apply and run("git", "status", "--porcelain", "--untracked-files=no").strip():
        sys.exit("refusing --apply: uncommitted changes to tracked files (git rm -f would discard them)")
    if remove_only:
        used = set()  # remove-only manifests: unconditional, no build analysis needed
        tracked = [t for t in run("git", "ls-files").splitlines() if not t.startswith("tools/shuttle/")]  # not the tooling
        return prune(a, all_rules, tracked, used)
    print(f"collecting used files: {BUILD_DIR} ...", flush=True)
    used = used_files(BUILD_DIR)
    print("collecting used files: temporary official config ...", flush=True)
    tmp = "out/PruneCheck"
    try:
        run("gn", "gen", tmp, "--args=" + official_args())
        used |= used_files(tmp, deps_log=False)
    finally:
        shutil.rmtree(os.path.join(SRC, tmp), ignore_errors=True)
    used |= grd_refs(used)
    tracked = [t for t in run("git", "ls-files").splitlines() if not t.startswith("tools/shuttle/")]  # not the tooling
    prune(a, all_rules, tracked, used)

def prune(a, all_rules, tracked, used):
    for manifest, rules in zip(a.manifests, all_rules):
        match = matcher(rules, dead_dirs(rules, tracked, used) if rules["deadtree"] else frozenset())
        by_rule, doomed, protected = {}, [], 0
        for f in tracked:
            r = match(f)
            if not r:
                continue
            if f in used and not r.startswith("remove "):
                protected += 1
                continue
            doomed.append(f)
            by_rule[r] = by_rule.get(r, 0) + 1
        name = os.path.splitext(os.path.basename(manifest))[0]
        listing = f"/tmp/sh-prune-{name}.txt"
        open(listing, "w").write("\n".join(doomed) + "\n")
        print(f"\n{name}: {len(doomed)} files to delete, {protected} matched but kept (used by the build)")
        for r, n in sorted(by_rule.items(), key=lambda x: -x[1])[:25]:
            print(f"  {n:7}  {r}")
        print(f"  full list: {listing}")
        if a.apply and doomed:
            for i in range(0, len(doomed), 2000):
                subprocess.run(["git", "rm", "-q", "-f", "--"] + doomed[i:i + 2000], cwd=SRC, check=True)
            print(f"  removed {len(doomed)} files (staged, not committed)")

if __name__ == "__main__":
    main()
