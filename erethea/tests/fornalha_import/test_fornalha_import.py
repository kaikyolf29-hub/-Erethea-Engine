#!/usr/bin/env python3
"""End-to-end test of the scenes built from game.project.json (modules/erethea_fornalha).

Copies the project next to this script, which looks like one made with the
Fornalha (game.project.json and stand-ins for its game scripts), and adds what
the Fornalha's Forja leaves: a player and an enemy as .glb models with their
animations, and the blockout of the world with its areas and spawn points.
A principal.tscn that the editor didn't make is there too, as the Fornalha's
own assembly leaves it. Audio/musica has two short songs (WAV files with their
.musica.json), which the build makes change at the end of the bar.

Then it imports the project, starts the editor with its MCP server and builds
the scenes with the build_fornalha_scenes tool, as an agent would: the first
build makes them and keeps principal.tscn, a second one changes nothing, a
forced one replaces principal.tscn and keeps a copy, and a scene changed by
hand is kept. Last, it runs the game without a window and walks the player.

    python erethea/tests/fornalha_import/test_fornalha_import.py <editor executable> [--out <folder>]
"""

import argparse
import contextlib
import json
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import wave
import zlib
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
        self.images = []

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

    def texture(self, name, png):
        """An image in the file, used by the boxes made after it."""
        while len(self.buffer) % 4:
            self.buffer.append(0)
        self.views.append({"buffer": 0, "byteOffset": len(self.buffer), "byteLength": len(png)})
        self.buffer += png
        self.images.append({"name": name, "mimeType": "image/png", "bufferView": len(self.views) - 1})

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
        primitive = {"attributes": {"POSITION": position}, "indices": index}
        if self.images:
            uvs = [(v[0] - cx + 0.5, v[2] - cz + 0.5) for v in vertices]
            primitive["attributes"]["TEXCOORD_0"] = self._accessor(
                b"".join(struct.pack("<2f", *uv) for uv in uvs), 5126, len(uvs), "VEC2", 34962
            )
            primitive["material"] = len(self.images) - 1
        self.meshes.append({"primitives": [primitive]})
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
        if self.images:
            document["images"] = self.images
            document["textures"] = [{"source": i} for i in range(len(self.images))]
            document["materials"] = [
                {
                    "name": image["name"],
                    "pbrMetallicRoughness": {"baseColorTexture": {"index": i}, "metallicFactor": 0.0},
                }
                for i, image in enumerate(self.images)
            ]
        text = json.dumps(document).encode()
        text += b" " * (-len(text) % 4)
        chunks = struct.pack("<II", len(text), 0x4E4F534A) + text
        chunks += struct.pack("<II", len(self.buffer), 0x004E4942) + bytes(self.buffer)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(struct.pack("<III", 0x46546C67, 2, 12 + len(chunks)) + chunks)


# The texture that the import extracts from the hero's model.
HERO_TEXTURE = "res://Characters/heroi/heroi_pele.png"


