#!/usr/bin/env python3
"""Generate the Erethea Engine branding assets (SVG, PNG and ICO).

The generated files live in `erethea/branding/` and are committed, so building
the engine never needs this script. Run it again only when the logo changes:

    pip install fonttools brotli pillow
    python erethea/tools/make_branding.py
    prek run svgo --files erethea/branding/*.svg   # Same SVG optimization as the CI.
    python erethea/tools/rebrand.py                # Copy the new files into the engine.

Rasterizing the SVGs needs Node.js with Playwright and Chromium
(`npm install playwright && npx playwright install chromium`).

The wordmark is drawn from Inter Bold/Regular glyph outlines (SIL OFL 1.1,
shipped in `thirdparty/fonts/`), converted to paths so no font is needed at
runtime.
"""

from __future__ import annotations

import io
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BRANDING = ROOT / "erethea" / "branding"
RASTERIZE_JS = Path(__file__).resolve().parent / "rasterize.mjs"

# Brand colors: monochrome, so the logo sits well on dark and light themes.
INK = "#0b0b0c"
PAPER = "#f4f4f4"
TEXT = "#e0e0e0"  # Converted to a dark gray by the editor's light theme.
SUBTEXT = "#a8a8a8"

# Emblem: an "E" inside a thick ring, on a black disc that keeps it visible on
# light backgrounds. Drawn in a 256x256 box. Three variants were proposed and
# variant C (modern sans-serif "E" with forward-slanted cuts) was chosen;
# EMBLEM_VARIANT picks the one in use.
E_SERIF = (
    "M76 70H162C170 70 176 63 180 48C184 66 180 82 172 92C170 86 166 82 158 82H122V122H140C146 122 150 118 152 111"
    "C155 122 155 134 153 145C151 139 147 134 140 134H122V172H166C174 172 181 163 186 146C190 164 187 179 180 190"
    "H76C82 188 86 184 88 178V82C86 75 82 71 76 70Z"
)
E_SANS = "M84 78L92 70H178L170 92H114V119H162L155 139H114V168H182L174 190H84Z"
EMBLEMS = {
    # A: serif "E", thick ring flush with the edge.
    "A": (117, 14, "translate(-3 8)", E_SERIF),
    # B: serif "E", thin ring inset from the edge.
    "B": (108, 9, "matrix(.88 0 0 .88 12.5 22)", E_SERIF),
    # C: modern sans-serif "E" with slanted cuts, thick ring.
    "C": (117, 14, "translate(-5 -2)", E_SANS),
}
EMBLEM_VARIANT = "C"


def emblem(x: float, y: float, size: float) -> str:
    """Ring with the "E", placed at (x, y) with the given size."""
    ring_r, ring_w, letter_transform, letter = EMBLEMS[EMBLEM_VARIANT]
    s = size / 256
    return (
        f'<g transform="translate({x:g} {y:g})scale({s:g})">'
        f'<circle cx="128" cy="128" r="124" fill="{INK}"/>'
        f'<circle cx="128" cy="128" r="{ring_r}" fill="none" stroke="{PAPER}" stroke-width="{ring_w}"/>'
        f'<path fill="{PAPER}" transform="{letter_transform}" d="{letter}"/>'
        "</g>"
    )


def console_badge() -> str:
    """Small terminal badge for the console executable icon (128x128 box)."""
    return (
        f'<rect width="58" height="44" x="68" y="82" fill="{PAPER}" stroke="{INK}" stroke-width="4" rx="8"/>'
        f'<path fill="none" stroke="{INK}" stroke-linecap="round" stroke-linejoin="round" stroke-width="6" '
        'd="m80 94 9 8-9 8m15 1h16"/>'
    )


def text_path(
    text: str, font_file: Path, cap_height_px: float, x: float, baseline: float, tracking: float = 0
) -> tuple[str, float]:
    """Return SVG path data for `text` (cap height scaled to `cap_height_px`) and its width in pixels."""
    from fontTools.pens.svgPathPen import SVGPathPen
    from fontTools.pens.transformPen import TransformPen
    from fontTools.ttLib import TTFont

    font = TTFont(io.BytesIO(font_file.read_bytes()))
    glyph_set = font.getGlyphSet()
    cmap = font.getBestCmap()
    cap_height = getattr(font["OS/2"], "sCapHeight", 0) or font["head"].unitsPerEm * 0.7
    scale = cap_height_px / cap_height
    hmtx = font["hmtx"]

    pen = SVGPathPen(glyph_set, lambda v: f"{v:.2f}".rstrip("0").rstrip("."))
    cursor = 0.0
    for char in text:
        name = cmap[ord(char)]
        # Flip Y: font units go up, SVG goes down.
        transform = (scale, 0, 0, -scale, x + cursor * scale, baseline)
        glyph_set[name].draw(TransformPen(pen, transform))
        cursor += hmtx[name][0] + tracking
    width = (cursor - tracking) * scale
    return pen.getCommands(), width


