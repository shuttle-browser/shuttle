#!/usr/bin/env python3
"""Delete enterprise policy definitions (YAML) from components/policy.

Usage: scripts/prune-policies.py [--apply] <names.txt>   (one policy name per line, # comments)

For each policy: `git rm` its YAML, blank its id in policies.yaml (`N: ''` is upstream's
"deleted id" marker), drop it from policy_atomic_groups.yaml lists (blanking atomic groups that
end up empty), and delete group dirs left without policies.
C++ references (`key::kName`, kSimplePolicyMap entries) are NOT touched: remove them first.
"""
import argparse, os, re, subprocess, sys
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS  # noqa: E402

SRC = _SHUTTLE_SRC
T = "components/policy/resources/templates"
DEFS = f"{T}/policy_definitions"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("names")
    a = ap.parse_args()
    names = {l.split("#")[0].strip() for l in open(a.names)} - {""}
    os.chdir(SRC)
    yamls = {}
    for d in sorted(os.listdir(DEFS)):
        p = f"{DEFS}/{d}"
        if os.path.isdir(p):
            for f in os.listdir(p):
                if f.endswith(".yaml") and not f.startswith(".") and f != "policy_atomic_groups.yaml":
                    yamls[f[:-5]] = f"{p}/{f}"
    missing = names - set(yamls)
    if missing:
        sys.exit(f"unknown policies: {sorted(missing)}")
    doomed = [yamls[n] for n in sorted(names)]

    # policies.yaml: blank ids of deleted policies.
    pol = open(f"{T}/policies.yaml").read()
    pol_new, n_ids = re.subn(r"^(  \d+): (\w+)$",
                             lambda m: f"{m.group(1)}: ''" if m.group(2) in names else m.group(0),
                             pol, flags=re.M)
    blanked = sum(1 for n in names if re.search(rf"^  \d+: {n}$", pol, re.M))

    # Atomic groups.
    empty_groups, ag_edits = [], {}
    for d in sorted(os.listdir(DEFS)):
        agf = f"{DEFS}/{d}/policy_atomic_groups.yaml"
        if not os.path.exists(agf):
            continue
        lines, out, cur, cur_items = open(agf).read().split("\n"), [], None, []
        groups = {}
        for l in lines:
            m = re.match(r"^(\w+):\s*$", l)
            if m:
                cur = m.group(1); groups[cur] = []
            m2 = re.match(r"^  - (\w+)\s*$", l)
            if m2 and cur:
                groups[cur].append(m2.group(1))
                if m2.group(1) in names:
                    continue
            out.append(l)
        dead = [g for g, items in groups.items() if items and all(i in names for i in items)]
        if dead:
            # drop the whole group stanza
            text, keep, skip = "\n".join(out), [], False
            for l in text.split("\n"):
                m = re.match(r"^(\w+):\s*$", l)
                if m:
                    skip = m.group(1) in dead
                if not skip:
                    keep.append(l)
            out = keep
            empty_groups += dead
        if out != lines:
            ag_edits[agf] = "\n".join(out)
    for g in empty_groups:
        pol_new = re.sub(rf"^(  \d+): {g}$", r"\1: ''", pol_new, flags=re.M)

    # Device proto maps stay: the generator copies them without validating names.
    maps = {}

    # Group dirs left without policies.
    left = {}
    for n, p in yamls.items():
        left.setdefault(os.path.dirname(p), []).append(n)
    dead_dirs = [d for d, ns in left.items() if all(n in names for n in ns)]

    print(f"policies to delete: {len(doomed)} (ids blanked: {blanked}); atomic groups emptied: "
          f"{empty_groups}; atomic group files edited: {len(ag_edits)}; proto maps edited: "
          f"{len(maps)}; group dirs removed: {[os.path.basename(d) for d in dead_dirs]}")
    if not a.apply:
        return
    open(f"{T}/policies.yaml", "w").write(pol_new)
    for f, t in {**ag_edits, **maps}.items():
        if f in ag_edits and not re.search(r"^\w+:", t, re.M):
            subprocess.run(["git", "rm", "-q", "-f", "--", f], check=True)  # yaml would load None
        else:
            open(f, "w").write(t)
    for i in range(0, len(doomed), 500):
        subprocess.run(["git", "rm", "-q", "--"] + doomed[i:i + 500], check=True)
    for d in dead_dirs:
        subprocess.run(["git", "rm", "-q", "-r", "-f", "--", d], check=True)


if __name__ == "__main__":
    main()
