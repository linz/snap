#!/usr/bin/env python3
"""Regenerates the SNAP application icon (snap*.ico, snap*_icon.xpm, snap_icon.svg)
from the vector design defined below. Run from anywhere; requires Pillow.

    python3 make_icon.py
"""

from __future__ import annotations

import math
import os
import string
from typing import cast

from PIL import Image, ImageDraw

RESOURCES_DIR = os.path.dirname(os.path.abspath(__file__))

# Design grid is 32 x 32 units, matching the historical icon size. This is just
# the vector coordinate space for the geometry below - actual raster output is
# produced at CANVAS resolution (see SUPERSAMPLE) and then downsampled to
# whatever pixel size is needed, so the grid size doesn't limit output quality.
CX, CY = 16.0, 15.5
ELLIPSE_A, ELLIPSE_B = 14.0, 7.5
CIRCLE_R = 3.6
BLUE_ANGLES_DEG = (320.0, 290.0, 85.0, 190.0)  # 0 = up, clockwise
RED_ANGLE_DEG = 45.0
RED_LENGTH = math.hypot(28.9 - CX, 21.2 - CY)

WHITE = (255, 255, 255, 255)
BLACK = (0, 0, 0, 255)
GREEN = (30, 130, 95, 255)
BLUE = (0, 0, 220, 255)
RED = (230, 20, 20, 255)

BORDER_W = 0.8
ELLIPSE_W = 1.1
BLUE_W = 0.9
RED_W = 1.1

# 32x supersampling keeps the largest ICO frame (256px) at 4x+ oversampling
# for anti-aliasing, not just the legacy 16/32px frames.
SUPERSAMPLE = 32
CANVAS = 32 * SUPERSAMPLE

ICO_SIZES = (16, 32, 48, 256)

Point = tuple[float, float]
Color = tuple[int, int, int]

_SYMBOLS = [c for c in string.printable if c not in ' "\\\t\n\r\x0b\x0c']


def edge_point(theta_deg: float) -> Point:
    """Point where a ray from (CX, CY) at theta_deg (0 = up, clockwise) exits the 32x32 square."""
    theta = math.radians(theta_deg)
    dx, dy = math.sin(theta), -math.cos(theta)
    candidates = []
    if dx > 0:
        candidates.append((32 - CX) / dx)
    elif dx < 0:
        candidates.append((0 - CX) / dx)
    if dy > 0:
        candidates.append((32 - CY) / dy)
    elif dy < 0:
        candidates.append((0 - CY) / dy)
    t = min(candidates)
    return (CX + dx * t, CY + dy * t)


def red_endpoint() -> Point:
    """Point RED_LENGTH units from (CX, CY) at RED_ANGLE_DEG (0 = up, clockwise)."""
    theta = math.radians(RED_ANGLE_DEG)
    dx, dy = math.sin(theta), -math.cos(theta)
    return (CX + dx * RED_LENGTH, CY + dy * RED_LENGTH)


def render_master() -> Image.Image:
    """Draws the icon once at CANVAS resolution; every output format downsamples from this."""
    im = Image.new("RGBA", (CANVAS, CANVAS), WHITE)
    draw = ImageDraw.Draw(im)

    def scale(point: Point) -> Point:
        x, y = point
        return (x * SUPERSAMPLE, y * SUPERSAMPLE)

    draw.ellipse(
        [
            scale((CX - ELLIPSE_A, CY - ELLIPSE_B)),
            scale((CX + ELLIPSE_A, CY + ELLIPSE_B)),
        ],
        outline=GREEN,
        width=round(ELLIPSE_W * SUPERSAMPLE),
    )

    for theta in BLUE_ANGLES_DEG:
        draw.line(
            [scale((CX, CY)), scale(edge_point(theta))],
            fill=BLUE,
            width=round(BLUE_W * SUPERSAMPLE),
            joint="curve",
        )

    draw.ellipse(
        [scale((CX - CIRCLE_R, CY - CIRCLE_R)), scale((CX + CIRCLE_R, CY + CIRCLE_R))],
        fill=BLACK,
    )

    draw.line(
        [scale((CX, CY)), scale(red_endpoint())],
        fill=RED,
        width=round(RED_W * SUPERSAMPLE),
        joint="curve",
    )

    # Border last, so it overlays the blue line ends rather than being drawn over by them.
    draw.rectangle(
        [0, 0, CANVAS - 1, CANVAS - 1],
        outline=BLACK,
        width=round(BORDER_W * SUPERSAMPLE),
    )

    return im


def render(size: int) -> Image.Image:
    """Downsamples the master render to a specific pixel size with anti-aliasing.

    The border is proportioned for the 32-unit design grid (see comment above), so at
    small sizes it downsamples to well under 1px and disappears into a grey wash instead
    of a crisp line. Stamping a >=1px outline on top fixes that; at sizes where the
    proportional border is already >=1px this just re-draws the same black pixels.
    """
    im = render_master().resize((size, size), Image.Resampling.LANCZOS)
    border_px = max(1, round(BORDER_W / 32 * size))
    ImageDraw.Draw(im).rectangle(
        [0, 0, size - 1, size - 1], outline=BLACK, width=border_px
    )
    return im