def svg(width: int, height: int, body: str) -> str:
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">{body}</svg>\n'


def lockup(word_cap: float, sub_cap: float, x: float, word_base: float, sub_base: float, fonts: Path):
    """Return the "ERETHEA" wordmark, the smaller "ENGINE" line below it, and the wordmark width."""
    word, word_w = text_path("ERETHEA", fonts / "Inter_Bold.woff2", word_cap, x, word_base, tracking=60)
    sub, _ = text_path("ENGINE", fonts / "Inter_Regular.woff2", sub_cap, x + 0.5, sub_base, tracking=480)
    return word, sub, word_w


def build_svgs() -> dict[str, str]:
    fonts = ROOT / "thirdparty" / "fonts"
    files: dict[str, str] = {}

    # App icon, also the default icon of new projects.
    files["icon.svg"] = svg(128, 128, emblem(0, 0, 128))
    files["icon_console.svg"] = svg(128, 128, emblem(0, 0, 128) + console_badge())

    # 16x16 icon for the editor's "About" menu entry.
    files["editor_icon_16.svg"] = svg(16, 16, emblem(0, 0, 16))

    # Full logo for the About dialog and credits (replaces editor/icons/Logo.svg).
    word, sub, word_w = lockup(22, 9, 70, 33, 52, fonts)
    files["logo.svg"] = svg(
        round(70 + word_w + 4),
        64,
        emblem(2, 4, 56) + f'<path fill="{TEXT}" d="{word}"/><path fill="{TEXT}" d="{sub}"/>',
    )

    # Project manager title bar logo (replaces editor/icons/TitleBarLogo.svg).
    word_small, small_w = text_path("ERETHEA", fonts / "Inter_Bold.woff2", 10, 26, 17, tracking=60)
    files["title_bar_logo.svg"] = svg(
        round(26 + small_w + 2),
        24,
        emblem(1, 2, 20) + f'<path fill="{TEXT}" d="{word_small}"/>',
    )

    # Boot splash: logo centered on a transparent 800x600 canvas.
    _, _, splash_w = lockup(40, 15, 0, 0, 0, fonts)
    word_splash, _, _ = lockup(40, 15, (800 - splash_w) / 2, 412, 446, fonts)
    _, sub_w = text_path("ENGINE", fonts / "Inter_Regular.woff2", 15, 0, 0, tracking=480)
    sub_splash, _ = text_path("ENGINE", fonts / "Inter_Regular.woff2", 15, (800 - sub_w) / 2, 446, tracking=480)
    files["splash.svg"] = svg(
        800,
        600,
        emblem(310, 150, 180) + f'<path fill="{PAPER}" d="{word_splash}"/><path fill="{SUBTEXT}" d="{sub_splash}"/>',
    )
    return files


def rasterize(jobs: list[tuple[Path, Path, int, int]]) -> None:
    args = ["node", str(RASTERIZE_JS)]
    for src, dst, w, h in jobs:
        args += [str(src), str(dst), str(w), str(h)]
    subprocess.run(args, check=True)


def build_ico(sources: list[Path], dst: Path) -> None:
    from PIL import Image

    images = [Image.open(p).convert("RGBA") for p in sources]
    images.sort(key=lambda im: im.size[0], reverse=True)
    images[0].save(dst, format="ICO", sizes=[im.size for im in images], append_images=images[1:])


def main() -> int:
    BRANDING.mkdir(parents=True, exist_ok=True)
    for name, content in build_svgs().items():
        (BRANDING / name).write_text(content, encoding="utf-8", newline="\n")
        print(f"wrote erethea/branding/{name}")

    tmp = BRANDING / ".raster"
    tmp.mkdir(exist_ok=True)
    ico_sizes = [16, 32, 48, 64, 128, 256]
    jobs = [
        (BRANDING / "icon.svg", BRANDING / "app_icon.png", 128, 128),
        (BRANDING / "icon.svg", BRANDING / "icon_256.png", 256, 256),
        (BRANDING / "splash.svg", BRANDING / "splash.png", 800, 600),
    ]
    for size in ico_sizes:
        jobs.append((BRANDING / "icon.svg", tmp / f"icon_{size}.png", size, size))
        jobs.append((BRANDING / "icon_console.svg", tmp / f"console_{size}.png", size, size))
    rasterize(jobs)

    build_ico([tmp / f"icon_{s}.png" for s in ico_sizes], BRANDING / "erethea.ico")
    build_ico([tmp / f"console_{s}.png" for s in ico_sizes], BRANDING / "erethea_console.ico")
    for leftover in tmp.iterdir():
        leftover.unlink()
    tmp.rmdir()
    print("wrote erethea/branding/*.png and *.ico")
    return 0


if __name__ == "__main__":
    os.chdir(ROOT)
    sys.exit(main())
