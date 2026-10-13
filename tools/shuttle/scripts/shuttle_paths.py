"""Checkout paths for the tooling scripts (mirror of scripts/lib/paths.sh). The tooling lives in the fork at
src/tools/shuttle/, so paths are repo-relative: SRC = the fork (src/), ROOT = its parent. Override with SHUTTLE_SRC / SHUTTLE_ROOT."""
import os

TOOLS = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
SRC = os.path.expanduser(os.environ.get("SHUTTLE_SRC") or os.path.dirname(os.path.dirname(TOOLS)))
ROOT = os.path.expanduser(os.environ.get("SHUTTLE_ROOT") or os.path.dirname(SRC))
LOGS = os.path.join(ROOT, "logs")
RELEASE = os.path.join(ROOT, "release")
# Dev-config build dir the prune/fix scripts read (siso inputs, gen/, args.gn). out/Dev (local compiles) was dropped
# 2026-10-13; out/Sccache has the same dev args plus sccache. Override with SHUTTLE_BUILD_DIR.


def _load_env():
    """Machine-local settings from ~/.shuttle/env (KEY=VALUE lines; env vars already set win)."""
    path = os.path.join(os.path.expanduser(os.environ.get("SHUTTLE_CONF", "~/.shuttle")), "env")
    if not os.path.isfile(path):
        return
    for line in open(path):
        line = line.split("#", 1)[0].strip()
        if line.startswith("export "):
            line = line[len("export "):]
        if "=" not in line or " " in line.split("=", 1)[0]:
            continue
        k, v = line.split("=", 1)
        os.environ.setdefault(k.strip(), v.strip().strip("'\""))


_load_env()
BUILD_DIR = os.environ.get("SHUTTLE_BUILD_DIR", "out/Sccache")
