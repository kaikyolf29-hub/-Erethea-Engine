#!/usr/bin/env python3
"""Test of Erethea's dense geometry (modules/erethea_dense_geometry).

Makes a landscape of about two million triangles as a glTF file, imports it in
two projects, one with rendering/erethea/dense_geometry/cluster_triangles and one
without, and draws both from near the ground and from far away
(measure.gd). Split into clusters, the model must become many mesh instances,
and near the ground the renderer must draw far fewer primitives than the same
model whole, as the far clusters use their LODs and the ones out of view are
culled. Needs a renderer that draws (xvfb-run on Linux, with Mesa's lavapipe as
Vulkan driver).

    python erethea/tests/dense_geometry/test_dense_geometry.py <editor> [--out <folder>] [--size <quads per side>]

Prints ERETHEA_DENSE_OK when it passes.
"""

import argparse
import array
import json
import math
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
CLUSTER_TRIANGLES = 65536
WIDTH = 1000.0


class Failure(Exception):
    pass


def check(condition, message):
    if not condition:
        raise Failure(message)
    print(f"  ok: {message}")


def height(x, z):
    return 18.0 * math.sin(x * 0.011) * math.cos(z * 0.013) + 6.0 * math.sin(x * 0.07 + z * 0.05)


def write_landscape_glb(path, size):
    """A grid of size x size quads (2 * size^2 triangles) shaped as hills."""
    side = size + 1
    positions = array.array("f")
    normals = array.array("f")
    step = WIDTH / size
    low, high = float("inf"), float("-inf")
    for j in range(side):
        z = -WIDTH / 2 + j * step
        for i in range(side):
            x = -WIDTH / 2 + i * step
            y = height(x, z)
            low, high = min(low, y), max(high, y)
            positions.extend((x, y, z))
            dx = (height(x + 0.5, z) - height(x - 0.5, z)) / 1.0
            dz = (height(x, z + 0.5) - height(x, z - 0.5)) / 1.0
            length = math.sqrt(dx * dx + 1.0 + dz * dz)
            normals.extend((-dx / length, 1.0 / length, -dz / length))
    indices = array.array("I")
    for j in range(size):
        for i in range(size):
            a = j * side + i
            indices.extend((a, a + side, a + 1, a + 1, a + side, a + side + 1))
    if sys.byteorder != "little":
        positions.byteswap()
        normals.byteswap()
        indices.byteswap()
    binary = positions.tobytes() + normals.tobytes() + indices.tobytes()
    vertex_count = side * side
    gltf = {
        "asset": {"version": "2.0", "generator": "Erethea dense geometry test"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": "Landscape"}],
        "meshes": [
            {
                "name": "Landscape",
                "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": 0}],
            }
        ],
        "materials": [{"name": "Ground", "pbrMetallicRoughness": {"baseColorFactor": [0.35, 0.55, 0.25, 1.0]}}],
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(positions) * 4, "target": 34962},
            {"buffer": 0, "byteOffset": len(positions) * 4, "byteLength": len(normals) * 4, "target": 34962},
            {
                "buffer": 0,
                "byteOffset": (len(positions) + len(normals)) * 4,
                "byteLength": len(indices) * 4,
                "target": 34963,
            },
        ],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": vertex_count,
                "type": "VEC3",
                "min": [-WIDTH / 2, low, -WIDTH / 2],
                "max": [WIDTH / 2, high, WIDTH / 2],
            },
            {"bufferView": 1, "componentType": 5126, "count": vertex_count, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5125, "count": len(indices), "type": "SCALAR"},
        ],
    }
    json_bytes = json.dumps(gltf).encode()
    json_bytes += b" " * ((4 - len(json_bytes) % 4) % 4)
    binary += b"\0" * ((4 - len(binary) % 4) % 4)
    with open(path, "wb") as file:
        file.write(struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(json_bytes) + 8 + len(binary)))
        file.write(struct.pack("<II", len(json_bytes), 0x4E4F534A) + json_bytes)
        file.write(struct.pack("<II", len(binary), 0x004E4942) + binary)
    return len(indices) // 3


