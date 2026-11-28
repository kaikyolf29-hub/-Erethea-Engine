#!/usr/bin/env python3
"""End-to-end test of the MCP server inside the Erethea editor (modules/erethea_mcp).

Starts the editor on a copy of the empty project next to this script, with the
server on through ERETHEA_MCP_PORT, and drives it as an MCP client such as
Claude Code would: it creates scenes, nodes and a script, saves, runs the game,
reads the game's errors and prints, and takes screenshots.

    python erethea/tests/mcp/test_mcp.py <editor executable> [--window] [--out <folder>]

Without --window the editor runs with --headless, so screenshots must fail with
a clear message. With --window (on a desktop, or under xvfb-run) they must be
PNG images, which are saved in the --out folder with the editor's log.
"""

import argparse
import base64
import http.client
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent

EXPECTED_TOOLS = {
    "get_editor_state",
    "get_scene_tree",
    "get_node_properties",
    "open_scene",
    "create_scene",
    "create_node",
    "set_node_properties",
    "delete_node",
    "write_script",
    "attach_script",
    "save_scene",
    "run_game",
    "stop_game",
    "get_errors",
    "take_screenshot",
    "rescan_files",
    "check_export",
    "build_fornalha_scenes",
    "run_test",
    "list_files",
    "read_file",
    "edit_file",
}

BOX_SCRIPT = """extends MeshInstance3D


func _ready() -> void:
	print("ERETHEA_MCP_GAME_STARTED")
	push_error("ERETHEA_MCP_GAME_ERROR")


func _process(delta: float) -> void:
	rotate_y(delta)
"""

BROKEN_SCRIPT = """extends Node3D

func _ready(:
	pass
"""


class Failure(Exception):
    pass


def check(condition, message):
    if not condition:
        raise Failure(message)
    print(f"  ok: {message}")


class Mcp:
    def __init__(self, port):
        self.port = port
        self.session = None
        self.last_id = 0

    def http(self, method, path, body=None, headers=None, timeout=60):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=timeout)
        all_headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
        if self.session:
            all_headers["Mcp-Session-Id"] = self.session
        all_headers.update(headers or {})
        data = None if body is None else (body if isinstance(body, bytes) else json.dumps(body).encode())
        connection.request(method, path, body=data, headers=all_headers)
        response = connection.getresponse()
        payload = response.read()
        result_headers = {name.lower(): value for name, value in response.getheaders()}
        connection.close()
        return response.status, result_headers, payload

    def rpc(self, method, params=None, timeout=60):
        self.last_id += 1
        message = {"jsonrpc": "2.0", "id": self.last_id, "method": method, "params": params or {}}
        status, headers, payload = self.http("POST", "/mcp", message, timeout=timeout)
        if status != 200:
            raise Failure(f"{method}: HTTP {status}: {payload[:500]!r}")
        reply = json.loads(payload)
        if reply.get("id") != self.last_id:
            raise Failure(f"{method}: the answer has id {reply.get('id')!r}, expected {self.last_id}")
        if method == "initialize":
            self.session = headers.get("mcp-session-id")
        return reply

    def call(self, tool, arguments=None, expect_error=False, timeout=60):
        reply = self.rpc("tools/call", {"name": tool, "arguments": arguments or {}}, timeout=timeout)
        if "error" in reply:
            raise Failure(f"{tool}: JSON-RPC error {reply['error']}")
        result = reply["result"]
        text = next((block.get("text", "") for block in result["content"] if block.get("type") == "text"), "")
        if result["isError"] != expect_error:
            raise Failure(f"{tool} {'succeeded' if expect_error else 'failed'}: {text[:1000]}")
        return result, text

    def data(self, tool, arguments=None, expect_error=False):
        result, text = self.call(tool, arguments, expect_error)
        return result.get("structuredContent") or json.loads(text)


