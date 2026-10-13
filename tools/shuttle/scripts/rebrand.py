#!/usr/bin/env python3
"""Rebrand the fork as Shuttle (docs/plans/REBRAND.md, _docs/plans/REBRAND-SHUTTLE.md). Idempotent; rerun after upstream merges.

  scripts/rebrand.py assets            render icon/logo assets into assets/build/tree/ (needs rsvg-convert, magick)
  scripts/rebrand.py apply [--apply]   copy assets + text edits into $SHUTTLE_ROOT/src (dry run without --apply),
                                       compile Assets.car + app.icns (macOS, --apply only)

Never run `apply --apply` while a build compiles from $SHUTTLE_ROOT/src.
"""
import argparse, glob, os, re, shutil, subprocess, sys, tempfile
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS  # noqa: E402

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = _SHUTTLE_SRC
BR = os.path.join(TOOLS, "assets")
MARK = os.path.join(BR, "v2", "Shuttle.svg")
BUILD = os.path.join(BR, "build")
TREE = os.path.join(BUILD, "tree")

NAME, NAME_LONG, NAME_LC, COMPANY = "Shuttle", "Shuttle Browser", "shuttle", "Silkweave"
BUNDLE_ID, TEAM_ID, CREATOR = "dev.silkweave.shuttle", "TT5SR2JM9L", "Shtl"
HOMEPAGE = "https://www.shuttlebrowser.com"
# Earlier brands: their text counts as "old" too, so `apply` renames an already-branded tree.
PREV = []  # dict(NAME=, NAME_LC=, BUNDLE_ID=, CREATOR=) per earlier brand
THEME = "chrome/app/theme"
MACDIR = f"{THEME}/chromium/mac"


def run(*cmd, **kw):
    subprocess.run(cmd, check=True, **kw)


def render(svg, size, out):
    os.makedirs(os.path.dirname(out), exist_ok=True)
    run("rsvg-convert", "-w", str(size), "-h", str(size), svg, "-o", out)


# ----------------------------------------------------------------------------- assets
def assets():
    run(sys.executable, os.path.join(BR, "make-icons.py"))
    shutil.rmtree(TREE, ignore_errors=True)
    t = lambda p: os.path.join(TREE, p)
    icon = os.path.join(BUILD, "app-icon.svg")
    # product logos (mark only)
    for s in (16, 24, 48, 64, 128, 256):
        render(MARK, s, t(f"{THEME}/chromium/product_logo_{s}.png"))
    for s in (24, 48, 64, 128, 256):
        render(MARK, s, t(f"{THEME}/chromium/linux/product_logo_{s}.png"))
    for scale, d in ((1, "default_100_percent"), (2, "default_200_percent")):
        for s in (16, 32):
            render(MARK, s * scale, t(f"{THEME}/{d}/chromium/product_logo_{s}.png"))
        if scale == 1:
            for s in (16, 32):
                render(MARK, s, t(f"{THEME}/{d}/chromium/linux/product_logo_{s}.png"))
        # wordmark (mark + name), 22 px tall at 1x
        for suffix, color in (("", "#202124"), ("_white", "#ffffff")):
            wordmark(22 * scale, color, t(f"{THEME}/{d}/chromium/product_logo_name_22{suffix}.png"))
    # status tray icon: monochrome silhouette (template-style, black on transparent)
    mono = t(f"{THEME}/chromium/product_logo_22_mono.png")
    render(MARK, 22, mono)
    run("magick", mono, "-alpha", "extract", "-background", "black", "-alpha", "shape", mono)
    shutil.copy(MARK, t(f"{THEME}/chromium/product_logo.svg"))
    os.makedirs(t("ui/webui/resources/images"), exist_ok=True)
    shutil.copy(MARK, t("ui/webui/resources/images/chrome_logo_dark.svg"))  # cr-toolbar logo in dark mode
    # macOS: legacy (<26) app icon bitmaps, Icon Composer document (26+), icns
    appset = t(f"{MACDIR}/Assets.xcassets/AppIcon.appiconset")
    for s in (16, 32, 64, 128, 256, 512, 1024):
        render(icon, s, os.path.join(appset, f"appicon_{s}.png"))
    iconset = os.path.join(BUILD, "app.iconset")
    shutil.rmtree(iconset, ignore_errors=True)
    for s in (16, 32, 128, 256, 512):
        render(icon, s, os.path.join(iconset, f"icon_{s}x{s}.png"))
    run("iconutil", "-c", "icns", iconset, "-o", t(f"{MACDIR}/app.icns"))
    # document badge icon ("Icon" in the asset catalog, UTTypeIconBadgeName): the bare mark
    for name, s in (("icon_256x256.png", 256), ("icon_256x256@2x.png", 512)):
        render(MARK, s, t(f"{MACDIR}/Assets.xcassets/Icon.iconset/{name}"))
    icon_composer(t(f"{MACDIR}/AppIcon.icon"))
    print("assets in", TREE)