def make_project(folder, glb, cluster_triangles):
    folder.mkdir(parents=True, exist_ok=True)
    shutil.copy(glb, folder / "dense.glb")
    shutil.copy(HERE / "measure.gd", folder / "measure.gd")
    (folder / "measure.tscn").write_text(
        '[gd_scene format=3]\n\n[ext_resource type="Script" path="res://measure.gd" id="1"]\n\n'
        '[node name="Measure" type="Node3D"]\nscript = ExtResource("1")\n'
    )
    (folder / "project.godot").write_text(
        'config_version=5\n\n[application]\n\nconfig/name="dense"\nrun/main_scene="res://measure.tscn"\n\n'
        "[display]\n\nwindow/size/viewport_width=960\nwindow/size/viewport_height=540\n\n"
        f"[rendering]\n\nerethea/dense_geometry/cluster_triangles={cluster_triangles}\n"
    )


def run_project(editor, folder):
    imported = subprocess.run(
        [editor, "--headless", "--import", "--path", str(folder)], capture_output=True, text=True, timeout=1800
    )
    (folder / "import.log").write_text(imported.stdout + imported.stderr)
    if imported.returncode != 0:
        print(imported.stdout[-3000:] + imported.stderr[-3000:])
        raise Failure(f"importing {folder.name} failed (exit code {imported.returncode})")
    result = subprocess.run(
        [editor, "--path", str(folder), "--audio-driver", "Dummy"], capture_output=True, text=True, timeout=1800
    )
    (folder / "run.log").write_text(result.stdout + result.stderr)
    for line in result.stdout.splitlines():
        if line.startswith("DENSE "):
            return json.loads(line[len("DENSE ") :])
    print(result.stdout[-3000:] + result.stderr[-3000:])
    raise Failure(f"{folder.name} printed no measurement")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("editor")
    parser.add_argument("--out", default=None)
    parser.add_argument("--size", type=int, default=1024)
    options = parser.parse_args()
    out = Path(options.out) if options.out else Path(tempfile.mkdtemp(prefix="erethea_dense_"))
    out.mkdir(parents=True, exist_ok=True)
    try:
        glb = out / "dense.glb"
        triangles = write_landscape_glb(glb, options.size)
        print(f"- the model has {triangles} triangles ({glb.stat().st_size // (1024 * 1024)} MB)")
        clustered = out / "clustered"
        whole = out / "whole"
        make_project(clustered, glb, CLUSTER_TRIANGLES)
        make_project(whole, glb, 0)
        split = run_project(options.editor, clustered)
        print("- split into clusters:", split)
        kept = run_project(options.editor, whole)
        print("- kept whole:", kept)
        check(kept["instances"] == 1, "without the setting, the model stays one mesh instance")
        check(
            split["instances"] >= triangles // CLUSTER_TRIANGLES,
            f"with the setting, it becomes {split['instances']} instances of at most {CLUSTER_TRIANGLES} triangles",
        )
        # Whole, Godot picks one LOD for the whole model by its distance, which
        # near the ground is one of the finest: a large part of the model.
        check(kept["near"]["primitives"] >= triangles * 0.25, "whole, the near view draws a large part of the model")
        check(
            split["near"]["primitives"] <= kept["near"]["primitives"] * 0.5,
            f"split, the near view draws {split['near']['primitives']} primitives instead of {kept['near']['primitives']}",
        )
        check(split["far"]["primitives"] > 0, "split, the far view still draws the model")
        print(
            f"    frame time near: {kept['near']['ms']:.1f} ms whole, {split['near']['ms']:.1f} ms split "
            "(lavapipe draws on the CPU: only for comparing one run with another)"
        )
    except Failure as failure:
        print(f"FAILED: {failure}")
        return 1
    print("ERETHEA_DENSE_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
