#!/usr/bin/env python3
"""Test of the preview embedded with LibGodot.

Builds libgodot_preview.c (a host program, like the Fornalha), loads the
engine as a shared library with it, shows a small glTF box with the preview,
and checks the capture: the center must show the (red) box, the corner the
sky. Needs a renderer that draws (xvfb-run on Linux, with Mesa's lavapipe as
Vulkan driver).

    python erethea/tests/libgodot/test_libgodot.py <library> [--out <folder>] [--engine-args "<extra arguments>"]

The library comes from: scons platform=<platform> target=editor library_type=shared_library
"""

import argparse
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE.parent / "preview"))

from preview_assets import pixel, read_png, write_box_glb  # noqa: E402


class Failure(Exception):
    pass


def check(condition, message):
    if not condition:
        raise Failure(message)
    print(f"  ok: {message}")


def build_host(out):
    """Compiles the host program. Returns the path of the executable."""
    source = HERE / "libgodot_preview.c"
    if sys.platform == "win32":
        target = out / "libgodot_preview.exe"
        compiler = shutil.which("cl")
        if compiler:
            command = [
                compiler,
                "/nologo",
                f"/I{ROOT}",
                str(source),
                f"/Fe{target}",
            ]
        else:
            compiler = shutil.which("gcc") or shutil.which("clang")
            command = [compiler, f"-I{ROOT}", "-o", str(target), str(source)] if compiler else None
    else:
        target = out / "libgodot_preview"
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        command = (
            [compiler, "-Wall", "-Werror", f"-I{ROOT}", "-o", str(target), str(source), "-ldl"] if compiler else None
        )
    if not command:
        raise Failure("No C compiler found (cc, gcc, clang or cl).")
    print("- build the host program:", " ".join(command))
    result = subprocess.run(command, capture_output=True, text=True, cwd=out)  # The object file lands in out.
    if result.returncode != 0:
        print(result.stdout + result.stderr)
        raise Failure("The host program doesn't compile.")
    return target


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("library")
    parser.add_argument("--out", default=None)
    parser.add_argument("--engine-args", default="")
    parser.add_argument("--timeout", type=int, default=240)
    options = parser.parse_args()

    out = Path(options.out) if options.out else Path(tempfile.mkdtemp(prefix="erethea_libgodot_"))
    out.mkdir(parents=True, exist_ok=True)
    library = Path(options.library).resolve()
    try:
        check(library.is_file(), f"the library exists ({library.name})")
        host = build_host(out)

        box = out / "box.glb"
        write_box_glb(box, 1.0, 1.0, (0.9, 0.1, 0.1))
        capture = out / "capture.png"
        capture.unlink(missing_ok=True)
        command = [
            str(host),
            str(library),
            "--audio-driver",
            "Dummy",
            "--fixed-fps",
            "30",
            *shlex.split(options.engine_args),
            f"--erethea-preview={box}",
            f"--erethea-preview-capture={capture}",
        ]
        print("- run:", " ".join(command))
        env = dict(os.environ, ERETHEA_LIBGODOT_TIMEOUT=str(options.timeout - 20))
        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=options.timeout, cwd=out, env=env)
        except subprocess.TimeoutExpired:
            raise Failure(f"The host didn't finish in {options.timeout} seconds.")
        (out / "libgodot.log").write_text(result.stdout + "\n--- stderr ---\n" + result.stderr)
        print(result.stdout)
        check(result.returncode == 0, f"the host runs the instance until it closes (exit code {result.returncode})")
        check("the instance started" in result.stdout, "GodotInstance.start() worked")
        check("capture saved" in result.stdout, "the preview saved its capture")
        check(capture.is_file() and capture.stat().st_size > 0, "the capture exists and isn't empty")

        image = read_png(capture)
        width, height = image[0], image[1]
        half_w, half_h = max(2, width // 16), max(2, height // 16)
        total = [0, 0, 0]
        count = 0
        for y in range(height // 2 - half_h, height // 2 + half_h):
            for x in range(width // 2 - half_w, width // 2 + half_w):
                for i, value in enumerate(pixel(image, x, y)):
                    total[i] += value
                count += 1
        center = tuple(value / count for value in total)
        corner = pixel(image, 4, 4)
        print(f"    capture {width} x {height}, center {tuple(round(v) for v in center)}, corner {corner}")
        check(sum(abs(center[i] - corner[i]) for i in range(3)) > 60, "the center is not the color of the background")
        check(center[0] > center[1] + 30 and center[0] > center[2] + 30, "the center is red, the color of the box")
    except Failure as failure:
        print(f"FAILED: {failure}")
        return 1
    print("ERETHEA_LIBGODOT_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
