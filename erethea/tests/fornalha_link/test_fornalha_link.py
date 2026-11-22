#!/usr/bin/env python3
"""End-to-end test of the live link with the Fornalha (modules/erethea_fornalha).

Starts a stand-in for the Fornalha's API (GET /fornalha/ponte and its
WebSocket) and opens the editor on a new project made like the Fornalha makes
them: the player and the blockout are there, but no scene yet.

- The editor builds the scenes by itself, since the project has none.
- Over HTTP, as the Fornalha's bridge add-on does: the Fornalha writes an enemy,
  says so, and the editor imports it and builds its scene.
- Through the WebSocket, once the Fornalha's answer names one: the editor says
  hello, the Fornalha pushes the next enemy, and the editor sends back what it
  built.

Last, it runs the game without a window and walks the player.

    python erethea/tests/fornalha_link/test_fornalha_link.py <editor executable> [--out <folder>]
"""

import argparse
import base64
import hashlib
import json
import os
import select
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

HERE = Path(__file__).resolve().parent
sys.dont_write_bytecode = True  # No __pycache__ in the repository.
sys.path.insert(0, str(HERE.parent / "fornalha_import"))

from test_fornalha_import import Glb, run  # noqa: E402
from test_mcp import Failure, check  # noqa: E402

WEBSOCKET_PATH = "/fornalha/ponte/ws"
WEBSOCKET_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


class Fornalha:
    """What the editor sees of the Fornalha: its bridge, over HTTP and WebSocket."""

    def __init__(self, port):
        self.lock = threading.Lock()
        self.version = 1
        self.files = []
        self.websocket = False  # Whether the HTTP answer names the WebSocket.
        self.http_projects = []
        self.received = []  # Messages from the editor through the WebSocket.
        self.websocket_connections = 0
        fornalha = self

        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *args):
                pass

            def do_GET(self):
                url = urlparse(self.path)
                if url.path == "/fornalha/ponte":
                    fornalha._bridge(self, parse_qs(url.query).get("projeto", [""])[0])
                elif url.path == WEBSOCKET_PATH and self.headers.get("Upgrade", "").lower() == "websocket":
                    fornalha._websocket(self)
                else:
                    self.send_error(404)

        self.server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
        self.server.daemon_threads = True
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def stop(self):
        self.server.shutdown()
        self.server.server_close()

    def wrote(self, files):
        """The Fornalha wrote files in the project."""
        with self.lock:
            self.version += 1
            self.files = files

    def _bridge(self, handler, project):
        with self.lock:
            self.http_projects.append(project)
            answer = {"versao": self.version, "arquivos": self.files}
            if self.websocket:
                answer["websocket"] = WEBSOCKET_PATH
        body = json.dumps(answer).encode()
        handler.send_response(200)
        handler.send_header("Content-Type", "application/json")
        handler.send_header("Content-Length", str(len(body)))
        handler.end_headers()
        handler.wfile.write(body)

    def _websocket(self, handler):
        accept = base64.b64encode(
            hashlib.sha1((handler.headers["Sec-WebSocket-Key"] + WEBSOCKET_GUID).encode()).digest()
        )
        handler.send_response(101)
        handler.send_header("Upgrade", "websocket")
        handler.send_header("Connection", "Upgrade")
        handler.send_header("Sec-WebSocket-Accept", accept.decode())
        handler.end_headers()
        handler.wfile.flush()
        handler.close_connection = True
        connection = handler.connection
        with self.lock:
            self.websocket_connections += 1
        sent = None
        buffer = b""
        while True:
            with self.lock:
                version, files = self.version, self.files
            if version != sent:
                sent = version
                message = json.dumps({"tipo": "ponte", "versao": version, "arquivos": files}).encode()
                connection.sendall(_frame(message))
            if select.select([connection], [], [], 0.1)[0]:
                data = connection.recv(65536)
                if not data:
                    return
                buffer += data
            while True:
                parsed = _read_frame(buffer)
                if parsed is None:
                    break
                opcode, payload, buffer = parsed
                if opcode == 1:
                    with self.lock:
                        self.received.append(json.loads(payload.decode()))
                elif opcode == 8:
                    connection.sendall(_frame(payload[:2], 8))
                    return
                elif opcode == 9:
                    connection.sendall(_frame(payload, 10))