def wordmark(h, color, out):
    w = int(h * 6.4)
    mark = re.sub(r"<svg[^>]*>", f'<svg x="0" y="0" width="{h}" height="{h}" viewBox="0 0 256 256" fill="none">',
                  open(MARK).read(), count=1)
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">' + mark +
           f'<text x="{h * 1.3:.1f}" y="{h * 0.78:.1f}" font-family="Helvetica Neue, Helvetica, Arial, sans-serif" '
           f'font-size="{h * 0.72:.1f}" font-weight="500" fill="{color}">{NAME}</text></svg>')
    with tempfile.NamedTemporaryFile("w", suffix=".svg", delete=False) as f:
        f.write(svg)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    run("rsvg-convert", f.name, "-o", out)
    run("magick", out, "-trim", "+repage", out)
    os.unlink(f.name)


def icon_composer(dst):
    """Minimal Icon Composer document: graphite background, mark layers, lens + eye on top."""
    import json
    os.makedirs(os.path.join(dst, "Assets"), exist_ok=True)
    layers = os.path.join(BUILD, "layers")
    for f in os.listdir(layers):
        shutil.copy(os.path.join(layers, f), os.path.join(dst, "Assets", f))
    lay = lambda n, glass=False: {"image-name": f"{n}.svg", "name": n, "glass": glass}
    doc = {
        "fill": {"linear-gradient": ["srgb:0.10980,0.14118,0.18824,1.00000", "srgb:0.04314,0.06275,0.09020,1.00000"]},
        "groups": [
            {"layers": [lay("eye"), lay("lens")], "shadow": {"kind": "neutral", "opacity": 0.5},
             "translucency": {"enabled": False, "value": 0.5}, "specular": True},
            {"layers": [lay("shine"), lay("teal", True), lay("violet", True), lay("amber", True)],
             "lighting": "combined", "shadow": {"kind": "layer-color", "opacity": 0.5},
             "translucency": {"enabled": False, "value": 0.1}, "specular": True},
        ],
        "supported-platforms": {"squares": "shared"},
    }
    with open(os.path.join(dst, "icon.json"), "w") as f:
        json.dump(doc, f, indent=2)


