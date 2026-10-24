#!/usr/bin/env python3
"""Regenerate the Oligarchy Tanks application icon.

Everything in the icon comes from the game itself, so it never drifts from
the look on screen:

  - the brick palette and the course pattern: src/Main.qml (brickPart);
  - the player tank grid: src/TankSprite.qml (playerRows);
  - the tank colours: src/Main.qml (p1body / p1dark / p1light) plus the
    neutral outline grey TankSprite uses for its trace.

One drawing is produced, the game's own tank on the game's own brickwork,
with the tread cells alternating the body colour and the outline grey by
row exactly as TankSprite draws them.

Outputs:
  pkgbuild/oligarchy-tanks.svg      the scalable icon (hicolor theme)
  src/icons/oligarchy-tanks.png      the same picture, embedded in the binary
                                     for the window icon
  pkgbuild/oligarchy-tanks.ico       Windows executable icon (multi-size)
  pkgbuild/oligarchy-tanks.icns      macOS application icon

The PNG and the ICNS need rsvg-convert (librsvg); the ICO also needs
ImageMagick. Without them the SVG is still written and the rest is left
alone.

Usage: python3 tools/make-icon.py
"""

import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The player's 16x16 grid, exactly as TankSprite draws it.
GRID = [
    "Tttt...MM...tttT",
    "Tttt...GG...tttT",
    "Tttt...GG...tttT",
    "Tttt...GG...tttT",
    "Tttt.HHHHHH.tttT",
    "TtttSBBBBBBStttT",
    "TtttSBBCCBBStttT",
    "TtttSBCCCCBStttT",
    "TtttSBBCCBBStttT",
    "TtttSSBBBBSStttT",
    "TtttSSSSSSSStttT",
    "Tttt.SSSSSS.tttT",
    "Tttt........tttT",
    "Tttt........tttT",
    "Tttt........tttT",
    "Tttt........tttT",
]

# The game's own palette.
P1_BODY = "#c8a028"
P1_DARK = "#6e5410"
P1_LIGHT = "#f0d878"
OUTLINE = "#6b7280"

BRICK = "#9e5230"
BRICK_TOP = "#b96a3e"
BRICK_BOTTOM = "#5e2f18"
MORTAR = "#2e1a10"

# The drawing: a 64x64 canvas, the brick field filling it edge to edge
# (no border of its own) and the 16x16 tank grid at 3x (48x48) centred.
CANVAS = 64
TANK_SCALE = 3


def color_for(ch, row):
    """TankSprite's colorFor(), with the tread band in one frame.

    The tread cells alternate body and outline by row, exactly as the game
    does (colourFor: (row + frame) % 2), so the band reads as a track and
    not as a flat bar.
    """
    if ch == "B":
        return P1_BODY
    if ch in "HMc":
        return P1_LIGHT
    if ch in "SGCD":
        return P1_DARK
    if ch == "T":
        return OUTLINE
    if ch == "t":
        return P1_BODY if row % 2 == 0 else OUTLINE
    return None


def tank_svg(scale, canvas, grid=GRID, color=None):
    """The tank, drawn from the game's 16x16 grid, scaled and centred."""
    size = 16 * scale
    offset = (canvas - size) // 2
    color = color or color_for

    out = []
    for row in range(16):
        run_color = None
        run_start = 0
        for col in range(17):
            color_value = color(grid[row][col], row) if col < 16 else None
            if color_value != run_color:
                if run_color is not None:
                    out.append(
                        f'    <rect x="{offset + run_start * scale}" '
                        f'y="{offset + row * scale}" '
                        f'width="{(col - run_start) * scale}" height="{scale}" '
                        f'fill="{run_color}"/>'
                    )
                run_color = color_value
                run_start = col
    return "\n".join(out)


def scaled_grid(grid, scale, size):
    """A grid where every cell became scale x scale cells of the same char."""
    out = []
    for row in grid:
        line = "".join(ch * scale for ch in row)
        for _ in range(scale):
            out.append(line)
    return out[:size] if len(out) > size else out


