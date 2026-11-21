#!/usr/bin/env python3
"""End-to-end test of the checks before export (modules/erethea_export_checks).

Copies the project next to this script, which looks like one made with the
Fornalha (it has a game.project.json and is commercial), adds models, textures
and their license receipts with known problems, and exports it as a .pck with
the editor's command line. The export log must list each problem, and nothing
about the assets that are fine. Then the same project without game.project.json,
like a plain Godot project, must export without any check.

    python erethea/tests/export_checks/test_export_checks.py <editor executable> [--out <folder>]
"""

import argparse
import base64
import hashlib
import json
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent


class Failure(Exception):
    pass


def check(condition, message):
    if not condition:
        raise Failure(message)
    print(f"  ok: {message}")


def gltf_mesh(vertices, indices):
    """A glTF 2.0 file with one mesh, its buffer embedded as base64."""
    vertex_bytes = b"".join(struct.pack("<3f", *v) for v in vertices)
    index_bytes = b"".join(struct.pack("<I", i) for i in indices)
    buffer = vertex_bytes + index_bytes
    low = [min(v[axis] for v in vertices) for axis in range(3)]
    high = [max(v[axis] for v in vertices) for axis in range(3)]
    return json.dumps(
        {
            "asset": {"version": "2.0", "generator": "erethea export checks test"},
            "scene": 0,
            "scenes": [{"nodes": [0]}],
            "nodes": [{"mesh": 0, "name": "Mesh"}],
            "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}],
            "buffers": [
                {
                    "byteLength": len(buffer),
                    "uri": "data:application/octet-stream;base64," + base64.b64encode(buffer).decode(),
                }
            ],
            "bufferViews": [
                {"buffer": 0, "byteOffset": 0, "byteLength": len(vertex_bytes), "target": 34962},
                {"buffer": 0, "byteOffset": len(vertex_bytes), "byteLength": len(index_bytes), "target": 34963},
            ],
            "accessors": [
                {
                    "bufferView": 0,
                    "componentType": 5126,
                    "count": len(vertices),
                    "type": "VEC3",
                    "min": low,
                    "max": high,
                },
                {"bufferView": 1, "componentType": 5125, "count": len(indices), "type": "SCALAR"},
            ],
        },
        indent=1,
    )


def box(width, height, depth):
    """A box standing on the ground: 12 triangles."""
    x, z = width / 2, depth / 2
    vertices = [(sx * x, y, sz * z) for y in (0.0, height) for sx in (-1, 1) for sz in (-1, 1)]
    faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    indices = []
    for a, b, c, d in faces:
        indices += [a, b, c, a, c, d]
    return gltf_mesh(vertices, indices)


def grid(columns, rows, size):
    """A standing grid of columns x rows quads, size meters square: 2 triangles each."""
    vertices = [(size * i / columns, size * j / rows, 0.0) for j in range(rows + 1) for i in range(columns + 1)]
    indices = []
    for j in range(rows):
        for i in range(columns):
            a = j * (columns + 1) + i
            b, c, d = a + 1, a + columns + 2, a + columns + 1
            indices += [a, b, c, a, c, d]
    return gltf_mesh(vertices, indices)


def png(width, height):
    """An RGB gradient, so that it doesn't compress to nothing."""
    rows = b"".join(b"\x00" + b"".join(bytes((x % 256, y % 256, 128)) for x in range(width)) for y in range(height))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")


def receipt(project, file, model, license_name, commercial_use, sha256=None):
    data = (project / file).read_bytes()
    return {
        "id": "rec_" + hashlib.sha1(file.encode()).hexdigest()[:8],
        "arquivo": file,
        "sha256": sha256 or hashlib.sha256(data).hexdigest(),
        "tamanho_bytes": len(data),
        "funcao": "modelo_3d",
        "modelo": {
            "id": model.lower().replace(" ", "-"),
            "nome": model,
            "licenca": license_name,
            "url_licenca": "",
            "uso_comercial": commercial_use,
            "onde": "worker",
        },
        "criado_em": "2026-11-21T00:00:00Z",
        "quem_pediu": "teste",
        "projeto_comercial": True,
    }