# ----------------------------------------------------------------------------- text edits
def edits(NAME, NAME_LC, BUNDLE_ID, CREATOR):
    """(path, [(old, new), ...]) exact substrings; each must be found unless already applied."""
    return [
        (f"{THEME}/chromium/BRANDING", [
            ("COMPANY_FULLNAME=The Chromium Authors", f"COMPANY_FULLNAME={COMPANY}"),
            ("COMPANY_SHORTNAME=The Chromium Authors", f"COMPANY_SHORTNAME={COMPANY}"),
            ("PRODUCT_FULLNAME=Chromium", f"PRODUCT_FULLNAME={NAME}"),
            ("PRODUCT_SHORTNAME=Chromium", f"PRODUCT_SHORTNAME={NAME}"),
            ("PRODUCT_INSTALLER_FULLNAME=Chromium Installer", f"PRODUCT_INSTALLER_FULLNAME={NAME} Installer"),
            ("PRODUCT_INSTALLER_SHORTNAME=Chromium Installer", f"PRODUCT_INSTALLER_SHORTNAME={NAME} Installer"),
            ("The Chromium Authors. All rights reserved.", f"{COMPANY}. All rights reserved."),
            ("MAC_BUNDLE_ID=org.chromium.Chromium", f"MAC_BUNDLE_ID={BUNDLE_ID}"),
            ("MAC_CREATOR_CODE=Cr24", f"MAC_CREATOR_CODE={CREATOR}"),
            ("MAC_TEAM_ID=\n", f"MAC_TEAM_ID={TEAM_ID}\n"),
        ]),
        ("chrome/common/chrome_paths_mac.mm", [('product_dir_name = "Chromium";', f'product_dir_name = "{NAME}";')]),
        ("chrome/common/chrome_paths_linux.cc", [('data_dir_basename = "chromium";', f'data_dir_basename = "{NAME_LC}";')]),
        ("chrome/browser/mac/initial_prefs.mm", [('kInitialPreferencesDirectory[] = "Chromium";',
                                                  f'kInitialPreferencesDirectory[] = "{NAME}";')]),
        ("components/os_crypt/common/keychain_password_mac.mm", [
            ('kDefaultServiceName[] = "Chromium Safe Storage";', f'kDefaultServiceName[] = "{NAME} Safe Storage";'),
            ('kDefaultAccountName[] = "Chromium";', f'kDefaultAccountName[] = "{NAME}";')]),
        ("components/os_crypt/async/browser/freedesktop_secret_key_provider.h", [
            ('kKeyName[] = "Chromium Safe Storage";', f'kKeyName[] = "{NAME} Safe Storage";')]),
        ("chrome/common/url_constants.h", [  # Help menu, F1, settings Help: our website (non-ChromeOS)
            (f'"https://support.google.com/chrome?p=help&ctx={c}";', f'"{HOMEPAGE}";') for c in ("keyboard", "menu", "settings")]),
    ]


EDITS = edits(NAME, NAME_LC, BUNDLE_ID, CREATOR)
for _prev in PREV:  # also accept an earlier brand's text as "old"
    for (path, reps), (_, prev_reps) in zip(EDITS, edits(**_prev)):
        reps[:0] = [(pn, n) for (_, n), (_, pn) in zip(list(reps), prev_reps) if pn != n]
# (grd, [files whose <message> bodies get rebranded])
STRING_GROUPS = [
    ("chrome/app/chromium_strings.grd", ["chrome/app/chromium_strings.grd", "chrome/app/settings_chromium_strings.grdp"]),
    ("components/components_chromium_strings.grd", ["components/components_chromium_strings.grd"]),
    ("components/components_strings.grd", [f"components/{n}_strings.grdp" for n in (
        "management", "new_or_sad_tab", "page_info", "search_engine_choice", "security_interstitials", "ssl_errors")]),
    ("components/privacy_sandbox_strings.grd", ["components/privacy_sandbox_strings.grd"]),
    ("extensions/strings/extensions_strings.grd", ["extensions/strings/extensions_chromium_strings.grdp"]),
]
STRING_FILES = [f for _, fs in STRING_GROUPS for f in fs]
NAME_RE = re.compile(r"Chromium(?!OS|Updater)")
PH_RE = re.compile(r'(<ph\b[^>]*/>|<ph\b.*?</ph>)', re.S)
KEEP_AFTER = ("BEGIN_LINK_CHROMIUM",)  # link text "Chromium" (the open source project) stays


def rebrand_message(t):
    """Rebrand text outside <ph> placeholders; the word right after a KEEP_AFTER placeholder stays."""
    out, keep = [], False
    for part in PH_RE.split(t):
        if PH_RE.fullmatch(part):
            out.append(part)
            keep = any(f'name="{k}"' in part for k in KEEP_AFTER)
            continue
        if keep and part.startswith("Chromium"):
            out.append("Chromium" + rebrand_text(part[len("Chromium"):]))
        else:
            out.append(rebrand_text(part))
        keep = False
    return "".join(out)


