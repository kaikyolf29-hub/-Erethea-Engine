#!/usr/bin/env python3
"""End-to-end test of the scenes built from game.project.json (modules/erethea_fornalha).

Copies the project next to this script, which looks like one made with the
Fornalha (game.project.json and stand-ins for its game scripts), and adds what
the Fornalha's Forja leaves: a player and an enemy as .glb models with their
animations, and the blockout of the world with its areas and spawn points.
A principal.tscn that the editor didn't make is there too, as the Fornalha's
own assembly leaves it.

Then it imports the project, starts the editor with its MCP server and builds
the scenes with the build_fornalha_scenes tool, as an agent would: the first
build makes them and keeps principal.tscn, a second one changes nothing, a
forced one replaces principal.tscn and keeps a copy, and a scene changed by
hand is kept. Last, it runs the game without a window and walks the player.

    python erethea/tests/fornalha_import/test_fornalha_import.py <editor executable> [--out <folder>]
"""

import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.dont_write_bytecode = True  # No __pycache__ in the repository.
sys.path.insert(0, str(HERE.parent / "mcp"))

from test_mcp import Failure, Mcp, check, wait_until_ready  # noqa: E402

FOREIGN_MAIN_SCENE = """[gd_scene format=3]

[node name="Principal" type="Node3D"]
"""


class Glb:
    """A binary glTF 2.0 file made of boxes, with animations that move its first node."""

    def __init__(self):
        self.buffer = bytearray()
        self.views = []
        self.accessors = []
        self.meshes = []
        self.nodes = []
        self.animations = []

    def _accessor(self, data, component, count, kind, target=None, low=None, high=None):
        while len(self.buffer) % 4:
            self.buffer.append(0)
        view = {"buffer": 0, "byteOffset": len(self.buffer), "byteLength": len(data)}
        if target:
            view["target"] = target
        self.buffer += data
        self.views.append(view)
        accessor = {"bufferView": len(self.views) - 1, "componentType": component, "count": count, "type": kind}
        if low is not None:
            accessor["min"], accessor["max"] = low, high
        self.accessors.append(accessor)
        return len(self.accessors) - 1

    def box(self, name, size, center):
        """A box whose bottom is at center's height."""
        (x, _y, z), (cx, cy, cz) = [s / 2 for s in size], center
        vertices = [(cx + sx * x, cy + h, cz + sz * z) for h in (0.0, size[1]) for sx in (-1, 1) for sz in (-1, 1)]
        faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
        # Counterclockwise seen from outside, so that collision works from outside.
        indices = [i for a, b, c, d in faces for i in (a, c, b, a, d, c)]
        position = self._accessor(
            b"".join(struct.pack("<3f", *v) for v in vertices),
            5126,
            len(vertices),
            "VEC3",
            34962,
            [min(v[i] for v in vertices) for i in range(3)],
            [max(v[i] for v in vertices) for i in range(3)],
        )
        index = self._accessor(struct.pack(f"<{len(indices)}H", *indices), 5123, len(indices), "SCALAR", 34963)
        self.meshes.append({"primitives": [{"attributes": {"POSITION": position}, "indices": index}]})
        self.nodes.append({"name": name, "mesh": len(self.meshes) - 1})

    def animation(self, name, lift):
        times = [0.0, 0.5, 1.0]
        moves = [(0.0, 0.0, 0.0), (0.0, lift, 0.0), (0.0, 0.0, 0.0)]
        time = self._accessor(struct.pack("<3f", *times), 5126, 3, "SCALAR", low=[0.0], high=[1.0])
        move = self._accessor(b"".join(struct.pack("<3f", *m) for m in moves), 5126, 3, "VEC3")
        self.animations.append({
            "name": name,
            "samplers": [{"input": time, "output": move, "interpolation": "LINEAR"}],
            "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}],
        })

    def save(self, path):
        while len(self.buffer) % 4:
            self.buffer.append(0)
        document = {
            "asset": {"version": "2.0", "generator": "erethea fornalha import test"},
            "scene": 0,
            "scenes": [{"nodes": list(range(len(self.nodes)))}],
            "nodes": self.nodes,
            "meshes": self.meshes,
            "buffers": [{"byteLength": len(self.buffer)}],
            "bufferViews": self.views,
            "accessors": self.accessors,
        }
        if self.animations:
            document["animations"] = self.animations
        text = json.dumps(document).encode()
        text += b" " * (-len(text) % 4)
        chunks = struct.pack("<II", len(text), 0x4E4F534A) + text
        chunks += struct.pack("<II", len(self.buffer), 0x004E4942) + bytes(self.buffer)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(struct.pack("<III", 0x46546C67, 2, 12 + len(chunks)) + chunks)