def make_assets(project):
    files = {
        # Fine.
        "Characters/hero/hero.gltf": box(0.5, 1.8, 0.3),
        # Exported in centimeters.
        "Characters/giant/giant.gltf": box(50, 180, 30),
        # 2,000 triangles, above the 1,000 of project.godot.
        "Enemies/blob/blob.gltf": grid(40, 25, 1.5),
        "World/house.gltf": box(8, 5, 8),
    }
    for name, text in files.items():
        (project / name).parent.mkdir(parents=True, exist_ok=True)
        (project / name).write_text(text, encoding="utf-8")
    (project / "UI").mkdir()
    # Above the 256 px of project.godot.
    (project / "UI/big.png").write_bytes(png(512, 512))
    (project / "UI/small.png").write_bytes(png(64, 64))
    (project / "Scripts").mkdir()
    (project / "Scripts/main.gd").write_text("extends Node\n", encoding="utf-8")

    receipts = [
        receipt(project, "Characters/hero/hero.gltf", "Hero Maker", "MIT", "sim"),
        receipt(project, "Characters/giant/giant.gltf", "Hero Maker", "MIT", "sim"),
        receipt(project, "Enemies/blob/blob.gltf", "NoSale 3D", "CC BY-NC 4.0", "nao"),
        receipt(project, "UI/big.png", "Paint Pro", "Custom", "condicional"),
        receipt(project, "UI/small.png", "Paint Pro", "MIT", "sim", sha256="0" * 64),
        # World/house.gltf has no receipt.
    ]
    (project / "ai").mkdir()
    (project / "ai/recibos.jsonl").write_text("".join(json.dumps(r) + "\n" for r in receipts), encoding="utf-8")


def run(editor, arguments, log, timeout):
    command = [editor, *arguments]
    print("Running: " + " ".join(command))
    result = subprocess.run(
        command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout
    )
    output = result.stdout + result.stderr
    log.write_text(output, encoding="utf-8")
    return result.returncode, output


def check_lines(output):
    """The "Erethea Checks" lines of the export log, as (level, text)."""
    lines = []
    for line in output.splitlines():
        match = re.match(r"^(?:(WARNING|ERROR): )?Erethea Checks: (.*)$", line.strip())
        if match:
            lines.append((match.group(1) or "INFO", match.group(2)))
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("editor")
    parser.add_argument("--out", type=Path, default=None, help="Folder for the logs.")
    parser.add_argument("--timeout", type=int, default=600)
    args = parser.parse_args()

    editor = str(Path(args.editor).resolve())
    out = (args.out or Path(tempfile.mkdtemp(prefix="erethea_export_checks_out_"))).resolve()
    out.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="erethea_export_checks_"))
    project = work / "project"
    shutil.copytree(HERE / "project", project)
    make_assets(project)

    try:
        print("Import")
        code, output = run(editor, ["--headless", "--import", "--path", str(project)], out / "import.log", args.timeout)
        check(code == 0, f"the project imports (exit code {code})")

        print("Project made with the Fornalha")
        pck = project / "build/game.pck"
        pck.parent.mkdir()
        code, output = run(
            editor,
            ["--headless", "--path", str(project), "--export-pack", "Linux", str(pck)],
            out / "export.log",
            args.timeout,
        )
        check(code == 0, f"the export ends (exit code {code})")
        check(pck.is_file() and pck.stat().st_size > 0, "the .pck is written")
        lines = check_lines(output)
        for level, text in lines:
            print(f"    {level}: {text}")

        def find(level, file, words):
            return [
                text
                for found_level, text in lines
                if found_level == level and text.startswith(f"res://{file}: ") and words in text
            ]

        check(
            len(find("WARNING", "Characters/giant/giant.gltf", "180 m tall")) == 1,
            "the giant is too tall (centimeters)",
        )
        check(len(find("WARNING", "Enemies/blob/blob.gltf", "2,000 triangles")) == 1, "the blob has too many triangles")
        check(len(find("ERROR", "Enemies/blob/blob.gltf", "NoSale 3D")) == 1, "the blob's model can't be sold")
        check(len(find("ERROR", "World/house.gltf", "No license receipt")) == 1, "the house has no receipt")
        check(len(find("WARNING", "UI/big.png", "512x512")) == 1, "the big texture is too big")
        check(len(find("WARNING", "UI/big.png", "Paint Pro")) == 1, "the big texture's license has conditions")
        check(len(find("WARNING", "UI/small.png", "Changed after")) == 1, "the small texture changed after its receipt")
        check(not any("hero" in text for _, text in lines), "nothing about the hero, which is fine")
        check(not any("Scripts/" in text for _, text in lines), "scripts need no receipt")
        summary = [text for level, text in lines if level == "INFO"]
        check(
            summary == ["Checked 4 models, 2 textures and 6 license receipts: 2 errors, 5 warnings."],
            f"the summary counts everything ({summary})",
        )

        print("Plain Godot project")
        (project / "game.project.json").unlink()
        pck.unlink()
        code, output = run(
            editor,
            ["--headless", "--path", str(project), "--export-pack", "Linux", str(pck)],
            out / "export_godot.log",
            args.timeout,
        )
        check(code == 0 and pck.is_file(), "the export ends")
        check(check_lines(output) == [], "no checks run by default")
    except Failure as failure:
        print(f"\nFAILED: {failure}\nLogs in {out}")
        return 1
    finally:
        shutil.rmtree(work, ignore_errors=True)

    print("\nERETHEA_EXPORT_CHECKS_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