def _frame(payload, opcode=1):
    """A frame from the server: not masked."""
    head = bytes([0x80 | opcode])
    if len(payload) < 126:
        head += bytes([len(payload)])
    elif len(payload) < 65536:
        head += bytes([126]) + struct.pack(">H", len(payload))
    else:
        head += bytes([127]) + struct.pack(">Q", len(payload))
    return head + payload


def _read_frame(buffer):
    """A whole frame from the client (masked), as (opcode, payload, rest), or None."""
    if len(buffer) < 2:
        return None
    opcode, length, offset = buffer[0] & 0x0F, buffer[1] & 0x7F, 2
    if length == 126:
        if len(buffer) < 4:
            return None
        length, offset = struct.unpack(">H", buffer[2:4])[0], 4
    elif length == 127:
        if len(buffer) < 10:
            return None
        length, offset = struct.unpack(">Q", buffer[2:10])[0], 10
    masked = buffer[1] & 0x80
    mask = buffer[offset : offset + 4] if masked else b"\0\0\0\0"
    offset += 4 if masked else 0
    if len(buffer) < offset + length:
        return None
    payload = bytes(b ^ mask[i % 4] for i, b in enumerate(buffer[offset : offset + length]))
    return opcode, payload, buffer[offset + length :]


def make_character(project, folder, name, size):
    model = Glb()
    model.box("Corpo", size, (0.0, 0.0, 0.0))
    for animation in ("parado", "andar", "correr", "atacar"):
        model.animation(animation, 0.05)
    model.save(project / folder / name / f"{name}.glb")
    forja = {"estatisticas": {"altura_m": size[1]}}
    (project / folder / name / f"{name}.forja.json").write_text(json.dumps(forja), encoding="utf-8")


def make_project(project):
    """The player and the blockout, as the Fornalha leaves them before the enemies."""
    make_character(project, "Characters", "heroi", (0.5, 1.7, 0.3))
    world = Glb()
    world.box("chao-col", (160.0, 1.0, 160.0), (0.0, -1.0, 0.0))
    world.box("vila_casa-col", (4.0, 3.0, 4.0), (10.0, 0.0, 10.0))
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


