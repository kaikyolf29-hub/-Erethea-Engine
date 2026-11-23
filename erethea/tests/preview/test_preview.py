#!/usr/bin/env python3
"""End-to-end test of the preview of the Fornalha (modules/erethea_preview).

Writes small glTF files and runs the engine as the Fornalha would, without a
project:

- --erethea-preview=<box.glb> --erethea-preview-capture=<image.png>: the
  capture exists, its center shows the (red) box and its corner the sky;
- a taller box fills more of the view, because the camera frames the asset;
- a control file with another asset replaces it without restarting (green);
- a file rewritten on disk is reloaded by itself;
- a file that isn't glTF makes the capture fail with exit code 1, and a good
  file still shown stays on screen when a bad one comes after it.

It needs a renderer that draws: run it with a window (xvfb-run on Linux, with
Mesa's lavapipe as Vulkan driver, or the desktop on Windows).

    python erethea/tests/preview/test_preview.py <editor executable> [--out <folder>] [--engine-args "<extra arguments>"]
"""

import argparse
import os
import shlex
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from preview_assets import pixel, read_png, write_box_glb  # noqa: E402

RED = (0.9, 0.1, 0.1)
GREEN = (0.1, 0.8, 0.1)
FPS = "30"


class Failure(Exception):
    pass


def check(condition, message):
    if not condition:
        raise Failure(message)
    print(f"  ok: {message}")


class Engine:
    def __init__(self, executable, out, extra_args, timeout):
        self.executable = executable
        self.out = Path(out)
        self.extra_args = extra_args
        self.timeout = timeout

    def command(self, arguments):
        return [str(self.executable), "--audio-driver", "Dummy", "--fixed-fps", FPS, *self.extra_args, *arguments]

    def run(self, name, arguments):
        log = self.out / f"{name}.log"
        print(f"- {name}")
        try:
            result = subprocess.run(
                self.command(arguments), capture_output=True, text=True, timeout=self.timeout, cwd=self.out
            )
        except subprocess.TimeoutExpired as error:
            log.write_text(
                (error.stdout or b"").decode(errors="replace")
                if isinstance(error.stdout, bytes)
                else (error.stdout or "")
            )
            raise Failure(f"{name}: the preview didn't finish in {self.timeout} seconds (see {log})")
        log.write_text(result.stdout + "\n--- stderr ---\n" + result.stderr)
        return result


def mean_color(image, box):
    x0, y0, x1, y1 = box
    total = [0, 0, 0]
    count = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            for i, value in enumerate(pixel(image, x, y)):
                total[i] += value
            count += 1
    return tuple(value / count for value in total)


def covered_fraction(image):
    """How much of the image is the red of the box (the floor and the sky are not red)."""
    width, height, _, _ = image
    red = 0
    total = 0
    for y in range(0, height, 4):
        for x in range(0, width, 4):
            r, g, b = pixel(image, x, y)
            if r > g + 40 and r > b + 40:
                red += 1
            total += 1
    return red / total