def png(width, height, color):
    """An RGBA PNG of one color."""

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    rows = b"".join(b"\x00" + bytes(color) * width for _ in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")


def make_assets(project):
    # The Forja's characters: their measures next to the model, and the
    # animations by the names the Fornalha's scripts play. The hero has a
    # texture, as the Forja's models do, which the build has to compress.
    for folder, name, size in (("Characters", "heroi", (0.5, 1.7, 0.3)), ("Enemies", "lobo", (0.4, 0.8, 1.0))):
        model = Glb()
        if name == "heroi":
            model.texture("pele", png(64, 64, (200, 150, 120, 255)))
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
    # The music of each situation: a short mono 16-bit WAV, as long as the
    # .musica.json says (compassos * 4 * 60 / bpm seconds).
    for situation, bpm, bars, hertz in MUSIC:
        seconds = bars * 4 * 60 / bpm
        rate = 22050
        samples = b"".join(
            struct.pack("<h", int(6000 * math.sin(2 * math.pi * hertz * i / rate))) for i in range(int(seconds * rate))
        )
        (project / "Audio/musica").mkdir(parents=True, exist_ok=True)
        with wave.open(str(project / f"Audio/musica/{situation}.wav"), "wb") as file:
            file.setnchannels(1)
            file.setsampwidth(2)
            file.setframerate(rate)
            file.writeframes(samples)
        info = {"situacao": situation, "bpm": bpm, "compassos": bars, "tonica": "La", "loop": True}
        (project / f"Audio/musica/{situation}.musica.json").write_text(json.dumps(info), encoding="utf-8")
    scene = {"jogador": "heroi", "inimigo_perto": False, "musica": "res://Audio/musica/vila.wav", "ceu": ""}
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


# situation, bpm, bars (of 4 beats), pitch
MUSIC = [("combate", 120, 1, 220.0), ("vila", 240, 2, 330.0)]
MUSIC_FILES = [f"res://Audio/musica/{situation}.wav" for situation, *_ in MUSIC]

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


def read_import_params(project, situation):
    """The [params] of Audio/musica/<situation>.wav.import as {key: text}."""
    params = {}
    in_params = False
    for line in (project / f"Audio/musica/{situation}.wav.import").read_text(encoding="utf-8").splitlines():
        if line.startswith("["):
            in_params = line == "[params]"
        elif in_params and "=" in line:
            key, value = line.split("=", 1)
            params[key] = value
    return params


def music_import_checks(project):
    for situation, bpm, bars, _hertz in MUSIC:
        params = read_import_params(project, situation)
        check(
            params.get("edit/loop_mode") == "2"
            and params.get("edit/loop_begin") == "0"
            and params.get("edit/loop_end") == "-1",
            f"{situation}.wav loops forward over all of it",
        )
        check(
            float(params.get("bpm", "0")) == bpm
            and params.get("beat_count") == str(bars * 4)
            and params.get("bar_beats") == "4",
            f"{situation}.wav has its bpm, beat count and beats per bar",
        )


def results_of(report):
    results(report)
    return report


@contextlib.contextmanager
def editor_session(editor, project, log_file, port, timeout):
    """Runs the editor with its MCP server and yields the connection."""
    environment = dict(os.environ, ERETHEA_MCP_PORT=str(port))
    command = [editor, "--editor", "--headless", "--path", str(project), "--audio-driver", "Dummy"]
    print("Running: " + " ".join(command))
    with open(log_file, "w", encoding="utf-8", errors="replace") as log:
        process = subprocess.Popen(
            command, env=environment, stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL
        )
        try:
            mcp = Mcp(port)
            wait_until_ready(mcp, process, timeout)
            mcp.rpc(
                "initialize",
                {"protocolVersion": "2025-06-18", "capabilities": {}, "clientInfo": {"name": "test", "version": "1"}},
            )
            yield mcp
        finally:
            process.terminate()
            try:
                process.wait(15)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


def build_checks(mcp, project):
    print("First build")
    report = mcp.data("build_fornalha_scenes")
    found = results(report)
    check(list(found) == SCENES, "one scene per character and per area, the world and the main scene, in order")
    check(all(found[scene] == "created" for scene in SCENES[:-1]), "the new scenes are created")
    check(found["res://principal.tscn"] == "kept", "the main scene that the editor didn't make is kept")
    check(any("arena" in warning for warning in report["warnings"]), "an area not in the blockout yet is reported")
    check(report["ok"], "nothing failed")
    check(report.get("music_clips") == ["combate", "vila"], "the report lists the music clips")
    # The hero's texture: imported without a window, nothing drew it in 3D, so
    # Godot left it uncompressed; the build marks it, and the editor compresses it
    # (and, as the project has Erethea's settings, streams its mipmaps).
    check(report.get("compressed_textures") == [HERO_TEXTURE], "the build marks the hero's texture as used in 3D")
    texture_import = project / (HERO_TEXTURE.removeprefix("res://") + ".import")
    deadline = time.monotonic() + 120
    while '"vram_texture": true' not in texture_import.read_text(encoding="utf-8") and time.monotonic() < deadline:
        time.sleep(1)
    check('"vram_texture": true' in texture_import.read_text(encoding="utf-8"), "and the editor compresses it in VRAM")
    check(sorted(report.get("music_imports", [])) == sorted(MUSIC_FILES), "the WAV files are set to import again")
    music_import_checks(project)
    character = (project / "Characters/heroi/heroi.tscn").read_text(encoding="utf-8")
    check('path="res://Characters/heroi/heroi.glb"' in character, "the character scene uses the model's file")
    check('path="res://Scripts/fornalha/jogador.gd"' in character, "the player has the Fornalha's player script")
    check('[node name="BracoCamera"' in character, "the player has its camera")
    enemy = (project / "Enemies/lobo/lobo.tscn").read_text(encoding="utf-8")
    check('path="res://Scripts/fornalha/inimigo.gd"' in enemy, "the enemy has the Fornalha's enemy script")
    forest = (project / "World/areas/floresta.tscn").read_text(encoding="utf-8")
    check(forest.count('instance=ExtResource("') == 2, "the forest has its two enemies")
    check(
        '[node name="Eco" type="Area3D"' in forest and 'reverb_bus_name = &"Eco_floresta"' in forest,
        "the forest echoes as a forest",
    )
    village = (project / "World/areas/vila.tscn").read_text(encoding="utf-8")
    check('reverb_bus_name = &"Eco_vila"' in village, "and the village as a village")
    check(report.get("audio_buses") == ["Eco_vila", "Eco_floresta"], "their reverb buses are added")
    layout = (project / "default_bus_layout.tres").read_text(encoding="utf-8")
    check(
        '"Eco_vila"' in layout and '"Eco_floresta"' in layout and "AudioEffectReverb" in layout,
        "to the project's bus layout",
    )
    world = (project / "World/mundo.tscn").read_text(encoding="utf-8")
    check('path="res://World/blockout.glb"' in world, "the world uses the blockout's file")
    check("directional_shadow_blend_splits = true" in world, "the sun's cascades blend into each other")
    check(
        'path="res://World/areas/vila.tscn"' in world and 'path="res://World/areas/floresta.tscn"' in world,
        "the world has its areas",
    )

    print("Second build")
    report = mcp.data("build_fornalha_scenes")
    found = results(report)
    check(all(found[scene] == "unchanged" for scene in SCENES[:-1]), "nothing changes")
    check("audio_buses" not in report, "the buses aren't added again")
    check(report.get("music_clips") == ["combate", "vila"], "the clips are still listed")
    check(report.get("music_imports") == [], "the import options are not changed again")
    music_import_checks(project)
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
    check(
        '[node name="Musica" type="AudioStreamPlayer" parent="."' in main and 'groups=["erethea_musica"]' in main,
        "the main scene has the Musica node in its group",
    )
    check("volume_db = -8.0" in main, "at the volume of the Sons")
    check(
        main.count('type="AudioStreamWAV"') == 0 and all(f'path="{wav}"' in main for wav in MUSIC_FILES),
        "using the WAV files as external resources",
    )
    check('[sub_resource type="AudioStreamInteractive"' in main, "with an interactive stream")
    check('clip_0/name = &"combate"' in main and 'clip_1/name = &"vila"' in main, "one clip per music, by situation")
    check("initial_clip = 1" in main, "starting by the music of the main scene")
    check("clip_count = 2" in main, "no more clips than music")
    check(
        "Vector2i(-1, -1)" in main
        and '"fade_beats": 2.0' in main
        and '"fade_mode": 3' in main
        and '"from_time": 2' in main
        and '"to_time": 1' in main,
        "with one transition from any clip to any: next bar, to the start, cross fade of two beats",
    )
    check(
        "func tocar(situacao: String)" in main and "switch_to_clip_by_name" in main,
        "and the script that changes the music",
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
        settings = (project / "project.godot").read_text(encoding="utf-8")
        check(
            "erethea/quality_preset=4" in settings and "erethea/temporal_upscaler=4" in settings,
            "the project from the Fornalha gets the settings of new Erethea projects",
        )
        check(
            "erethea/subsurface_scattering/follow_surface=0.05" in settings,
            "but keeps the one it had chosen",
        )
        check("erethea_fornalha/erethea_defaults_applied=PackedStringArray(" in settings, "and remembers them")

        texture_import = project / (HERO_TEXTURE.removeprefix("res://") + ".import")
        check(texture_import.exists(), "the hero's texture is extracted from the model")
        check(
            '"vram_texture": false' in texture_import.read_text(encoding="utf-8"),
            "and, imported without a window, left uncompressed",
        )

        print("Editor with the MCP server")
        imports = {
            situation: (project / f"Audio/musica/{situation}.wav.import").read_text(encoding="utf-8")
            for situation, *_ in MUSIC
        }
        with editor_session(editor, project, out / "editor.log", args.port, args.timeout) as mcp:
            build_checks(mcp, project)

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
        music = walk.get("music", {})
        check(
            music.get("playing") and music.get("clips") == 2 and music.get("current") in ("vila", "combate"),
            "the music plays when the group is asked for a situation",
        )

        print("Without interactive music")
        # The import options as the editor first wrote them, and the setting off.
        for situation, text in imports.items():
            (project / f"Audio/musica/{situation}.wav.import").write_text(text, encoding="utf-8")
        settings = project / "project.godot"
        text = settings.read_text(encoding="utf-8")
        check("[editor]\n" in text, "the project settings have an editor section")
        settings.write_text(
            text.replace("[editor]\n", "[editor]\n\nerethea_fornalha/interactive_music=false\n", 1),
            encoding="utf-8",
        )
        with editor_session(editor, project, out / "editor_no_music.log", args.port, args.timeout) as mcp:
            report = results_of(mcp.data("build_fornalha_scenes", {"force": True}))
            check("music_clips" not in report, "no music clips are reported")
            main = (project / "principal.tscn").read_text(encoding="utf-8")
            check('name="Musica"' not in main and "erethea_musica" not in main, "the main scene has no Musica node")
            check(
                all(
                    (project / f"Audio/musica/{situation}.wav.import").read_text(encoding="utf-8") == text
                    for situation, text in imports.items()
                ),
                "and the import files are left as they were",
            )

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