def write_svg(path: str) -> None:
    """Writes a vector copy of the design, for viewing/editing outside of this script."""
    lines = [
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32" width="256" height="256">'
    ]
    lines.append('  <rect x="0" y="0" width="32" height="32" fill="white"/>')
    lines.append(
        f'  <ellipse cx="{CX}" cy="{CY}" rx="{ELLIPSE_A}" ry="{ELLIPSE_B}" '
        f'fill="none" stroke="#1e825f" stroke-width="{ELLIPSE_W}"/>'
    )
    lines.append(
        f'  <g stroke="#0000dc" stroke-width="{BLUE_W}" stroke-linecap="round">'
    )
    for theta in BLUE_ANGLES_DEG:
        ex, ey = edge_point(theta)
        lines.append(f'    <line x1="{CX}" y1="{CY}" x2="{ex:.3f}" y2="{ey:.3f}"/>')
    lines.append("  </g>")
    lines.append(f'  <circle cx="{CX}" cy="{CY}" r="{CIRCLE_R}" fill="#000000"/>')
    rx, ry = red_endpoint()
    lines.append(
        f'  <line x1="{CX}" y1="{CY}" x2="{rx:.3f}" y2="{ry:.3f}" '
        f'stroke="#e61414" stroke-width="{RED_W}" stroke-linecap="round"/>'
    )
    lines.append(
        f'  <rect x="0.4" y="0.4" width="31.2" height="31.2" '
        f'fill="none" stroke="black" stroke-width="{BORDER_W}"/>'
    )
    lines.append("</svg>")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def write_ico(path: str, sizes: tuple[int, ...] = ICO_SIZES) -> None:
    """Writes a multi-resolution .ico so Windows doesn't have to upscale a single small frame
    for Explorer's large/jumbo icon views or the desktop shortcut.

    Each size is rendered independently (rather than letting Pillow auto-downscale one
    base image) so the per-size border fix in render() applies to every embedded frame.
    Pillow's ICO writer requires the image .save() is called on to be the largest
    requested size - anything bigger than it is silently dropped - so frames are sorted
    largest-first before the biggest is used as the primary image.
    """
    frames = [render(s) for s in sorted(sizes, reverse=True)]
    frames[0].save(path, sizes=[(s, s) for s in sizes], append_images=frames[1:])


def _build_palette(pixels: list[list[Color]]) -> tuple[dict[Color, str], int]:
    """Assigns one XPM symbol per unique color, using 2 chars/pixel only if 1 isn't enough."""
    colors: dict[Color, str] = {}
    for row in pixels:
        for rgb in row:
            colors.setdefault(rgb, "")
    chars_per_pixel = 1 if len(colors) <= len(_SYMBOLS) else 2
    symbols = (
        _SYMBOLS
        if chars_per_pixel == 1
        else [a + b for a in _SYMBOLS for b in _SYMBOLS]
    )
    for i, rgb in enumerate(colors):
        colors[rgb] = symbols[i]
    return colors, chars_per_pixel


def _xpm_color_table(palette: dict[Color, str], chars_per_pixel: int) -> list[str]:
    """One "<symbol> c #RRGGBB" line per palette entry, in the XPM color-table format."""
    lines = []
    for rgb, sym in palette.items():
        r, g, b = rgb
        lines.append(f'"{sym.ljust(chars_per_pixel)} c #{r:02X}{g:02X}{b:02X}",')
    return lines


def _xpm_pixel_rows(pixels: list[list[Color]], palette: dict[Color, str]) -> list[str]:
    """One quoted, comma-terminated string per pixel row, in the XPM pixel format."""
    row_strings = ["".join(palette[rgb] for rgb in row) for row in pixels]
    return [
        f'"{pixel_str}"{"," if i < len(row_strings) - 1 else ""}'
        for i, pixel_str in enumerate(row_strings)
    ]


def write_xpm(path: str, size: int, var_name: str) -> None:
    """Writes an XPM copy of the icon: wxWidgets' wxICON() macro loads .ico resources on
    Windows but expects a compiled-in XPM array (matched by var_name) on other platforms."""
    im = render(size).convert("RGB")
    pixels: list[list[Color]] = [
        [cast(Color, im.getpixel((x, y))) for x in range(size)] for y in range(size)
    ]
    palette, chars_per_pixel = _build_palette(pixels)

    lines = [
        "/* XPM */",
        f"static const char *{var_name}[] = {{",
        "/* columns rows colors chars-per-pixel */",
        f'"{size} {size} {len(palette)} {chars_per_pixel} ",',
        *_xpm_color_table(palette, chars_per_pixel),
        "/* pixels */",
        *_xpm_pixel_rows(pixels, palette),
        "};",
    ]

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def main() -> None:
    """Regenerates every checked-in icon asset from the design at the top of this file."""
    write_svg(os.path.join(RESOURCES_DIR, "snap_icon.svg"))
    # snap.ico is shared by the ICO_SNAP16 and ICO_SNAP32 .rc resource names (see the
    # snap_manager/snapadjust/snapplot .rc files) - both need the full 16/32/48/256 set,
    # so one multi-res file covers both instead of two identical copies.
    write_ico(os.path.join(RESOURCES_DIR, "snap.ico"))
    write_xpm(os.path.join(RESOURCES_DIR, "snap16_icon.xpm"), 16, "ICO_SNAP16_xpm")
    write_xpm(os.path.join(RESOURCES_DIR, "snap32_icon.xpm"), 32, "ICO_SNAP32_xpm")


if __name__ == "__main__":
    main()
