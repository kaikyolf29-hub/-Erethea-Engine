#!/usr/bin/env python3
"""Generate the Erethea Engine branding assets (SVG, PNG, ICO and BMP).

The logo is the art in `erethea/branding/source/`: the emblem (`emblem.png`)
and the full logo with its lettering (`logo.png`; `logo_dark_background.png`
is the same logo for dark backgrounds, with the colors swapped). This script
traces the art into vectors with potrace, so the icons stay sharp at every
size, and lays it out for each place the engine shows its logo.

The generated files live in `erethea/branding/` and are committed, so building
the engine never needs this script. Run it again only when the logo changes:

    sudo apt install potrace          # or: brew install potrace / choco install potrace
    pip install pillow numpy
    python erethea/tools/make_branding.py
    prek run svgo --files erethea/branding/*.svg   # Same SVG optimization as the CI.
    python erethea/tools/rebrand.py                # Copy the new files into the engine.

Rasterizing the SVGs needs Node.js with Playwright and Chromium
(`npm install playwright && npx playwright install chromium`).
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BRANDING = ROOT / "erethea" / "branding"
SOURCE = BRANDING / "source"
RASTERIZE_JS = Path(__file__).resolve().parent / "rasterize.mjs"

# Brand colors, taken from the art.
INK = "#0b0b10"
CREAM = "#efe6d2"
TEXT = "#e0e0e0"  # Lettering inside the editor: its light theme turns this color dark gray.

# The emblem in emblem.png (1024x1024): a dark disc, two thin rings with a
# row of dots and a cross between them, and in the middle a "C"-shaped
# crescent pierced by a sword that makes it an "E".
EMBLEM_SIZE = 1024
EMBLEM_CENTER = 511.5
DISC_RADIUS = 504
DETAILS_RADIUS = 380  # The rings, dots and cross are outside it; the "E" is inside.
# In small icons the rings and dots would be a blur: one thicker ring instead,
# and a bigger "E".
SMALL_RING_RADIUS = 462
SMALL_RING_WIDTH = 40
SMALL_E_SCALE = 1.12

# Where things are in logo.png (2048x2360): the emblem, the "ERETHEA" lettering,
# and the lettering with the "ENGINE" line under it.
LOGO_EMBLEM = (280, 96, 1767, 1583)
LOGO_WORD_ROWS = (1790, 2040)
LOGO_TEXT_ROWS = (1790, 2230)

# Godot's boot splash background, Color(0.14, 0.14, 0.14).
SPLASH_BACKGROUND = "#242424"
# Inno Setup wizard images for 100%, 150% and 200% screen scaling: the emblem
# in the corner of every page, and the full logo beside the last page.
INSTALLER_SMALL_SIZES = [(55, 58), (83, 80), (110, 106)]
INSTALLER_LARGE_SIZES = [(164, 314), (328, 628)]


@dataclass
class Shape:
    """Traced path data, in the potrace units of its source image, and its box in source pixels."""

    path: str
    image_height: int
    left: float
    top: float
    right: float
    bottom: float

    @property
    def width(self) -> float:
        return self.right - self.left

    @property
    def height(self) -> float:
        return self.bottom - self.top

    def place(self, x: float, y: float, height: float, color: str) -> tuple[str, float]:
        """Draw the shape with its top left corner at (x, y), `height` pixels tall. Return it and its width."""
        k = height / self.height
        transform = (
            f"translate({x:g} {y:g})scale({k:g})translate({-self.left:g} {-self.top:g})"
            f"translate(0 {self.image_height})scale(.1 -.1)"
        )
        return f'<path fill="{color}" transform="{transform}" d="{self.path}"/>', self.width * k


def trace(mask, image_height: int) -> Shape:
    """Trace a boolean mask (True where the shape is) with potrace."""
    import numpy as np
    from PIL import Image

    ys, xs = np.nonzero(mask)
    with tempfile.TemporaryDirectory() as folder:
        bitmap = Path(folder) / "mask.pbm"
        vector = Path(folder) / "mask.svg"
        Image.fromarray(np.where(mask, 0, 255).astype(np.uint8)).convert("1").save(bitmap)
        subprocess.run(
            [
                "potrace",
                str(bitmap),
                "--svg",
                "--flat",
                "--turdsize",
                "4",
                "--alphamax",
                "1.0",
                "--opttolerance",
                "0.3",
                "--output",
                str(vector),
            ],
            check=True,
        )
        text = vector.read_text(encoding="utf-8")
    found = re.search(r'<path d="([^"]+)"', text)
    if found is None:
        raise RuntimeError("potrace wrote no path")
    path = " ".join(found.group(1).split())
    return Shape(path, mask.shape[0], float(xs.min()), float(ys.min()), float(xs.max() + 1), float(ys.max() + 1))


def trace_art() -> dict[str, Shape]:
    import numpy as np
    from PIL import Image

    emblem = np.array(Image.open(SOURCE / "emblem.png").convert("RGBA")).astype(int)
    light = (emblem[..., 0] > 128) & (emblem[..., 3] > 128)
    rows, columns = np.indices(light.shape)
    radius = np.hypot(columns - EMBLEM_CENTER, rows - EMBLEM_CENTER)

    logo = np.array(Image.open(SOURCE / "logo.png").convert("RGBA")).astype(int)
    lettering = (logo[..., 0] < 128) & (logo[..., 3] > 128)
    logo_rows = np.indices(lettering.shape)[0]

    def rows_between(first: int, last: int):
        return lettering & (logo_rows >= first) & (logo_rows < last)

    return {
        "e": trace(light & (radius < DETAILS_RADIUS), EMBLEM_SIZE),
        "details": trace(light & (radius >= DETAILS_RADIUS), EMBLEM_SIZE),
        "word": trace(rows_between(*LOGO_WORD_ROWS), lettering.shape[0]),
        "text": trace(rows_between(*LOGO_TEXT_ROWS), lettering.shape[0]),
    }


def emblem(art: dict[str, Shape], x: float, y: float, size: float, small: bool = False, inverted: bool = False) -> str:
    """The emblem at (x, y), `size` pixels wide. `inverted` is the light disc for dark backgrounds."""
    disc, detail = (CREAM, INK) if inverted else (INK, CREAM)
    potrace = f"translate(0 {EMBLEM_SIZE})scale(.1 -.1)"
    if small:
        c = EMBLEM_CENTER
        e_transform = f"translate({c:g} {c:g})scale({SMALL_E_SCALE:g})translate({-c:g} {-c:g}){potrace}"
        details = (
            f'<circle cx="{c:g}" cy="{c:g}" r="{SMALL_RING_RADIUS}" fill="none" stroke="{detail}" '
            f'stroke-width="{SMALL_RING_WIDTH}"/>'
        )
    else:
        e_transform = potrace
        details = f'<path fill="{detail}" transform="{potrace}" d="{art["details"].path}"/>'
    return (
        f'<g transform="translate({x:g} {y:g})scale({size / EMBLEM_SIZE:g})">'
        f'<circle cx="{EMBLEM_CENTER:g}" cy="{EMBLEM_CENTER:g}" r="{DISC_RADIUS}" fill="{disc}"/>'
        f'{details}<path fill="{detail}" transform="{e_transform}" d="{art["e"].path}"/>'
        "</g>"
    )


def console_badge() -> str:
    """Small terminal badge for the console executable icon (128x128 box)."""
    return (
        f'<rect width="58" height="44" x="68" y="82" fill="{CREAM}" stroke="{INK}" stroke-width="4" rx="8"/>'
        f'<path fill="none" stroke="{INK}" stroke-linecap="round" stroke-linejoin="round" stroke-width="6" '
        'd="m80 94 9 8-9 8m15 1h16"/>'
    )


def svg(width: float, height: float, body: str) -> str:
    # The viewBox lets a page show the SVG at another size, such as the README's logo.
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width:g}" height="{height:g}" '
        f'viewBox="0 0 {width:g} {height:g}">{body}</svg>\n'
    )


def vertical_logo(art: dict[str, Shape], x: float, y: float, height: float, dark_background: bool) -> tuple[str, float]:
    """The logo as in logo.png: the emblem over the lettering. Return it and its width."""
    left, top, right, _ = LOGO_EMBLEM
    text = art["text"]
    k = height / (text.bottom - top)
    lettering, _ = text.place(
        x + (text.left - left) * k, y + (text.top - top) * k, text.height * k, CREAM if dark_background else INK
    )
    return emblem(art, x, y, (right - left) * k, inverted=dark_background) + lettering, (right - left) * k


def build_svgs(art: dict[str, Shape]) -> dict[str, str]:
    files: dict[str, str] = {}

    # App icon, also the default icon of new projects.
    files["icon.svg"] = svg(128, 128, emblem(art, 0, 0, 128))
    files["icon_console.svg"] = svg(128, 128, emblem(art, 0, 0, 128) + console_badge())

    # 16x16 icon for the editor's "About" menu entry.
    files["editor_icon_16.svg"] = svg(16, 16, emblem(art, 0, 0, 16, small=True))

    # Logo for the About dialog and credits (replaces editor/icons/Logo.svg):
    # the emblem beside the lettering, to fit where Godot's logo was.
    lettering, width = art["text"].place(70, 10, 44, TEXT)
    files["logo.svg"] = svg(round(70 + width + 4), 64, emblem(art, 4, 4, 56) + lettering)

    # Project manager title bar logo (replaces editor/icons/TitleBarLogo.svg).
    word, width = art["word"].place(26, 7, 10, TEXT)
    files["title_bar_logo.svg"] = svg(round(26 + width + 2), 24, emblem(art, 1, 2, 20, small=True) + word)

    # Boot splash, on the engine's dark gray: the logo for dark backgrounds,
    # centered on a transparent 800x600 canvas.
    _, width = vertical_logo(art, 0, 0, 460, True)
    body, _ = vertical_logo(art, (800 - width) / 2, 70, 460, True)
    files["splash.svg"] = svg(800, 600, body)

    # The README's logo, for GitHub's light and dark themes.
    for name, dark in (("logo_vertical.svg", False), ("logo_vertical_dark.svg", True)):
        _, width = vertical_logo(art, 0, 0, 360, dark)
        body, _ = vertical_logo(art, 0, 0, 360, dark)
        files[name] = svg(round(width), 360, body)
    return files


def installer_images(art: dict[str, Shape]) -> dict[str, tuple[int, int, str]]:
    """The installer's wizard images (size and SVG), by BMP file name. They are only rasterized."""
    files: dict[str, tuple[int, int, str]] = {}
    for w, h in INSTALLER_SMALL_SIZES:
        size = min(w, h)
        files[f"installer_small_{w}x{h}.bmp"] = (
            w,
            h,
            svg(w, h, emblem(art, (w - size) / 2, (h - size) / 2, size, small=size < 100)),
        )
    for w, h in INSTALLER_LARGE_SIZES:
        logo_height = w * 1.1
        _, logo_width = vertical_logo(art, 0, 0, logo_height, True)
        body, _ = vertical_logo(art, (w - logo_width) / 2, (h - logo_height) / 2, logo_height, True)
        files[f"installer_large_{w}x{h}.bmp"] = (
            w,
            h,
            svg(w, h, f'<rect width="{w}" height="{h}" fill="{SPLASH_BACKGROUND}"/>' + body),
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


def build_bmp(source: Path, dst: Path) -> None:
    """24-bit BMP on white, as Inno Setup wants its wizard images."""
    from PIL import Image

    image = Image.open(source).convert("RGBA")
    background = Image.new("RGBA", image.size, "white")
    Image.alpha_composite(background, image).convert("RGB").save(dst, format="BMP")


def main() -> int:
    art = trace_art()
    BRANDING.mkdir(parents=True, exist_ok=True)
    for name, content in build_svgs(art).items():
        (BRANDING / name).write_text(content, encoding="utf-8", newline="\n")
        print(f"wrote erethea/branding/{name}")

    tmp = BRANDING / ".raster"
    tmp.mkdir(exist_ok=True)
    # The small icons, only for the rasters.
    (tmp / "icon_small.svg").write_text(svg(128, 128, emblem(art, 0, 0, 128, small=True)), encoding="utf-8")
    (tmp / "console_small.svg").write_text(
        svg(128, 128, emblem(art, 0, 0, 128, small=True) + console_badge()), encoding="utf-8"
    )
    ico_sizes = [16, 24, 32, 48, 64, 128, 256]
    jobs = [
        (BRANDING / "icon.svg", BRANDING / "app_icon.png", 128, 128),
        (BRANDING / "icon.svg", BRANDING / "icon_256.png", 256, 256),
        (BRANDING / "splash.svg", BRANDING / "splash.png", 800, 600),
    ]
    installer = installer_images(art)
    for bmp, (w, h, content) in installer.items():
        (tmp / f"{bmp}.svg").write_text(content, encoding="utf-8")
        jobs.append((tmp / f"{bmp}.svg", tmp / f"{bmp}.png", w, h))
    for size in ico_sizes:
        small = size <= 64
        jobs.append((tmp / "icon_small.svg" if small else BRANDING / "icon.svg", tmp / f"icon_{size}.png", size, size))
        jobs.append((
            tmp / "console_small.svg" if small else BRANDING / "icon_console.svg",
            tmp / f"console_{size}.png",
            size,
            size,
        ))
    rasterize(jobs)

    build_ico([tmp / f"icon_{s}.png" for s in ico_sizes], BRANDING / "erethea.ico")
    build_ico([tmp / f"console_{s}.png" for s in ico_sizes], BRANDING / "erethea_console.ico")
    for bmp in installer:
        build_bmp(tmp / f"{bmp}.png", BRANDING / bmp)
    for leftover in tmp.iterdir():
        leftover.unlink()
    tmp.rmdir()
    print("wrote erethea/branding/*.png, *.ico and *.bmp")
    return 0


if __name__ == "__main__":
    os.chdir(ROOT)
    sys.exit(main())
