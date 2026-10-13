#!/usr/bin/env python3
"""Remove Chromium's test targets (and the files only they use) from the fork.

Policy (sweep 2, user decision): tests are removed wholesale in SCOPE dirs; only the
`chrome` target has to build. Re-run after each upstream merge.

Steps:
  1. `gn gen out/TestScan --ide=json` with the dev build dir's args (prune.BUILD_DIR).
  2. Delete set D = testonly targets in SCOPE, minus those (transitively) needed by testonly
     targets outside SCOPE (e.g. blink unit tests use //content/test:test_support).
     The root //BUILD.gn doesn't pin; gn-fix-unresolved.py strips its deps afterwards.
  3. Files: referenced only by D targets, not used by the chrome build (prune.py's "used"),
     and either C++ (.cc/.mm/.h) or under a test-ish path. Resources outside test paths stay
     (grit/TS read undeclared inputs). Test-named C++ files no target references go too.
  4. BUILD.gn: remove the invocation blocks of D targets (`foo("name") { ... }`).

Usage: scripts/prune-tests.py [--apply]
Then:  git add -A; scripts/fix-gn-refs.py --apply; scripts/gn-fix-unresolved.py
"""
import argparse, collections, json, os, re, shutil, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prune  # noqa: E402

SRC = prune.SRC
SCOPE = ("chrome", "components", "content", "extensions", "services", "ui", "device")
SCAN = "out/TestScan"
TESTPATH = re.compile(
    r"(^|/)(test|tests|testing|test_support|testdata|test_data|fuzzers?|[a-z_]*fuzzer_corpus|corpus"
    r"|integration_tests|e2e_test_files|test_utils?|mocks?|fakes?|browsertests?|unittests?"
    r"|interactive_ui_tests|chromedriver|web_test|webui_test)(/|$)"
    r"|_(unittest|browsertest|interactive_uitest|uitest|apitest|test|tests|perftest|fuzzer|fuzztest"
    r"|test_util|test_utils|test_base|test_helper|test_helpers|test_support|test_api|mock|fake"
    r"|browsertest_base|browsertest_util|unittest_util|unittest_utils|testing|tester)(_[a-z0-9]+)*\.[a-z]+$"
    r"|(^|/)(fake|mock|test|scoped_test|stub|testing)_[^/]*$"
    r"|^(chrome|content|extensions|components|services)/test/")
CXX = (".cc", ".mm", ".h")
INVOKE = re.compile(r'^[ \t]*([A-Za-z_][\w.]*)\(\s*"([^"]+)"\s*\)\s*\{', re.M)


def norm(label):
    return label.split("(")[0]


def label_dir(label):
    return label[2:].split(":")[0]


def in_scope(path):
    return path.split("/")[0] in SCOPE


def load_targets():
    os.makedirs(os.path.join(SRC, SCAN), exist_ok=True)
    shutil.copy(os.path.join(SRC, prune.BUILD_DIR, "args.gn"), os.path.join(SRC, SCAN, "args.gn"))
    prune.run("gn", "gen", SCAN, "--ide=json", "--json-file-name=project.json")
    raw = json.load(open(os.path.join(SRC, SCAN, "project.json")))["targets"]
    tgt = {}
    for label, t in raw.items():
        e = tgt.setdefault(norm(label), {"testonly": False, "deps": set(), "files": set()})
        e["testonly"] |= bool(t.get("testonly"))
        e["deps"] |= {norm(d) for d in t.get("deps", [])}
        for k in ("sources", "public", "inputs", "data"):
            e["files"] |= {f[2:] for f in (t.get(k) or []) if f.startswith("//")}
        if t.get("script", "").startswith("//"):
            e["files"].add(t["script"][2:])
    return tgt


def delete_set(tgt):
    cand = {l for l, v in tgt.items() if v["testonly"] and in_scope(label_dir(l))}
    pinned, stack = set(), []
    for l, v in tgt.items():
        if in_scope(label_dir(l)) or l.startswith("//:"):
            continue
        stack += [d for d in v["deps"] if d in cand]
    # Walk through *all* deps: a non-testonly target (grit, ts_library) between two test
    # targets still needs its own testonly inputs.
    seen = set()
    while stack:
        x = stack.pop()
        if x not in seen:
            seen.add(x)
            stack += [d for d in tgt.get(x, {"deps": ()})["deps"]]
    pinned = cand & seen
    return cand - pinned, pinned


def doomed_files(tgt, D, used, tracked):
    gitlinks = {l.split("\t", 1)[1] for l in prune.run("git", "ls-files", "-s").split("\n")
                if l.startswith("160000 ")}
    keep, keepdirs, dele, deldirs = set(), set(), set(), set()
    for l, v in tgt.items():
        for f in v["files"]:
            if f.endswith("/"):
                (deldirs if l in D else keepdirs).add(f.rstrip("/"))
            else:
                (dele if l in D else keep).add(f)

    def under(f, dirs):
        p = f
        while "/" in p:
            p = p.rsplit("/", 1)[0]
            if p in dirs:
                return True
        return False

    gn_text = {}

    def listed_in_build_gn(f):
        """True if an enclosing BUILD.gn names f: the scan only sees this platform's targets."""
        d, rel = f, ""
        while "/" in d:
            d, base = d.rsplit("/", 1)
            rel = base if not rel else base + "/" + rel
            if d not in gn_text:
                try:
                    gn_text[d] = open(os.path.join(SRC, d, "BUILD.gn")).read()
                except OSError:
                    gn_text[d] = ""
            if '"' + rel + '"' in gn_text[d]:
                return True
        return False

    doom, why = [], collections.Counter()
    for f in tracked:
        if not in_scope(f) or f in used or f in keep or under(f, keepdirs):
            continue
        if f.endswith(("BUILD.gn", ".gni", ".gitignore")) or f in gitlinks:
            continue  # .gitignore keeps CIPD/DEPS-fetched test data out of `git add -A`
        testish = bool(TESTPATH.search(f))
        if f in dele and (f.endswith(CXX) or testish):
            r = "source of a deleted target"
        elif testish and under(f, deldirs):
            r = "data of a deleted target"
        elif testish and f.endswith(CXX):
            if listed_in_build_gn(f):
                continue  # used by another platform's targets (Linux ozone/IME/bluetooth stubs, fakes)
            r = "unreferenced test C++"
        else:
            continue
        doom.append(f)
        why[r] += 1
    return doom, why


