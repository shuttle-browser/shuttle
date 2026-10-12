#!/usr/bin/env python3
"""Make the fork en-US only: strip per-locale grit outputs/translations and delete .xtb files.

Usage: scripts/prune-locales.py [--apply] [--src $SHUTTLE_ROOT/src] [--keep en-US --keep en]

For every tracked .grd/.grdp (except grit's own test data):
  - removes <output .../> and <file .../> elements whose lang="X" is not kept
    (per-locale .pak / android / xtb translation entries),
  - removes <if>/<then>/<else> blocks left empty by that.
Then deletes tracked .xtb files no remaining grd references, drops their lines from
*.grd.gritdeps, and lists other BUILD files still naming .xtb files (fix by hand).
Rerun after every upstream merge (before scripts/rebrand.py). Dry run by default.
"""
import argparse
import os
import re
import subprocess
import sys
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS  # noqa: E402

SKIP_PREFIXES = ("tools/grit/", "tools/translation/", "shuttle/")  # shuttle/ = this tooling


def git_ls(src, *patterns):
    out = subprocess.run(["git", "-C", src, "ls-files", "-z", "--", *patterns],
                         check=True, capture_output=True, text=True).stdout
    return [p for p in out.split("\0") if p and not p.startswith(SKIP_PREFIXES)]


ELEM_RE = re.compile(r'[ \t]*<(output|file)\b[^>]*?\blang="(?P<lang>[^"]+)"[^>]*?/>[ \t]*\n?', re.S)
# empty <if>, or <if> whose <then> and <else> are both empty (grit rejects <if> with an <else> but no <then>)
_E = r'(?:\s|<!--(?:(?!-->).)*-->)*'  # whitespace and comments
EMPTY_RE = re.compile(r'[ \t]*<if\b[^>]*>' + _E + r'(?:<then>' + _E + r'</then>' + _E + r'(?:<else>' + _E + r'</else>' + _E + r')?)?</if>[ \t]*(?:<!--[^\n]*?-->[ \t]*)?\n?', re.S)
# After a run, check that every <if> with <then>/<else> has exactly both (grit rule); see check_if_structure().

LANG_IF_RE = re.compile(r'''[ \t]*<if expr="'(?P<lang>[^']+)' == lang">[^\n]*?</if>[ \t]*\n?''')
FILE_REF_RE = re.compile(r'''<include\b[^>]*\bfile="([^"]+)"''')
XTB_REF_RE = re.compile(r'path="([^"]+\.xtb)"')


def strip_grd(text, keep):
    removed = 0

    def drop(m):
        nonlocal removed
        if m.group("lang") in keep:
            return m.group(0)
        removed += 1
        return ""

    text = ELEM_RE.sub(drop, text)
    # per-language one-line includes (Google ToS translations): <if expr="'de' == lang">...</if>
    text = LANG_IF_RE.sub(drop, text)
    while True:
        text, n = EMPTY_RE.subn("", text)
        if not n:
            break
    return text, removed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--src", default=_SHUTTLE_SRC)
    ap.add_argument("--keep", action="append", default=None)
    a = ap.parse_args()
    keep = set(a.keep or ["en-US", "en"])  # "en" = untranslated data packs
    src = a.src

    # 1. grd/grdp
    edited, total = 0, 0
    remaining_xtb = set()
    dropped_files = set()  # per-language <include> files no grd uses any more
    for rel in git_ls(src, "*.grd", "*.grdp"):
        path = os.path.join(src, rel)
        with open(path, encoding="utf-8") as f:
            text = f.read()
        new, n = strip_grd(text, keep)
        if n:
            old_refs = {os.path.normpath(os.path.join(os.path.dirname(rel), f))
                        for f in FILE_REF_RE.findall(text)}
            new_refs = {os.path.normpath(os.path.join(os.path.dirname(rel), f))
                        for f in FILE_REF_RE.findall(new)}
            dropped_files.update(old_refs - new_refs)
        for ref in XTB_REF_RE.findall(new):
            remaining_xtb.add(os.path.basename(ref))
        if n:
            edited += 1
            total += n
            if a.apply:
                with open(path, "w", encoding="utf-8") as f:
                    f.write(new)
    print(f"grd/grdp: {edited} files, {total} locale elements removed")
    if a.apply:
        import xml.etree.ElementTree as ET
        for rel in git_ls(src, "*.grd", "*.grdp"):
            try:
                root = ET.parse(os.path.join(src, rel)).getroot()
            except ET.ParseError:
                continue
            for el in root.iter("if"):
                kids = [c.tag for c in el]
                if ("then" in kids or "else" in kids) and kids != ["then", "else"]:
                    print(f"FIX by hand: {rel}: <if expr={el.get('expr')!r}> children {kids}")

    # 2. xtb files
    xtbs = [p for p in git_ls(src, "*.xtb") if os.path.basename(p) not in remaining_xtb]
    print(f"xtb: {len(xtbs)} files to delete ({len(remaining_xtb)} still referenced)")
    if a.apply and xtbs:
        for i in range(0, len(xtbs), 500):
            subprocess.run(["git", "-C", src, "rm", "-q", "--", *xtbs[i:i + 500]], check=True)
    gone = set(xtbs)
    tracked = set(git_ls(src, *sorted(dropped_files))) if dropped_files else set()
    if tracked:
        print(f"per-language include files: {len(tracked)} to delete")
        if a.apply:
            subprocess.run(["git", "-C", src, "rm", "-q", "--", *sorted(tracked)], check=True)

    # 3. gritdeps
    for rel in git_ls(src, "*.gritdeps"):
        path = os.path.join(src, rel)
        with open(path, encoding="utf-8") as f:
            lines = f.readlines()
        # paths are relative to the src root
        keep_lines = [l for l in lines if l.strip() not in gone]
        if len(keep_lines) != len(lines):
            print(f"gritdeps: {rel}: -{len(lines) - len(keep_lines)} lines")
            if a.apply:
                with open(path, "w", encoding="utf-8") as f:
                    f.writelines(keep_lines)

    # 4. leftovers to fix by hand
    out = subprocess.run(["git", "-C", src, "grep", "-l", r"\.xtb", "--", "*.gn", "*.gni"],
                         capture_output=True, text=True).stdout.split()
    out = [p for p in out if not p.startswith(SKIP_PREFIXES)]
    if out:
        print("BUILD files naming .xtb (check: xtb_locales handles templated ones):")
        for p in out:
            print("  " + p)
    if not a.apply:
        print("(dry run; pass --apply)")


if __name__ == "__main__":
    sys.exit(main())
