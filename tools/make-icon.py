#!/usr/bin/env python3
"""Regenerate the Oligarchy Tanks application icon.

Everything in the icon comes from the game itself, so it never drifts from
the look on screen:

  - the brick palette and the course pattern: src/Main.qml (brickPart);
  - the player tank grid: src/TankSprite.qml (playerRows);
  - the tank colours: src/Main.qml (p1body / p1dark / p1light) plus the
    neutral outline grey TankSprite uses for its trace.

Outputs:
  pkgbuild/oligarchy-tanks.svg      the scalable icon (hicolor theme)
  src/icons/oligarchy-tanks.png      the same picture, embedded in the binary
                                     for the window icon

The PNG needs rsvg-convert (librsvg). Without it the SVG is still written and
the PNG is left alone.

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
OMARCHY_GREEN = "#9ece6a"

# Geometry: a 64x64 canvas, a 4 px green border, a 56x56 brick field and the
# 16x16 tank grid at 3x (48x48) centred in it.
BORDER = 4
FIELD = 64 - 2 * BORDER
TANK_SCALE = 3


def color_for(ch):
    """TankSprite's colorFor(), with the tread band in one frame."""
    if ch == "B":
        return P1_BODY
    if ch in "HMc":
        return P1_LIGHT
    if ch in "SGCD":
        return P1_DARK
    if ch == "T":
        return OUTLINE
    if ch == "t":
        return P1_BODY
    return None


def tank_svg():
    """The tank, drawn from the game's 16x16 grid, scaled and centred."""
    size = 16 * TANK_SCALE
    offset = (64 - size) // 2

    out = []
    for row in range(16):
        run_color = None
        run_start = 0
        for col in range(17):
            color = color_for(GRID[row][col]) if col < 16 else None
            if color != run_color:
                if run_color is not None:
                    out.append(
                        f'    <rect x="{offset + run_start * TANK_SCALE}" '
                        f'y="{offset + row * TANK_SCALE}" '
                        f'width="{(col - run_start) * TANK_SCALE}" height="{TANK_SCALE}" '
                        f'fill="{run_color}"/>'
                    )
                run_color = color
                run_start = col
    return "\n".join(out)


def brick_svg():
    """The brickwork: four courses, offset joints, highlight and shadow."""
    out = []
    course_height = 13
    for i in range(4):
        y = BORDER + i * (course_height + 1)
        if i % 2 == 0:
            bricks = [(BORDER, 27), (BORDER + 28, 27)]
        else:
            bricks = [(BORDER, 13), (BORDER + 14, 27), (BORDER + 42, 13)]
        for x, width in bricks:
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


def build_svg():
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <!-- The Omarchy green border, the same #9ece6a the game uses everywhere. -->
  <rect width="64" height="64" rx="7" fill="{OMARCHY_GREEN}"/>
  <clipPath id="field">
    <rect x="{BORDER}" y="{BORDER}" width="{FIELD}" height="{FIELD}" rx="3"/>
  </clipPath>
  <g clip-path="url(#field)">
    <rect x="{BORDER}" y="{BORDER}" width="{FIELD}" height="{FIELD}" fill="{MORTAR}"/>
{brick_svg()}
  </g>
  <!-- The P1 tank, drawn from the game's own 16x16 grid. -->
  <g>
{tank_svg()}
  </g>
</svg>
"""


def main():
    svg_path = os.path.join(ROOT, "pkgbuild", "oligarchy-tanks.svg")
    with open(svg_path, "w", encoding="utf-8") as handle:
        handle.write(build_svg())
    print(f"wrote {svg_path}")

    if shutil.which("rsvg-convert") is None:
        print("rsvg-convert not found: the PNG was left alone")
        return 0

    png_dir = os.path.join(ROOT, "src", "icons")
    os.makedirs(png_dir, exist_ok=True)
    png_path = os.path.join(png_dir, "oligarchy-tanks.png")
    subprocess.run(
        ["rsvg-convert", "-w", "256", "-h", "256", svg_path, "-o", png_path], check=True
    )
    print(f"wrote {png_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