def rebrand_text(t):
    for p in PREV:
        t = t.replace(p["NAME"], NAME)
    return NAME_RE.sub(NAME, t.replace("The Chromium Authors", COMPANY))


def text_edits(apply):
    changed = 0
    for path, reps in EDITS:
        p = os.path.join(SRC, path)
        s = orig = open(p).read()
        for old, new in reps:
            s = s.replace(old, new)
        for old, new in reps:
            if new not in s:
                sys.exit(f"ERROR {path}: neither old nor new text found: {old!r}")
        if s != orig:
            changed += 1
            print("edit", path)
            if apply:
                open(p, "w").write(s)
    return changed


def message_bodies(text):
    """Rebrand only <message> bodies (not desc attributes, comments or conditions)."""
    def fix(m):
        return m.group(1) + rebrand_message(m.group(2)) + m.group(3)
    return re.sub(r"(<message\b[^>]*>)(.*?)(</message>)", fix, text, flags=re.S)


def strings(apply):
    """Rebrand <message> bodies. No .xtb remap: the fork is en-US only (scripts/prune-locales.py runs first)."""
    total = 0
    for _, files in STRING_GROUPS:
        for f in files:
            p = os.path.join(SRC, f)
            orig = open(p).read()
            new = message_bodies(orig)
            if new != orig:
                total += 1
                print("strings", f)
                if apply:
                    open(p, "w").write(new)
    if not total:
        print("strings: already rebranded")
    return total


def product_glyph():
    """Monochrome mark for the cr-icon "chrome-product" (Settings sidebar "About"): the three segments at
    different opacities + the ring, all currentColor."""
    svg = open(MARK).read()
    paths = re.findall(r'<path\b[^>]*\bd="([^"]+)"', svg)
    seg = "".join(f'<path d="{d}" opacity="{o}"></path>' for d, o in zip(paths[:3], ("1", "0.7", "0.45")))
    ring = (f'<path d="{paths[6]}" fill="none" stroke="currentColor" stroke-width="12"></path>'
            if len(paths) > 6 else "")
    return f'<g id="chrome-product" viewBox="0 0 256 256">{seg}{ring}</g>'


def glyph_edits(apply):
    p = os.path.join(SRC, "ui/webui/resources/cr_elements/icons.html.ts")
    s = orig = open(p).read()
    s = re.sub(r'<g id="chrome-product"[^>]*>.*?</g>', lambda m: product_glyph(), s, flags=re.S)
    if s != orig:
        print("edit ui/webui/resources/cr_elements/icons.html.ts (chrome-product glyph)")
        if apply:
            open(p, "w").write(s)


def copy_assets(apply):
    if not os.path.isdir(TREE):
        sys.exit("run `scripts/rebrand.py assets` first")
    n = 0
    for dp, _, fs in os.walk(TREE):
        for f in fs:
            rel = os.path.relpath(os.path.join(dp, f), TREE)
            n += 1
            if apply:
                os.makedirs(os.path.dirname(os.path.join(SRC, rel)), exist_ok=True)
                shutil.copy(os.path.join(TREE, rel), os.path.join(SRC, rel))
    if apply:  # drop Chromium's Icon Composer layers that the new document doesn't reference
        keep = set(os.listdir(os.path.join(TREE, MACDIR, "AppIcon.icon", "Assets")))
        a = os.path.join(SRC, MACDIR, "AppIcon.icon", "Assets")
        for f in os.listdir(a):
            if f not in keep:
                os.unlink(os.path.join(a, f))
        run(sys.executable, os.path.join(SRC, "tools/mac/icons/compile_car.py"),
            os.path.join(SRC, MACDIR, "Assets.xcassets"))
    print(f"assets: {n} files" + (" copied, Assets.car compiled" if apply else " to copy"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["assets", "apply"])
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()
    if a.cmd == "assets":
        assets()
    else:
        text_edits(a.apply)
        glyph_edits(a.apply)
        strings(a.apply)
        copy_assets(a.apply)
        if not a.apply:
            print("dry run; rerun with --apply")


if __name__ == "__main__":
    main()