def make_assets(project):
    # The Forja's characters: their measures next to the model, and the
    # animations by the names the Fornalha's scripts play.
    for folder, name, size in (("Characters", "heroi", (0.5, 1.7, 0.3)), ("Enemies", "lobo", (0.4, 0.8, 1.0))):
        model = Glb()
        model.box("Corpo", size, (0.0, 0.0, 0.0))
        for animation in ("parado", "andar", "correr", "atacar"):
            model.animation(animation, 0.05)
        model.save(project / folder / name / f"{name}.glb")
        forja = {"estatisticas": {"altura_m": size[1]}}
        (project / folder / name / f"{name}.forja.json").write_text(json.dumps(forja), encoding="utf-8")

    # The blockout: ground and a house per area, with collision (-col), and
    # where each area is.
    world = Glb()
    world.box("chao-col", (160.0, 1.0, 160.0), (0.0, -1.0, 0.0))
    world.box("vila_casa-col", (4.0, 3.0, 4.0), (10.0, 0.0, 10.0))
    world.box("floresta_pedra-col", (2.0, 2.0, 2.0), (6.0, 0.0, -36.0))
    world.save(project / "World/blockout.glb")
    info = {
        "areas": [
            {"id": "vila", "tipo": "vila", "centro": [0.0, 0.0, 0.0], "inimigos": 1},
            {"id": "floresta", "tipo": "floresta", "centro": [0.0, 0.0, -40.0], "inimigos": 2},
        ],
        "spawn_jogador": [0.0, 0.2, 5.0],
        "spawn_inimigos": [[-6.0, 0.1, -3.0], [3.0, 0.1, -42.0], [-3.0, 0.1, -38.0]],
    }
    (project / "World/blockout.info.json").write_text(json.dumps(info, indent=1), encoding="utf-8")
    (project / ".fornalha").mkdir()
    scene = {"jogador": "heroi", "inimigo_perto": False, "musica": "", "ceu": ""}
    (project / ".fornalha/cena.json").write_text(json.dumps(scene), encoding="utf-8")
    (project / "principal.tscn").write_text(FOREIGN_MAIN_SCENE, encoding="utf-8")


def run(command, log, timeout):
    print("Running: " + " ".join(command))
    result = subprocess.run(
        command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout
    )
    output = result.stdout + result.stderr
    log.write_text(output, encoding="utf-8")
    return result.returncode, output


SCENES = [
    "res://Characters/heroi/heroi.tscn",
    "res://Enemies/lobo/lobo.tscn",
    "res://World/areas/vila.tscn",
    "res://World/areas/floresta.tscn",
    "res://World/mundo.tscn",
    "res://principal.tscn",
]


def results(report):
    print("    " + report["summary"])
    for scene in report["scenes"]:
        print(f"    {scene['result']}: {scene['file']} {scene.get('message', '')}")
    for warning in report["warnings"]:
        print(f"    warning: {warning}")
    return {scene["file"]: scene["result"] for scene in report["scenes"]}


