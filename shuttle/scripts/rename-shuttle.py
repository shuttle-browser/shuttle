#!/usr/bin/env python3
"""One-off: rename the fork's own code from Agent Chrome to Shuttle (_docs/plans/REBRAND-SHUTTLE.md, R3).

  scripts/rename-shuttle.py [--apply]

Moves chrome/browser/agent_chrome -> chrome/browser/shuttle (agent_chrome_service* -> shuttle_service*)
and rewrites identifiers, paths, prefs, storage names, comments and copyright headers in tracked files.
Branding (BRANDING, profile dirs, keychain, chromium_strings + xtbs, icons) stays with scripts/rebrand.py,
so its files (SKIP) and *.xtb are skipped here.
Never run with --apply while a build compiles from $SHUTTLE_ROOT/src.
"""
import os, re, subprocess, sys
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS  # noqa: E402

SRC = _SHUTTLE_SRC
SKIP = {  # owned by scripts/rebrand.py (its EDITS + STRING_FILES)
    "chrome/app/chromium_strings.grd", "chrome/app/settings_chromium_strings.grdp", "chrome/app/theme/chromium/BRANDING",
    "chrome/common/chrome_paths_mac.mm", "chrome/common/chrome_paths_linux.cc", "chrome/browser/mac/initial_prefs.mm",
    "components/os_crypt/common/keychain_password_mac.mm",
    "components/os_crypt/async/browser/freedesktop_secret_key_provider.h",
}

MOVES = [
    ("chrome/browser/agent_chrome/agent_chrome_service.cc", "chrome/browser/shuttle/shuttle_service.cc"),
    ("chrome/browser/agent_chrome/agent_chrome_service.h", "chrome/browser/shuttle/shuttle_service.h"),
    ("chrome/browser/agent_chrome/agent_chrome_service_factory.cc", "chrome/browser/shuttle/shuttle_service_factory.cc"),
    ("chrome/browser/agent_chrome/agent_chrome_service_factory.h", "chrome/browser/shuttle/shuttle_service_factory.h"),
]

# Ordered (regex, replacement); applied to every tracked text file that matches PROBE.
REPS = [
    (r"The Agent Chrome Authors", "Silkweave"),
    (r"chrome/browser/agent_chrome", "chrome/browser/shuttle"),
    (r"CHROME_BROWSER_AGENT_CHROME_AGENT_CHROME_SERVICE", "CHROME_BROWSER_SHUTTLE_SHUTTLE_SERVICE"),
    (r"CHROME_BROWSER_AGENT_CHROME_", "CHROME_BROWSER_SHUTTLE_"),
    (r"AGENT_CHROME_", "SHUTTLE_"),
    (r"agent_chrome_service", "shuttle_service"),
    (r"AgentChromeService", "ShuttleService"),
    (r"AgentChrome", "Shuttle"),
    (r"\bagent_chrome\b", "shuttle"),
    (r"agent_chrome_", "shuttle_"),
    (r"agent-chrome-tools", "shuttle-tools"),
    (r"TODO\(agent-chrome\)", "TODO(shuttle)"),
    (r"agent-chrome", "shuttle"),
    (r"Agent Chrome", "Shuttle"),
]
PROBE = re.compile(r"agent_chrome|AgentChrome|AGENT_CHROME|agent-chrome|Agent Chrome")
EXTRA = [  # exact edits outside the generic patterns
    ("chrome/browser/shuttle/profile_registry.cc", 'kDomainPrefix[] = "ac-";', 'kDomainPrefix[] = "shuttle-";'),
]


def git(*a):
    return subprocess.run(["git", "-C", SRC, *a], check=True, capture_output=True, text=True).stdout


def main():
    apply = "--apply" in sys.argv
    if apply:
        if os.path.isdir(os.path.join(SRC, "chrome/browser/agent_chrome")):
            git("mv", "chrome/browser/agent_chrome", "chrome/browser/shuttle")
        for old, new in MOVES:
            old = old.replace("/agent_chrome/", "/shuttle/")
            if os.path.exists(os.path.join(SRC, old)):
                git("mv", old, new)
    files = [f for f in git("grep", "-lE", PROBE.pattern, "--", ":!*.xtb", ":!shuttle").splitlines() if f not in SKIP]
    total = 0
    for f in files:
        p = os.path.join(SRC, f)
        try:
            s = orig = open(p, encoding="utf-8").read()
        except (UnicodeDecodeError, FileNotFoundError):
            print("skip", f)
            continue
        for rx, rep in REPS:
            s = re.sub(rx, rep, s)
        if s != orig:
            total += 1
            if apply:
                open(p, "w", encoding="utf-8").write(s)
            else:
                print("edit", f)
    for f, old, new in EXTRA:
        p = os.path.join(SRC, f if apply else f.replace("/shuttle/", "/agent_chrome/"))
        s = open(p).read()
        if old in s and apply:
            open(p, "w").write(s.replace(old, new))
        elif new not in s and old not in s:
            sys.exit(f"ERROR {f}: {old!r} not found")
    print(f"{total} files {'rewritten' if apply else 'to rewrite (dry run)'}")
    left = subprocess.run(["git", "-C", SRC, "grep", "-nE", PROBE.pattern, "--", ":!*.xtb", ":!shuttle", *[f":!{s}" for s in SKIP]],
                          capture_output=True, text=True).stdout
    if apply and left:
        print("LEFT:\n" + left)


if __name__ == "__main__":
    main()