class KeptAlive:
    """Sends every request on one connection, as .NET, Node and most HTTP clients do."""

    def __init__(self, mcp):
        self.mcp = mcp
        self.connection = http.client.HTTPConnection("127.0.0.1", mcp.port, timeout=60)
        self.headers = {}

    def rpc(self, method, params=None):
        self.mcp.last_id += 1
        message = {"jsonrpc": "2.0", "id": self.mcp.last_id, "method": method, "params": params or {}}
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
            "Mcp-Session-Id": self.mcp.session,
        }
        try:
            self.connection.request("POST", "/mcp", body=json.dumps(message).encode(), headers=headers)
            response = self.connection.getresponse()
            payload = response.read()
        except (http.client.HTTPException, ConnectionError) as error:
            raise Failure(f"{method}: the server dropped the kept-alive connection ({error!r})")
        self.headers = {name.lower(): value for name, value in response.getheaders()}
        if response.status != 200:
            raise Failure(f"{method}: HTTP {response.status}: {payload[:500]!r}")
        if self.headers.get("connection", "").lower() == "close":
            raise Failure(f"{method}: the server closed the kept-alive connection")
        return json.loads(payload)

    def call(self, tool, arguments=None):
        reply = self.rpc("tools/call", {"name": tool, "arguments": arguments or {}})
        if "error" in reply:
            raise Failure(f"{tool}: JSON-RPC error {reply['error']}")
        return reply["result"]

    def close(self):
        self.connection.close()


def write_noise_png(path, size):
    """An RGB image of random pixels, which takes the editor a while to import."""

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    rows = b"".join(b"\x00" + os.urandom(size * 3) for _ in range(size))
    header = struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows, 1)) + chunk(b"IEND", b"")
    )


def write_triangle_gltf(path):
    """A glTF model with one triangle, its buffer inside the file."""
    positions = struct.pack("<9f", 0, 0, 0, 1, 0, 0, 0, 1, 0)
    model = {
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "Triangle", "mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
        "buffers": [
            {
                "byteLength": len(positions),
                "uri": "data:application/octet-stream;base64," + base64.b64encode(positions).decode(),
            }
        ],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": len(positions)}],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": 3,
                "type": "VEC3",
                "min": [0, 0, 0],
                "max": [1, 1, 0],
            }
        ],
    }
    path.write_text(json.dumps(model), encoding="utf-8")


def check_connections_and_imports(mcp, project, window):
    print("Kept-alive connection and imports")
    kept = KeptAlive(mcp)
    for _ in range(300):
        kept.rpc("ping")
    check(True, "300 requests on one connection, none dropped")
    check(kept.headers.get("keep-alive") == "timeout=110", "the server says how long it keeps idle connections")

    # Textures that take some seconds to import, while tools are called. With a
    # window, the editor's progress dialog keeps the server answering during
    # the import, and the tools that need the editor's files answer that it's
    # busy. Headless, the editor prints its progress instead, and the requests
    # wait for the import to end. Either way, no connection is dropped.
    folder = project / "mcp_test" / "noise"
    folder.mkdir(parents=True)
    count = 16
    for index in range(count):
        write_noise_png(folder / f"noise_{index}.png", 1024)
    write_triangle_gltf(project / "mcp_test" / "triangle.gltf")
    kept.call("rescan_files")
    busy = 0
    importing_seen = False
    imported = False
    deadline = time.time() + 180
    while time.time() < deadline:
        result = kept.call("get_scene_tree")
        if result["isError"]:
            text = result["content"][0]["text"]
            if (result.get("structuredContent") or {}).get("busy") != "importing":
                raise Failure(f"get_scene_tree failed during the import: {text[:500]}")
            busy += 1
        state = kept.call("get_editor_state")["structuredContent"]
        importing_seen = importing_seen or state["importing"]
        imported = (project / "mcp_test" / "triangle.gltf.import").exists() and all(
            (folder / f"noise_{index}.png.import").exists() for index in range(count)
        )
        if imported and not state["importing"] and not state["scanning"] and not result["isError"]:
            break
        time.sleep(0.05)
    check(imported, f"the {count} textures and a model were imported")
    if window:
        check(importing_seen, "get_editor_state said the editor was importing")
        check(busy > 0, f"get_scene_tree answered {busy} times that the editor was busy importing")
    check(not kept.call("get_scene_tree")["isError"], "after the import, the tools work again")
    kept.close()

    print("3D models")
    model = mcp.data("create_node", {"scene": "res://mcp_test/triangle.gltf", "properties": {"position": [0, 0, -3]}})
    check(model["node"] == "Triangle" and model["type"] == "Node3D", "a glTF model is instanced as a scene")
    check(
        find_node(mcp.data("get_scene_tree")["root"], "Triangle")["scene"] == "res://mcp_test/triangle.gltf",
        "the tree shows the model's file",
    )
    _, text = mcp.call("create_node", {"scene": "res://mcp_test/noise/noise_0.png"}, expect_error=True)
    check("CompressedTexture2D" in text, "a texture isn't instanced, and the error says what it is")
    write_triangle_gltf(project / "mcp_test" / "late.gltf")
    _, text = mcp.call("create_node", {"scene": "res://mcp_test/late.gltf"}, expect_error=True)
    check("rescan_files" in text, "a model not imported yet asks for rescan_files")