def build_checks(mcp, project):
    print("First build")
    report = mcp.data("build_fornalha_scenes")
    found = results(report)
    check(list(found) == SCENES, "one scene per character and per area, the world and the main scene, in order")
    check(all(found[scene] == "created" for scene in SCENES[:-1]), "the new scenes are created")
    check(found["res://principal.tscn"] == "kept", "the main scene that the editor didn't make is kept")
    check(any("arena" in warning for warning in report["warnings"]), "an area not in the blockout yet is reported")
    check(report["ok"], "nothing failed")
    character = (project / "Characters/heroi/heroi.tscn").read_text(encoding="utf-8")
    check('path="res://Characters/heroi/heroi.glb"' in character, "the character scene uses the model's file")
    check('path="res://Scripts/fornalha/jogador.gd"' in character, "the player has the Fornalha's player script")
    check('[node name="BracoCamera"' in character, "the player has its camera")
    enemy = (project / "Enemies/lobo/lobo.tscn").read_text(encoding="utf-8")
    check('path="res://Scripts/fornalha/inimigo.gd"' in enemy, "the enemy has the Fornalha's enemy script")
    forest = (project / "World/areas/floresta.tscn").read_text(encoding="utf-8")
    check(forest.count('instance=ExtResource("') == 2, "the forest has its two enemies")
    world = (project / "World/mundo.tscn").read_text(encoding="utf-8")
    check('path="res://World/blockout.glb"' in world, "the world uses the blockout's file")
    check(
        'path="res://World/areas/vila.tscn"' in world and 'path="res://World/areas/floresta.tscn"' in world,
        "the world has its areas",
    )

    print("Second build")
    found = results(mcp.data("build_fornalha_scenes"))
    check(all(found[scene] == "unchanged" for scene in SCENES[:-1]), "nothing changes")
    check(found["res://principal.tscn"] == "kept", "the main scene is still kept")

    print("Forced build")
    report = mcp.data("build_fornalha_scenes", {"force": True})
    found = results(report)
    check(found["res://principal.tscn"] == "replaced", "the main scene is replaced")
    check(all(found[scene] == "unchanged" for scene in SCENES[:-1]), "the other scenes stay as they are")
    backup = project / report.get("backup_folder", "res://missing").replace("res://", "") / "principal.tscn"
    check(
        backup.is_file() and backup.read_text(encoding="utf-8") == FOREIGN_MAIN_SCENE,
        f"a copy of the old one is in {report.get('backup_folder')}",
    )
    main = (project / "principal.tscn").read_text(encoding="utf-8")
    check(
        'path="res://World/mundo.tscn"' in main and 'path="res://Characters/heroi/heroi.tscn"' in main,
        "the main scene has the world and the player",
    )

    print("A scene changed by hand")
    village = project / "World/areas/vila.tscn"
    village.write_text(
        village.read_text(encoding="utf-8") + '\n[node name="Bau" type="Node3D" parent="."]\n', encoding="utf-8"
    )
    found = results(mcp.data("build_fornalha_scenes"))
    check(found["res://World/areas/vila.tscn"] == "kept", "the changed area is kept")
    check('[node name="Bau"' in village.read_text(encoding="utf-8"), "with the change")
    check(
        all(found[scene] == "unchanged" for scene in SCENES if scene != "res://World/areas/vila.tscn"),
        "the others don't change",
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("editor")
    parser.add_argument("--out", type=Path, default=None, help="Folder for the logs.")
    parser.add_argument("--port", type=int, default=11492)
    parser.add_argument("--timeout", type=int, default=600)
    parser.add_argument("--keep", action="store_true", help="Keep the project, to look into a failure.")
    args = parser.parse_args()

    editor = str(Path(args.editor).resolve())
    out = (args.out or Path(tempfile.mkdtemp(prefix="erethea_fornalha_import_out_"))).resolve()
    out.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="erethea_fornalha_import_"))
    project = work / "project"
    shutil.copytree(HERE / "project", project)
    make_assets(project)

    try:
        print("Import")
        code, _ = run([editor, "--headless", "--import", "--path", str(project)], out / "import.log", args.timeout)
        check(code == 0, f"the project imports (exit code {code})")

        print("Editor with the MCP server")
        environment = dict(os.environ, ERETHEA_MCP_PORT=str(args.port))
        command = [editor, "--editor", "--headless", "--path", str(project), "--audio-driver", "Dummy"]
        print("Running: " + " ".join(command))
        with open(out / "editor.log", "w", encoding="utf-8", errors="replace") as log:
            process = subprocess.Popen(
                command, env=environment, stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL
            )
            try:
                mcp = Mcp(args.port)
                wait_until_ready(mcp, process, args.timeout)
                mcp.rpc(
                    "initialize",
                    {
                        "protocolVersion": "2025-06-18",
                        "capabilities": {},
                        "clientInfo": {"name": "test", "version": "1"},
                    },
                )
                build_checks(mcp, project)
            finally:
                process.terminate()
                try:
                    process.wait(15)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()

        print("The game")
        code, output = run(
            [editor, "--headless", "--path", str(project), "--script", "res://test_walk.gd"],
            out / "game.log",
            args.timeout,
        )
        line = next((line for line in output.splitlines() if line.startswith("ERETHEA_WALK ")), None)
        check(line is not None, f"the game runs (exit code {code})")
        walk = json.loads(line.split(" ", 1)[1])
        print(f"    {walk}")
        check(walk["player"], "the player is in the main scene")
        check(
            walk["walked_m"] > 2.0 and walk["on_floor"] and not walk["fell"],
            f"the player walks on the ground ({walk['walked_m']:.1f} m)",
        )
        check(walk.get("animation") == "andar", "playing the walk cycle")
        check(
            walk["loops"] == {"parado": True, "andar": True, "atacar": False},
            "the walk cycles loop, the attack doesn't",
        )
        check(walk["enemies"] == 3, "the three enemies are in the game")
        areas = {area["name"]: area["enemies"] for area in walk["areas"]}
        check(areas == {"vila": 1, "floresta": 2}, "each in the scene of its area")
    except Failure as failure:
        print(f"\nFAILED: {failure}\nLogs in {out}")
        return 1
    finally:
        if args.keep:
            print(f"The project is in {project}")
        else:
            shutil.rmtree(work, ignore_errors=True)

    print("\nERETHEA_FORNALHA_IMPORT_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