def brick_svg(canvas):
    """The brickwork: four courses, offset joints, highlight and shadow.

    The courses fill the canvas edge to edge; there is no border of its own.
    """
    out = []
    course_height = (canvas + 3) // 4
    gap = 1
    for i in range(4):
        y = i * (course_height + gap)
        if i % 2 == 0:
            half = (canvas - gap) // 2
            bricks = [(0, half), (half + gap, canvas - half - gap)]
        else:
            quarter = (canvas - 2 * gap) // 4
            bricks = [
                (0, quarter),
                (quarter + gap, quarter * 2),
                (quarter * 3 + gap * 2, canvas - quarter * 3 - gap * 2),
            ]
        for x, width in bricks:
            if width <= 0:
                continue
            out.append(
                f'    <rect x="{x}" y="{y}" width="{width}" height="{course_height}" fill="{BRICK}"/>'
            )
            out.append(
                f'    <rect x="{x}" y="{y}" width="{width}" height="2" fill="{BRICK_TOP}"/>'
            )
            out.append(
                f'    <rect x="{x}" y="{y + course_height - 2}" width="{width}" height="2" '
                f'fill="{BRICK_BOTTOM}"/>'
            )
    return "\n".join(out)


def build_svg(scale=TANK_SCALE, canvas=CANVAS, grid=GRID, color=None):
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {canvas} {canvas}" \
width="{canvas}" height="{canvas}">
  <clipPath id="field">
    <rect width="{canvas}" height="{canvas}"/>
  </clipPath>
  <g clip-path="url(#field)">
    <rect width="{canvas}" height="{canvas}" fill="{MORTAR}"/>
{brick_svg(canvas)}
  </g>
  <!-- The P1 tank, drawn from the game's own 16x16 grid. -->
  <g>
{tank_svg(scale, canvas, grid, color)}
  </g>
</svg>
"""


def main():
    # The scalable icon, and the picture the window icon falls back to.
    svg_path = os.path.join(ROOT, "pkgbuild", "oligarchy-tanks.svg")
    with open(svg_path, "w", encoding="utf-8") as handle:
        handle.write(build_svg())
    print(f"wrote {svg_path}")

    if shutil.which("rsvg-convert") is None:
        print("rsvg-convert not found: the PNG was left alone")
        return 0

    icons_dir = os.path.join(ROOT, "src", "icons")
    os.makedirs(icons_dir, exist_ok=True)
    window_png = os.path.join(icons_dir, "oligarchy-tanks.png")
    subprocess.run(
        ["rsvg-convert", "-w", "1024", "-h", "1024", svg_path, "-o", window_png], check=True
    )
    print(f"wrote {window_png}")

    write_icns(svg_path)
    write_ico(window_png)
    return 0


def png_at(svg_path, size, out_path):
    subprocess.run(
        ["rsvg-convert", "-w", str(size), "-h", str(size), svg_path, "-o", out_path],
        check=True,
    )
    with open(out_path, "rb") as handle:
        return handle.read()


def write_icns(svg_path):
    # ICNS is a simple container: a magic and a length, then entries of
    # (4-byte type, 4-byte length, PNG payload). Modern macOS reads the PNG
    # types, so the sizes below cover every slot the Finder asks for.
    out = os.path.join(ROOT, "pkgbuild", "oligarchy-tanks.icns")
    slots = [(b"icp4", 16), (b"icp5", 32), (b"icp6", 64), (b"ic07", 128),
             (b"ic08", 256), (b"ic09", 512), (b"ic10", 1024)]
    entries = []
    for kind, size in slots:
        tmp = os.path.join(ROOT, "build", f"icon-{size}.png")
        os.makedirs(os.path.dirname(tmp), exist_ok=True)
        data = png_at(svg_path, size, tmp)
        entries.append(kind + (8 + len(data)).to_bytes(4, "big") + data)
        os.remove(tmp)
    body = b"".join(entries)
    with open(out, "wb") as handle:
        handle.write(b"icns" + (8 + len(body)).to_bytes(4, "big") + body)
    print(f"wrote {out}")


def write_ico(png_path):
    tool = shutil.which("magick") or shutil.which("convert")
    if not tool:
        print("ImageMagick not found: the ICO was left alone")
        return
    out = os.path.join(ROOT, "pkgbuild", "oligarchy-tanks.ico")
    subprocess.run(
        [tool, png_path, "-define", "icon:auto-resize=256,128,64,48,32,16", out],
        check=True,
    )
    print(f"wrote {out}")


if __name__ == "__main__":
    sys.exit(main())