def wait_until_ready(mcp, editor, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if editor.poll() is not None:
            raise Failure(f"the editor exited with code {editor.returncode} before its MCP server answered")
        try:
            status, _, payload = mcp.http("GET", "/status", timeout=5)
            if status == 200 and json.loads(payload).get("ready"):
                return json.loads(payload)
        except OSError:
            pass
        time.sleep(1)
    raise Failure(f"the MCP server didn't answer within {timeout} s")


def find_node(tree, name):
    if tree.get("name") == name:
        return tree
    for child in tree.get("children", []):
        found = find_node(child, name)
        if found:
            return found
    return None


def save_screenshot(result, out, name):
    images = [block for block in result["content"] if block.get("type") == "image"]
    check(len(images) == 1 and images[0]["mimeType"] == "image/png", f"{name}: one PNG image")
    png = base64.b64decode(images[0]["data"])
    check(png.startswith(b"\x89PNG\r\n\x1a\n"), f"{name}: valid PNG data ({len(png)} bytes)")
    (out / f"{name}.png").write_bytes(png)


def run_checks(mcp, project, window, out):
    print("Security")
    status, _, _ = mcp.http("POST", "/mcp", {"jsonrpc": "2.0", "id": 1, "method": "ping"}, {"Host": "example.com"})
    check(status == 403, "a request for another host is refused")
    status, _, _ = mcp.http(
        "POST", "/mcp", {"jsonrpc": "2.0", "id": 1, "method": "ping"}, {"Origin": "https://example.com"}
    )
    check(status == 403, "a request from a web page is refused")
    status, _, _ = mcp.http("GET", "/mcp")
    check(status == 405, "GET /mcp is refused (no server-sent events)")

    print("Session")
    reply = mcp.rpc(
        "initialize",
        {"protocolVersion": "2025-06-18", "capabilities": {}, "clientInfo": {"name": "test_mcp.py", "version": "1"}},
    )
    check(reply["result"]["serverInfo"]["name"] == "erethea", "the server is Erethea")
    check(reply["result"]["protocolVersion"] == "2025-06-18", "protocol version 2025-06-18")
    check(mcp.session and len(mcp.session) == 32, "a session id")
    status, _, _ = mcp.http("POST", "/mcp", {"jsonrpc": "2.0", "method": "notifications/initialized"})
    check(status == 202, "notifications are accepted")
    tools = {tool["name"] for tool in mcp.rpc("tools/list")["result"]["tools"]}
    check(tools == EXPECTED_TOOLS, f"{len(tools)} tools")
    status, _, payload = mcp.http(
        "POST",
        "/mcp",
        [
            {"jsonrpc": "2.0", "id": 100, "method": "ping"},
            {"jsonrpc": "2.0", "id": 101, "method": "tools/call", "params": {"name": "get_editor_state"}},
        ],
    )
    check(status == 200 and len(json.loads(payload)) == 2, "a batch gets two answers")

    print("Editor state")
    state = mcp.data("get_editor_state")
    check(state["project_name"] == "Erethea MCP Test", "the test project is open")
    check(state["playing"] is False, "the game isn't running")
    check(state["headless"] is (not window), f"headless is {not window}")

    print("Scenes")
    mcp.data("create_scene", {"path": "res://mcp_test/enemy.tscn", "root_type": "CharacterBody3D"})
    created = mcp.data("create_scene", {"path": "mcp_test/main.tscn"})
    check(
        created["scene"] == "res://mcp_test/main.tscn" and created["root"] == "Main",
        "main.tscn created with its root Main",
    )
    mcp.call("create_scene", {"path": "res://mcp_test/main.tscn"}, expect_error=True)
    print("  ok: an existing scene isn't overwritten")
    mcp.call("open_scene", {"path": "res://mcp_test/nothing.tscn"}, expect_error=True)
    print("  ok: a missing scene can't be opened")
    check(
        mcp.data("open_scene", {"path": "res://mcp_test/main.tscn"})["scene"] == "res://mcp_test/main.tscn",
        "main.tscn is open",
    )

    print("Nodes")
    box = mcp.data(
        "create_node",
        {
            "type": "MeshInstance3D",
            "name": "Box",
            "properties": {"mesh": {"type": "BoxMesh", "properties": {"size": [2, 1, 1]}}, "position": [0, 1, 0]},
        },
    )
    check(box["node"] == "Box" and box["type"] == "MeshInstance3D", "Box created")
    mcp.data(
        "create_node",
        {"type": "Camera3D", "name": "Camera", "properties": {"position": [0, 2, 5], "rotation_degrees": [-15, 0, 0]}},
    )
    mcp.data("create_node", {"type": "DirectionalLight3D", "properties": {"rotation_degrees": "Vector3(-45, 30, 0)"}})
    changed = mcp.data(
        "set_node_properties",
        {
            "node": "Box",
            "properties": {"position": "Vector3(0, 0.5, 0)", "rotation_degrees": [0, 45, 0], "visible": True},
        },
    )
    check(changed["properties"]["position"] == "Vector3(0, 0.5, 0)", "set_node_properties returns the new values")
    _, text = mcp.call("set_node_properties", {"node": "Box", "properties": {"nope": 1}}, expect_error=True)
    check("nope" in text, "an unknown property is refused")
    _, text = mcp.call("set_node_properties", {"node": "Box", "properties": {"position": [1, 2]}}, expect_error=True)
    check("Vector3" in text, "a wrong value is refused with an example")
    properties = mcp.data("get_node_properties", {"node": "Box", "properties": ["position", "mesh"]})["properties"]
    check(properties["position"] == "Vector3(0, 0.5, 0)", "get_node_properties reads the position")
    check(properties["mesh"] == {"type": "BoxMesh"}, "the mesh is a BoxMesh")
    enemy = mcp.data("create_node", {"scene": "res://mcp_test/enemy.tscn", "properties": {"position": [3, 0, 0]}})
    check(enemy["node"] == "Enemy" and enemy["type"] == "CharacterBody3D", "enemy.tscn instanced")
    mcp.data("create_node", {"type": "Node3D", "name": "Temporary"})
    mcp.data("delete_node", {"node": "Temporary"})
    mcp.call("delete_node", {"node": "."}, expect_error=True)
    print("  ok: the root can't be deleted")
    mcp.call("create_node", {"type": "EditorPlugin"}, expect_error=True)
    print("  ok: editor classes can't be added")
    tree = mcp.data("get_scene_tree")
    check(find_node(tree["root"], "Temporary") is None, "Temporary was deleted")
    check(
        find_node(tree["root"], "Enemy")["scene"] == "res://mcp_test/enemy.tscn", "the tree shows the instanced scene"
    )

    print("Scripts")
    result, _ = mcp.call(
        "write_script", {"path": "res://mcp_test/broken.gd", "source": BROKEN_SCRIPT}, expect_error=True
    )
    errors = result["structuredContent"]["errors"]
    check(
        errors and errors[0]["line"] == 3,
        f"the broken script's error is on line 3 ({errors[0]['message'] if errors else ''})",
    )
    written = mcp.data("write_script", {"path": "res://mcp_test/box.gd", "source": BOX_SCRIPT})
    check(written["valid"] is True, "box.gd has no errors")
    mcp.data("attach_script", {"node": "Box", "path": "res://mcp_test/box.gd"})
    mcp.call("attach_script", {"node": "Camera", "path": "res://mcp_test/box.gd"}, expect_error=True)
    print("  ok: a script for another type is refused")
    check(
        find_node(mcp.data("get_scene_tree")["root"], "Box").get("script") == "res://mcp_test/box.gd", "Box has box.gd"
    )

    print("Saving")
    mcp.data("save_scene")
    saved = (project / "mcp_test" / "main.tscn").read_text(encoding="utf-8")
    check(
        "BoxMesh" in saved and "box.gd" in saved and "enemy.tscn" in saved,
        "main.tscn on disk has the mesh, the script and the instance",
    )
    check((project / "mcp_test" / "box.gd").read_text(encoding="utf-8") == BOX_SCRIPT, "box.gd on disk")

    check_connections_and_imports(mcp, project, window)

    print("Checks before export")
    report = mcp.data("check_export")
    check(
        report["fornalha_project"] is False and report["errors"] == 0 and report["warnings"] == 0,
        "nothing to fix in a plain project with no assets",
    )
    mcp.call("check_export", {"preset": "Windows"}, expect_error=True)
    print("  ok: an unknown export preset is refused")

    print("Scenes from game.project.json")
    _, text = mcp.call("build_fornalha_scenes", {}, expect_error=True)
    check("game.project.json" in text, "a plain project has none to build")

    print("Game")
    mcp.call("run_game", {}, expect_error=True)
    print("  ok: without a main scene, run_game asks for one")
    if not window:
        mcp.call("take_screenshot", {"target": "editor"}, expect_error=True)
        print("  ok: a headless editor has nothing to capture")
    run = mcp.data("run_game", {"scene": "res://mcp_test/main.tscn", "headless": not window})
    check(run["playing"] is True, "the game runs")
    started = False
    game_error = None
    deadline = time.time() + 90
    while time.time() < deadline and not (started and game_error):
        log = mcp.data("get_errors", {"since": run["errors_since"], "include_output": True})
        for entry in log["entries"]:
            if entry["source"] == "game" and "ERETHEA_MCP_GAME_STARTED" in entry["message"]:
                started = True
            if entry["source"] == "game" and entry["level"] == "error" and "ERETHEA_MCP_GAME_ERROR" in entry["message"]:
                game_error = entry
        time.sleep(0.5)
    check(started, "get_errors has the game's print")
    check(game_error is not None, "get_errors has the game's error")
    check(
        game_error.get("file") == "res://mcp_test/box.gd" and game_error.get("line") == 6,
        f"the error points at box.gd:6 ({game_error.get('file')}:{game_error.get('line')})",
    )
    only_errors = mcp.data("get_errors", {"since": run["errors_since"]})
    check(all(entry["level"] != "output" for entry in only_errors["entries"]), "without include_output, no prints")

    if window:
        time.sleep(2)
        result, _ = mcp.call("take_screenshot", {"target": "game", "max_size": 800}, timeout=30)
        save_screenshot(result, out, "game")
        result, _ = mcp.call("take_screenshot", {"target": "editor"}, timeout=30)
        save_screenshot(result, out, "editor")
    else:
        _, text = mcp.call("take_screenshot", {"target": "game"}, expect_error=True, timeout=30)
        check("headless" in text, "a headless game sends no screenshot, and the error says why")

    stopped = mcp.data("stop_game")
    check(stopped["playing"] is False and stopped["was_playing"] is True, "the game stopped")
    check(mcp.data("get_editor_state")["playing"] is False, "the editor knows")
    if window:
        result, _ = mcp.call("take_screenshot", {"target": "3d"}, timeout=30)
        save_screenshot(result, out, "editor_3d")

    print("Test mode")
    arguments = {"scene": "res://mcp_test/main.tscn", "seconds": 2, "headless": not window}
    if window:
        arguments["screenshots"] = [1.0]
    result, text = mcp.call("run_test", arguments, timeout=90)
    report = result.get("structuredContent") or json.loads(text)
    check(report["finished"] and report["ended"] == "time", f"the game played its {report['seconds']} s")
    check(report["fps"]["frames"] > 0, f"at {report['fps'].get('average_fps')} FPS")
    error = next((entry for entry in report["errors"] if entry["message"] == "ERETHEA_MCP_GAME_ERROR"), None)
    check(
        error is not None and error.get("script_file") == "res://mcp_test/box.gd",
        "the report has the game's error, in box.gd",
    )
    check(report["passed"] is False, "so the test didn't pass")
    if window:
        save_screenshot(result, out, "test")

    print("End of session")
    status, _, _ = mcp.http("DELETE", "/mcp")
    check(status == 204, "the session is closed")
    status, _, _ = mcp.http("POST", "/mcp", {"jsonrpc": "2.0", "id": 1, "method": "ping"})
    check(status == 404, "the closed session is unknown")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("editor", help="the Erethea editor's executable")
    parser.add_argument("--window", action="store_true", help="run the editor with a window and check the screenshots")
    parser.add_argument("--out", default="mcp_test_output", help="folder for the screenshots and the editor's log")
    parser.add_argument("--port", type=int, default=11491)
    parser.add_argument("--timeout", type=int, default=300, help="seconds to wait for the editor to start")
    arguments = parser.parse_args()

    out = Path(arguments.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="erethea_mcp_"))
    project = work / "project"
    shutil.copytree(HERE / "project", project)

    environment = dict(os.environ, ERETHEA_MCP_PORT=str(arguments.port))
    command = [str(Path(arguments.editor).resolve()), "--editor", "--path", str(project), "--audio-driver", "Dummy"]
    if not arguments.window:
        command.append("--headless")
    log_path = out / "editor.log"
    print("Running:", " ".join(command))

    failure = None
    with open(log_path, "w", encoding="utf-8", errors="replace") as log:
        editor = subprocess.Popen(
            command, env=environment, stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL
        )
        try:
            mcp = Mcp(arguments.port)
            status = wait_until_ready(mcp, editor, arguments.timeout)
            print(f"MCP server ready: {status['mcp']} ({status['version']})")
            run_checks(mcp, project, arguments.window, out)
        except (Failure, OSError, KeyError, TypeError, ValueError) as error:
            failure = error
        finally:
            editor.terminate()
            try:
                editor.wait(15)
            except subprocess.TimeoutExpired:
                editor.kill()
                editor.wait()

    log_text = log_path.read_text(encoding="utf-8", errors="replace")
    mcp_lines = [line for line in log_text.splitlines() if "Erethea MCP" in line]
    print("\nEditor log lines about MCP:\n" + "\n".join(mcp_lines))
    shutil.rmtree(work, ignore_errors=True)
    if failure:
        print("\nLast lines of the editor's log:\n" + "\n".join(log_text.splitlines()[-80:]))
        print(f"\nFAILED: {failure}")
        return 1
    print("\nERETHEA_MCP_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