def block_end(text, i):
    """Index after the brace that closes the '{' at text[i]."""
    depth, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "#":
            i = text.find("\n", i)
            if i < 0:
                return n
            continue
        if c == '"':
            i += 1
            while i < n and text[i] != '"':
                i += 2 if text[i] == "\\" else 1
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    raise ValueError("unbalanced braces")


def strip_blocks(D, apply, tracked):
    by_file = collections.defaultdict(set)
    for l in D:
        if os.path.join(label_dir(l), "BUILD.gn") not in tracked:
            continue  # untracked (CIPD/DEPS-fetched) BUILD files are not ours
        by_file[os.path.join(label_dir(l), "BUILD.gn")].add(l.split(":", 1)[1])
    removed, missing = 0, []
    for bf, names in sorted(by_file.items()):
        path = os.path.join(SRC, bf)
        if not os.path.exists(path):
            continue
        text, found = open(path).read(), set()
        while True:
            for m in INVOKE.finditer(text):
                if m.group(1) != "template" and m.group(2) in names:
                    start = text.rfind("\n", 0, m.start()) + 1
                    # take preceding comment lines along
                    while True:
                        prev = text.rfind("\n", 0, start - 1) + 1
                        if start > 0 and text[prev:start].lstrip().startswith("#"):
                            start = prev
                        else:
                            break
                    end = block_end(text, m.end() - 1)
                    if text[end:end + 1] == "\n":
                        end += 1
                    text = text[:start] + text[end:]
                    found.add(m.group(2))
                    removed += 1
                    break
            else:
                break
        missing += [f"//{os.path.dirname(bf)}:{n}" for n in names - found]
        if apply and found:
            os.chmod(path, os.stat(path).st_mode | 0o200)  # some are checked in read-only
            open(path, "w").write(re.sub(r"\n{3,}", "\n\n", text))
    return removed, missing


FUZZ_TEMPLATES = ("fuzzer_test", "fuzzer_testcase", "protobuf_mutator_fuzzer_test")


def strip_fuzzers(apply):
    """fuzzer_test() targets aren't testonly when fuzzing is off; remove them in SCOPE."""
    removed = 0
    for bf in prune.run("git", "ls-files", "--", *[f"{d}/**BUILD.gn" for d in SCOPE]).split():
        path = os.path.join(SRC, bf)
        text = orig = open(path).read()
        while True:
            m = next((m for m in INVOKE.finditer(text) if m.group(1) in FUZZ_TEMPLATES), None)
            if not m:
                break
            start = text.rfind("\n", 0, m.start()) + 1
            end = block_end(text, m.end() - 1)
            text = text[:start] + text[end + (text[end:end + 1] == "\n"):]
            removed += 1
        if apply and text != orig:
            os.chmod(path, os.stat(path).st_mode | 0o200)
            open(path, "w").write(text)
    return removed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()
    print("gn gen --ide=json ...", flush=True)
    tgt = load_targets()
    D, pinned = delete_set(tgt)
    print(f"testonly targets in scope: {len(D) + len(pinned)}, pinned by out-of-scope tests: "
          f"{len(pinned)}, to delete: {len(D)}")
    print("collecting files the chrome build uses ...", flush=True)
    used = prune.used_files(prune.BUILD_DIR)
    used |= prune.grd_refs(used)
    tracked = [t for t in prune.run("git", "ls-files").split("\n") if t and not t.startswith("tools/shuttle/")]
    doom, why = doomed_files(tgt, D, used, tracked)
    print(f"files to delete: {len(doom)}")
    for r, n in why.most_common():
        print(f"  {n:7}  {r}")
    open("/tmp/sh-prune-tests.txt", "w").write("\n".join(doom) + "\n")
    open("/tmp/sh-prune-tests-pinned.txt", "w").write("\n".join(sorted(pinned)) + "\n")
    removed, missing = strip_blocks(D, a.apply, set(tracked))
    open("/tmp/sh-prune-tests-unmatched-targets.txt", "w").write("\n".join(sorted(missing)) + "\n")
    print(f"BUILD.gn blocks removed: {removed}; target labels without a literal block "
          f"(template-generated etc.): {len(missing)} -> /tmp/sh-prune-tests-unmatched-targets.txt")
    print(f"fuzzer_test blocks removed: {strip_fuzzers(a.apply)}")
    print("file list: /tmp/sh-prune-tests.txt")
    if a.apply:
        for i in range(0, len(doom), 2000):
            subprocess.run(["git", "rm", "-q", "-f", "--"] + doom[i:i + 2000], cwd=SRC, check=True)
        print(f"removed {len(doom)} files (staged, not committed)")
    shutil.rmtree(os.path.join(SRC, SCAN), ignore_errors=True)


if __name__ == "__main__":
    main()
