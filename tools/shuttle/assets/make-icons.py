#!/usr/bin/env python3
"""Build Shuttle icon artwork from the v2 mark (assets/v2/Shuttle.svg, 2026-10-08).

Outputs (assets/build/, git-ignored):
  app-icon.svg          macOS app icon: graphite squircle (Apple grid) + v2 mark + lens and glowing eye
  layers/*.svg          Icon Composer layers (1024 canvas): amber, violet, teal, shine, lens, eye
Render previews with rsvg-convert. Asset sets (icns, PNGs, Assets.car) come from scripts/rebrand.py.
"""
import os, re

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "v2", "Shuttle.svg")
OUT = os.path.join(HERE, "build")

# v2 mark: 256 viewBox, disc fills it; lens ring centre (128.19, 128), ring radius ~55.
MARK_D = 660.0                  # mark diameter on the 1024 icon canvas (same as v1)
SCALE = MARK_D / 256.0
OFF = (1024 - MARK_D) / 2
LC = (128.194, 128.0)           # lens centre (mark units)
LR = 52.0                       # lens fill radius, inside the ring stroke
PR = 16.5                       # pupil radius

PAL = {"lens": "#031E2D", "lens_hi": "#123447", "lens_edge": "#00131F", "pupil": "#8BFCFD",
       "glow": "#19D8F5", "sq_top": "#1C2430", "sq_bottom": "#0B1017"}


def read_mark():
    s = open(SRC).read()
    inner = re.search(r"<svg[^>]*>(.*)</svg>", s, re.S).group(1)
    body, defs = inner.split("<defs>", 1)
    body = body.strip()
    m = re.match(r'<g clip-path="[^"]*">(.*)</g>$', body, re.S)  # drop the full-canvas clip group
    if m:
        body = m.group(1)
    defs = defs.rsplit("</defs>", 1)[0]
    return body, defs


def lens_defs():
    return (f'<radialGradient id="ac-lens" cx="{LC[0]-12:.1f}" cy="{LC[1]-14:.1f}" r="{LR*1.25:.1f}" gradientUnits="userSpaceOnUse">'
            f'<stop offset="0" stop-color="{PAL["lens_hi"]}"/><stop offset=".7" stop-color="{PAL["lens"]}"/>'
            f'<stop offset="1" stop-color="{PAL["lens_edge"]}"/></radialGradient>'
            f'<radialGradient id="ac-pupil"><stop offset="0" stop-color="#E8FFFF"/>'
            f'<stop offset=".55" stop-color="{PAL["pupil"]}"/><stop offset="1" stop-color="{PAL["glow"]}"/></radialGradient>'
            '<filter id="ac-glow" x="-100%" y="-100%" width="300%" height="300%"><feGaussianBlur stdDeviation="4"/></filter>'
            '<filter id="ac-shadow" x="-20%" y="-20%" width="140%" height="140%">'
            '<feDropShadow dx="0" dy="5" stdDeviation="6" flood-color="#000" flood-opacity=".5"/></filter>'
            f'<linearGradient id="ac-sq" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{PAL["sq_top"]}"/>'
            f'<stop offset="1" stop-color="{PAL["sq_bottom"]}"/></linearGradient>')


def lens_eye():
    x, y = LC
    return (f'<circle cx="{x}" cy="{y}" r="{LR}" fill="url(#ac-lens)"/>'
            f'<circle cx="{x}" cy="{y}" r="{LR*0.6:.1f}" fill="none" stroke="#1B4A60" stroke-width="1.2" opacity=".8"/>'
            f'<circle cx="{x}" cy="{y}" r="{PR*1.8:.1f}" fill="{PAL["glow"]}" opacity=".55" filter="url(#ac-glow)"/>'
            f'<circle cx="{x}" cy="{y}" r="{PR}" fill="url(#ac-pupil)"/>')


def wrap(content, defs):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">'
            f'<defs>{defs}</defs>{content}</svg>\n')


# Icon Composer: the whole 1024 canvas is the tile (the system masks and insets it), so the mark is
# drawn larger there than on the legacy 824 px squircle (same mark-to-tile ratio as Chrome's icon).
LAYER_D = 800.0


def place(inner, d=MARK_D):
    off, scale = (1024 - d) / 2, d / 256.0
    return f'<g fill="none" transform="translate({off} {off}) scale({scale:.6f})">{inner}</g>'


def main():
    os.makedirs(os.path.join(OUT, "layers"), exist_ok=True)
    body, defs = read_mark()
    # lens + eye go under the ring strokes (the mask/ring paths follow the shine ellipse)
    i = body.index("<mask")
    mark = body[:i] + lens_eye() + body[i:]
    sq = ('<rect x="100" y="100" width="824" height="824" rx="185" fill="url(#ac-sq)"/>'
          '<rect x="101" y="101" width="822" height="822" rx="184" fill="none" stroke="#fff" stroke-opacity=".08" stroke-width="2"/>')
    open(os.path.join(OUT, "app-icon.svg"), "w").write(
        wrap(sq + f'<g filter="url(#ac-shadow)">{place(mark)}</g>', defs + lens_defs()))
    # Icon Composer layers: one element group per layer, same placement
    paths = re.findall(r"<path[^>]*/>", body[:i])
    names = ["amber", "violet", "teal", "shine"]
    for name, p in zip(names, paths):
        open(os.path.join(OUT, "layers", f"{name}.svg"), "w").write(wrap(place(p, LAYER_D), defs))
    open(os.path.join(OUT, "layers", "lens.svg"), "w").write(
        wrap(place(f'<circle cx="{LC[0]}" cy="{LC[1]}" r="{LR}" fill="url(#ac-lens)"/>' + body[i:], LAYER_D), defs + lens_defs()))
    open(os.path.join(OUT, "layers", "eye.svg"), "w").write(
        wrap(place(f'<circle cx="{LC[0]}" cy="{LC[1]}" r="{PR}" fill="url(#ac-pupil)"/>', LAYER_D), lens_defs()))
    print("wrote", OUT)


if __name__ == "__main__":
    main()