def wait_for(what, condition, process, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            result = condition()
        except PermissionError:
            # On Windows, a file can't be opened while the editor writes it.
            result = None
        if result:
            return result
        if process.poll() is not None:
            raise Failure(f"the editor quit (exit code {process.returncode}) while waiting until {what}")
        time.sleep(0.25)
    raise Failure(f"timed out waiting until {what}")


def link_checks(fornalha, project, process, log_path, timeout):
    main = project / "principal.tscn"
    record = project / ".erethea/fornalha_import.json"
    print("A new project")
    # The record is written just after the main scene: wait for both, or the
    # check can land between the two.
    wait_for("the editor builds the scenes", lambda: main.is_file() and record.is_file(), process, timeout)
    check((project / "Characters/heroi/heroi.tscn").is_file(), "the player's scene is built")
    check((project / "World/mundo.tscn").is_file(), "the world is built")
    check(record.is_file(), "and what was built is recorded")

    print("Over HTTP")
    expected_project = str(project).replace("\\", "/").rstrip("/") + "/"
    wait_for("the editor asks the Fornalha", lambda: fornalha.http_projects, process, timeout)
    asked = fornalha.http_projects[-1].replace("\\", "/")
    check(asked.lower() == expected_project.lower(), f"for this project ({asked})")
    make_character(project, "Enemies", "lobo", (0.4, 0.8, 1.0))
    fornalha.wrote(["Enemies/lobo/lobo.glb", "Enemies/lobo/lobo.forja.json"])
    wolf = project / "Enemies/lobo/lobo.tscn"
    forest = project / "World/areas/floresta.tscn"
    wait_for("the editor builds the new enemy's scene", wolf.is_file, process, timeout)
    wait_for(
        "the enemy goes into the forest",
        lambda: "res://Enemies/lobo/lobo.tscn" in forest.read_text(encoding="utf-8"),
        process,
        timeout,
    )
    print("    The enemy is imported, gets its scene and goes into its area.")
    log = log_path.read_text(encoding="utf-8", errors="replace")
    check("the Fornalha wrote new files (Enemies/lobo/lobo.glb" in log, "the editor says what the Fornalha wrote")

    print("Through the WebSocket")
    fornalha.websocket = True
    wait_for(
        "the editor says hello through the WebSocket",
        lambda: any(message.get("tipo") == "ola" for message in fornalha.received),
        process,
        timeout,
    )
    hello = next(message for message in fornalha.received if message.get("tipo") == "ola")
    print(f"    {hello}")
    check(hello["projeto"].replace("\\", "/").lower() == expected_project.lower(), "for this project")
    check(hello["monta_cenas"] is True, "saying that it builds the scenes")
    asked_before = len(fornalha.http_projects)
    make_character(project, "Enemies", "urso", (0.9, 1.4, 1.6))
    fornalha.wrote(["Enemies/urso/urso.glb", "Enemies/urso/urso.forja.json"])
    report = wait_for(
        "the editor sends what it built",
        lambda: next(
            (
                message
                for message in fornalha.received
                if message.get("tipo") == "cenas"
                and any(scene["file"] == "res://Enemies/urso/urso.tscn" for scene in message.get("scenes", []))
            ),
            None,
        ),
        process,
        timeout,
    )
    print(f"    {report['summary']}")
    results = {scene["file"]: scene["result"] for scene in report["scenes"]}
    check(results["res://Enemies/urso/urso.tscn"] == "created", "the new enemy's scene is created")
    check(results["res://Characters/heroi/heroi.tscn"] == "unchanged", "the player's scene doesn't change")
    check(len(fornalha.http_projects) <= asked_before + 1, "and the editor stops asking over HTTP")
    check(fornalha.websocket_connections == 1, "with one connection")

    log = log_path.read_text(encoding="utf-8", errors="replace")
    check("live link with the Fornalha open" in log, "the editor says the live link is open")
    errors = [line for line in log.splitlines() if "ERROR" in line and ("Erethea" in line or "WebSocket" in line)]
    check(not errors, "no errors from the live link" + "".join("\n      " + line for line in errors))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("editor")
    parser.add_argument("--out", type=Path, default=None, help="Folder for the logs.")
    parser.add_argument("--port", type=int, default=11493, help="Port of the stand-in for the Fornalha.")
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--keep", action="store_true", help="Keep the project, to look into a failure.")
    args = parser.parse_args()

    editor = str(Path(args.editor).resolve())
    out = (args.out or Path(tempfile.mkdtemp(prefix="erethea_fornalha_link_out_"))).resolve()
    out.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="erethea_fornalha_link_"))
    project = work / "project"
    shutil.copytree(HERE.parent / "fornalha_import/project", project)
    make_project(project)
    fornalha = Fornalha(args.port)

    try:
        print("Import")
        code, _ = run([editor, "--headless", "--import", "--path", str(project)], out / "import.log", 600)
        check(code == 0, f"the project imports (exit code {code})")
        check(not (project / "principal.tscn").exists(), "importing builds no scene")

        print("Editor")
        environment = dict(os.environ, ERETHEA_FORNALHA_PORT=str(args.port))
        command = [editor, "--editor", "--headless", "--path", str(project), "--audio-driver", "Dummy"]
        print("Running: " + " ".join(command))
        log_path = out / "editor.log"
        with open(log_path, "w", encoding="utf-8", errors="replace") as log:
            process = subprocess.Popen(
                command, env=environment, stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL
            )
            try:
                link_checks(fornalha, project, process, log_path, args.timeout)
            finally:
                process.terminate()
                try:
                    process.wait(15)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()

        print("The game")
        code, output = run(
            [editor, "--headless", "--path", str(project), "--script", "res://test_walk.gd"], out / "game.log", 600
        )
        line = next((line for line in output.splitlines() if line.startswith("ERETHEA_WALK ")), None)
        check(line is not None, f"the game runs (exit code {code})")
        walk = json.loads(line.split(" ", 1)[1])
        print(f"    {walk}")
        check(
            walk["player"] and walk["walked_m"] > 2.0 and walk["on_floor"] and not walk["fell"],
            f"the player walks on the ground ({walk.get('walked_m', 0):.1f} m)",
        )
        check(walk["enemies"] == 3, "the enemies the Fornalha sent are in the game")
    except Failure as failure:
        print(f"\nFAILED: {failure}\nLogs in {out}")
        return 1
    finally:
        fornalha.stop()
        if args.keep:
            print(f"The project is in {project}")
        else:
            shutil.rmtree(work, ignore_errors=True)

    print("\nERETHEA_FORNALHA_LINK_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