def center_box(image, fraction=0.06):
    width, height, _, _ = image
    half_w = max(2, int(width * fraction))
    half_h = max(2, int(height * fraction))
    return (width // 2 - half_w, height // 2 - half_h, width // 2 + half_w, height // 2 + half_h)


def test_capture(engine, work):
    box = work / "box.glb"
    write_box_glb(box, 1.0, 1.0, RED)
    image_path = work / "box.png"
    result = engine.run("capture", [f"--erethea-preview={box}", f"--erethea-preview-capture={image_path}"])
    check(result.returncode == 0, f"the preview exits with 0 (got {result.returncode})")
    check(image_path.is_file() and image_path.stat().st_size > 0, "the capture exists and isn't empty")
    check("showing" in result.stdout, "the log says which asset it shows")
    image = read_png(image_path)
    width, height = image[0], image[1]
    check(width > 0 and height > 0, f"the capture is an image ({width} x {height})")
    center = mean_color(image, center_box(image))
    corner = pixel(image, 4, 4)
    print(f"    center {tuple(round(v) for v in center)}, corner {corner}")
    check(sum(abs(center[i] - corner[i]) for i in range(3)) > 60, "the center is not the color of the background")
    check(center[0] > center[1] + 30 and center[0] > center[2] + 30, "the center is red, the color of the box")
    return covered_fraction(image)


def test_framing(engine, work, small_fraction):
    # The camera frames the asset: a box ten times bigger takes the same room
    # in the view, not ten times more.
    big = work / "big.glb"
    write_box_glb(big, 10.0, 10.0, RED)
    image_path = work / "big.png"
    result = engine.run("framing", [f"--erethea-preview={big}", f"--erethea-preview-capture={image_path}"])
    check(result.returncode == 0, "the big box is captured")
    fraction = covered_fraction(read_png(image_path))
    print(f"    the box covers {small_fraction:.2f} of the view as red, the big box {fraction:.2f}")
    check(fraction > 0.1, "the big box is in the view")
    check(small_fraction > 0.1, "the small box is in the view")
    check(abs(fraction - small_fraction) < 0.15, "a box ten times bigger covers about the same part of the view")


def test_control_file(engine, work):
    first = work / "first.glb"
    second = work / "second.glb"
    write_box_glb(first, 1.0, 1.0, RED)
    write_box_glb(second, 1.0, 1.0, GREEN)
    control = work / "control.txt"
    control.write_text(str(second))
    image_path = work / "swapped.png"
    # At 30 frames per second the control file is read after 15 frames.
    result = engine.run(
        "control",
        [
            f"--erethea-preview={first}",
            f"--erethea-preview-control={control}",
            f"--erethea-preview-capture={image_path}",
            "--erethea-preview-frames=45",
        ],
    )
    check(result.returncode == 0, "the preview exits with 0")
    shown = [line for line in result.stdout.splitlines() if "Erethea preview: showing" in line]
    check(
        len(shown) == 2 and shown[0].endswith("first.glb") and shown[1].endswith("second.glb"),
        "it showed the first asset, then the second, in one run",
    )
    image = read_png(image_path)
    center = mean_color(image, center_box(image))
    check(center[1] > center[0] + 30 and center[1] > center[2] + 30, "the capture shows the second asset (green)")


def test_reload_on_change(engine, work):
    asset = work / "changing.glb"
    write_box_glb(asset, 1.0, 1.0, RED)
    image_path = work / "reloaded.png"
    arguments = [
        f"--erethea-preview={asset}",
        f"--erethea-preview-capture={image_path}",
        "--erethea-preview-frames=120",
    ]
    print("- reload")
    process = subprocess.Popen(
        engine.command(arguments), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=work
    )
    lines = []
    first_shown = threading.Event()

    def read_output():
        for line in process.stdout:
            lines.append(line)
            if "Erethea preview: showing" in line:
                first_shown.set()

    reader = threading.Thread(target=read_output, daemon=True)
    reader.start()
    try:
        check(first_shown.wait(engine.timeout), "the preview shows the first version")
        write_box_glb(asset, 1.0, 1.0, GREEN)
        # The file system has a precision of one second: make the change visible.
        later = time.time() + 10
        os.utime(asset, (later, later))
        process.wait(timeout=engine.timeout)
    except subprocess.TimeoutExpired:
        process.kill()
        raise Failure("the preview didn't finish")
    reader.join(5)
    (work / "reload.log").write_text("".join(lines))
    check(process.returncode == 0, "the preview exits with 0")
    check(
        sum(1 for line in lines if "Erethea preview: showing" in line) == 2,
        "the preview reloaded the file when it changed",
    )
    image = read_png(image_path)
    center = mean_color(image, center_box(image))
    check(center[1] > center[0] + 30 and center[1] > center[2] + 30, "the capture shows the new version (green)")


def test_bad_files(engine, work):
    not_gltf = work / "broken.glb"
    not_gltf.write_bytes(b"this is not a glTF file")
    image_path = work / "broken.png"
    result = engine.run("broken", [f"--erethea-preview={not_gltf}", f"--erethea-preview-capture={image_path}"])
    check(result.returncode == 1, f"a broken file makes the preview exit with 1 (got {result.returncode})")
    check(not image_path.exists(), "a broken file leaves no capture")

    wrong_type = work / "model.obj"
    wrong_type.write_text("v 0 0 0\n")
    result = engine.run(
        "wrong_type", [f"--erethea-preview={wrong_type}", f"--erethea-preview-capture={work / 'wrong.png'}"]
    )
    check(result.returncode == 1, "a file that is not .glb or .gltf is refused")

    # A bad asset after a good one: the good one stays on screen.
    good = work / "good.glb"
    write_box_glb(good, 1.0, 1.0, RED)
    control = work / "bad_control.txt"
    control.write_text(str(not_gltf))
    image_path = work / "kept.png"
    result = engine.run(
        "kept",
        [
            f"--erethea-preview={good}",
            f"--erethea-preview-control={control}",
            f"--erethea-preview-capture={image_path}",
            "--erethea-preview-frames=45",
        ],
    )
    check(result.returncode == 0, "the preview survives a broken file")
    image = read_png(image_path)
    center = mean_color(image, center_box(image))
    check(center[0] > center[1] + 30, "the good asset is still on screen")


def test_quality(engine, work):
    box = work / "quality_box.glb"
    write_box_glb(box, 1.0, 1.0, RED)
    for preset in ("low", "cinematic"):
        image_path = work / f"quality_{preset}.png"
        result = engine.run(
            f"quality_{preset}",
            [
                f"--erethea-preview={box}",
                f"--erethea-preview-quality={preset}",
                f"--erethea-preview-capture={image_path}",
            ],
        )
        check(result.returncode == 0 and image_path.is_file(), f"the preview draws with the {preset} preset")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable")
    parser.add_argument("--out", default=None)
    parser.add_argument(
        "--engine-args", default="", help="extra arguments for the engine, such as --rendering-driver opengl3"
    )
    parser.add_argument("--timeout", type=int, default=240)
    options = parser.parse_args()

    out = Path(options.out) if options.out else Path(tempfile.mkdtemp(prefix="erethea_preview_"))
    out.mkdir(parents=True, exist_ok=True)
    work = out / "work"
    work.mkdir(exist_ok=True)
    engine = Engine(Path(options.executable).resolve(), out, shlex.split(options.engine_args), options.timeout)
    try:
        small_fraction = test_capture(engine, work)
        test_framing(engine, work, small_fraction)
        test_control_file(engine, work)
        test_reload_on_change(engine, work)
        test_bad_files(engine, work)
        test_quality(engine, work)
    except Failure as failure:
        print(f"FAILED: {failure}")
        return 1
    print("ERETHEA_PREVIEW_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
