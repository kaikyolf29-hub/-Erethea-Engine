#!/usr/bin/env python3
"""End-to-end test of the assistant panel inside the Erethea editor (modules/erethea_assistant).

Starts the editor on a copy of the project next to this script, with a fake model
server that answers as the Anthropic API (Claude), an OpenAI-compatible server and
Ignis. A small editor plugin in the project (addons/assistant_driver) types in the
panel and presses its buttons when this script asks. The script then checks, through
the editor's MCP server and the project's files, that the answers changed the scene
and the scripts, and that "Undo last answer" put them back; and, through the fake
server, that the questions carried the editor's context and the tools' results.

    python erethea/tests/assistant/test_assistant.py <editor executable> [--out <folder>]

Nothing here reaches a real model: the real test with Ignis and Claude is part of
the final test on the creator's computer.
"""

import argparse
import http.client
import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HERE = Path(__file__).resolve().parent

ENEMY_SCRIPT = "extends Node3D\n\n\nfunc _process(delta: float) -> void:\n\trotate_y(delta)\n"
PLAYER_SCRIPT = "extends Node3D\n\n\nfunc _ready() -> void:\n\tpass\n"

# Claude's first answer, with spacing and escapes that only survive if the panel
# sends it back byte for byte, as Claude's thinking blocks require.
CLAUDE_CONTENT_1 = (
    '[{"type":"thinking","thinking":"","signature":"c2lnbmF0dXJl\\u002b=="},'
    '{"type":"text","text":"Adding an enemy."},  '
    '{"type":"tool_use","id":"toolu_01","name":"create_node",'
    '"input":{"type":"Node3D","name":"Enemy","properties":{"position":[1, 0, 2.5]}}}]'
)


class Failure(Exception):
    pass


def check(condition, message):
    if not condition:
        raise Failure(message)
    print(f"  ok: {message}")


def claude_message(content, stop_reason):
    return (
        '{"id":"msg_test","type":"message","role":"assistant","model":"claude-opus-5-5","content":'
        + content
        + f',"stop_reason":"{stop_reason}","usage":{{"input_tokens":10,"output_tokens":5}}}}'
    )


def openai_message(message, finish_reason):
    return json.dumps({
        "id": "chatcmpl-test",
        "choices": [{"index": 0, "finish_reason": finish_reason, "message": message}],
    })


class FakeModels:
    """Answers each endpoint from a list of prepared answers and keeps the requests."""

    def __init__(self):
        self.answers = {
            "/anthropic/v1/messages": [],
            "/openai/v1/chat/completions": [],
            "/ignis/v1/chat/completions": [],
        }
        self.requests = {path: [] for path in self.answers}
        self.lock = threading.Lock()
        fake = self

        class Handler(BaseHTTPRequestHandler):
            def do_POST(self):
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length).decode("utf-8")
                with fake.lock:
                    queue = fake.answers.get(self.path)
                    if queue is None:
                        status, answer = 404, '{"error": "unknown path"}'
                    else:
                        fake.requests[self.path].append({"headers": dict(self.headers.items()), "body": body})
                        status, answer = (
                            queue.pop(0) if queue else (500, '{"error": {"message": "no answer prepared"}}')
                        )
                data = answer.encode("utf-8")
                self.send_response(status)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def log_message(self, format, *args):
                pass

        self.server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.port = self.server.server_address[1]
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def prepare(self, path, *answers):
        with self.lock:
            self.answers[path].extend(answers)

    def taken(self, path):
        with self.lock:
            return list(self.requests[path])


class Mcp:
    def __init__(self, port):
        self.port = port
        self.session = None
        self.last_id = 0

    def http(self, method, path, body=None, timeout=60):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=timeout)
        headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
        if self.session:
            headers["Mcp-Session-Id"] = self.session
        connection.request(method, path, body=None if body is None else json.dumps(body).encode(), headers=headers)
        response = connection.getresponse()
        payload = response.read()
        result_headers = {name.lower(): value for name, value in response.getheaders()}
        connection.close()
        return response.status, result_headers, payload

    def rpc(self, method, params=None):
        self.last_id += 1
        status, headers, payload = self.http(
            "POST", "/mcp", {"jsonrpc": "2.0", "id": self.last_id, "method": method, "params": params or {}}
        )
        if status != 200:
            raise Failure(f"{method}: HTTP {status}: {payload[:500]!r}")
        if method == "initialize":
            self.session = headers.get("mcp-session-id")
        return json.loads(payload)

    def call(self, tool, arguments=None, expect_error=False):
        result = self.rpc("tools/call", {"name": tool, "arguments": arguments or {}})["result"]
        text = next((block.get("text", "") for block in result["content"] if block.get("type") == "text"), "")
        if result["isError"] != expect_error:
            raise Failure(f"{tool} {'succeeded' if expect_error else 'failed'}: {text[:1000]}")
        return result.get("structuredContent") or {}

    def node_names(self):
        tree = self.call("get_scene_tree")
        return [child["name"] for child in tree["root"].get("children", [])]


class Driver:
    """Talks to addons/assistant_driver, which uses the panel inside the editor."""

    def __init__(self, project, editor):
        self.folder = project / ".driver"
        self.editor = editor
        self.next = 1

    def wait_ready(self, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.editor.poll() is not None:
                raise Failure(f"the editor exited with code {self.editor.returncode}")
            if (self.folder / "ready.json").exists():
                return
            time.sleep(0.5)
        raise Failure(f"the driver plugin didn't find the assistant panel within {timeout} s")

    def send(self, command, timeout=120):
        temporary = self.folder / f"command_{self.next}.tmp"
        temporary.write_text(json.dumps(command), encoding="utf-8")
        temporary.rename(self.folder / f"command_{self.next}.json")
        done = self.folder / f"done_{self.next}.json"
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.editor.poll() is not None:
                raise Failure(f"the editor exited with code {self.editor.returncode}")
            if done.exists():
                self.next += 1
                return json.loads(done.read_text(encoding="utf-8"))
            time.sleep(0.2)
        raise Failure(f"the panel didn't finish {command} within {timeout} s")


def wait_for_mcp(mcp, editor, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if editor.poll() is not None:
            raise Failure(f"the editor exited with code {editor.returncode} before its MCP server answered")
        try:
            status, _, payload = mcp.http("GET", "/status", timeout=5)
            if status == 200 and json.loads(payload).get("ready"):
                return
        except OSError:
            pass
        time.sleep(1)
    raise Failure(f"the MCP server didn't answer within {timeout} s")


def new_text(before, after):
    """What the conversation shows since `before`."""
    return after[len(before) :] if after.startswith(before) else after


def test_claude(fake, driver, mcp, project):
    print("Claude (Anthropic API)")
    path = "/anthropic/v1/messages"
    fake.prepare(
        path,
        (200, claude_message(CLAUDE_CONTENT_1, "tool_use")),
        (
            200,
            claude_message(
                '[{"type":"tool_use","id":"toolu_02","name":"save_scene","input":{}},'
                '{"type":"tool_use","id":"toolu_03","name":"write_script","input":{"path":"res://enemy.gd","source":'
                + json.dumps(ENEMY_SCRIPT)
                + "}}]",
                "tool_use",
            ),
        ),
        (200, claude_message('[{"type":"text","text":"Added Enemy and its script."}]', "end_turn")),
    )
    shown = driver.send({"ask": "Add an enemy and give it a script."})["conversation"]
    check("Added Enemy and its script." in shown, "the answer shows in the panel")
    check("create_node" in shown and "write_script" in shown, "the panel lists the actions")

    requests = fake.taken(path)
    check(len(requests) == 3, "three requests: the question and two rounds of tool results")
    first = requests[0]
    check(first["headers"].get("x-api-key") == "test-key", "the API key goes in x-api-key")
    check(first["headers"].get("anthropic-version") == "2026-10-21", "anthropic-version 2026-10-21")
    body = json.loads(first["body"])
    check(body["model"] == "claude-opus-5-5", "model claude-opus-5-5")
    check(body["tool_choice"] == {"type": "auto"} and "thinking" not in body, "tool_choice auto, no thinking setting")
    check(
        len(body["tools"]) >= 20 and all("input_schema" in tool for tool in body["tools"]), "the MCP tools go as tools"
    )
    question = body["messages"][0]["content"][0]["text"]
    check("Scene being edited: res://main.tscn" in question, "the question carries the edited scene")
    check("Made with Fornalha: yes" in question and "Floresta de teste" in question, "and game.project.json")
    check(question.endswith("Add an enemy and give it a script."), "and ends with the question")
    check(CLAUDE_CONTENT_1 in requests[1]["body"], "Claude's answer goes back byte for byte")
    results = json.loads(requests[1]["body"])["messages"][2]
    check(
        results["content"][0]["type"] == "tool_result" and results["content"][0]["tool_use_id"] == "toolu_01",
        "the result of create_node",
    )
    third = json.loads(requests[2]["body"])["messages"][4]
    check(
        [block["tool_use_id"] for block in third["content"]] == ["toolu_02", "toolu_03"], "both results in one message"
    )

    check("Enemy" in mcp.node_names(), "Enemy is in the scene")
    check("Enemy" in (project / "main.tscn").read_text(encoding="utf-8"), "main.tscn was saved with Enemy")
    check((project / "enemy.gd").read_text(encoding="utf-8") == ENEMY_SCRIPT, "enemy.gd was written")

    fake.prepare(path, (200, claude_message('[{"type":"text","text":"The scene has Main."}]', "end_turn")))
    shown = driver.send({"ask": "What is in the scene now?"})["conversation"]
    check("The scene has Main." in shown, "a second question in the same conversation")
    body = json.loads(fake.taken(path)[3]["body"])
    check(len(body["messages"]) == 7, "the whole conversation goes again")
    check(
        "game.project.json:" not in body["messages"][6]["content"][0]["text"],
        "game.project.json only goes again when it changes",
    )

    # The last answer only read, so the one before is the one to undo.
    before = shown
    result = driver.send({"undo": True})
    check("Undid the last answer" in new_text(before, result["conversation"]), "the panel says the answer was undone")
    check("Enemy" not in mcp.node_names(), "Enemy left the scene")
    check("Enemy" not in (project / "main.tscn").read_text(encoding="utf-8"), "main.tscn was saved again without Enemy")
    check(not (project / "enemy.gd").exists(), "enemy.gd was deleted")
    check(result["undo_disabled"], "nothing more to undo")


def test_claude_undo(fake, driver, mcp, project):
    print("Undo last answer")
    path = "/anthropic/v1/messages"
    fake.prepare(
        path,
        (
            200,
            claude_message(
                '[{"type":"tool_use","id":"toolu_10","name":"create_node","input":{"type":"Node3D","name":"Tree"}},'
                '{"type":"tool_use","id":"toolu_11","name":"save_scene","input":{}},'
                '{"type":"tool_use","id":"toolu_12","name":"write_script","input":{"path":"res://tree.gd","source":"extends Node3D\\n"}}]',
                "tool_use",
            ),
        ),
        (200, claude_message('[{"type":"text","text":"Added a tree."}]', "end_turn")),
    )
    result = driver.send({"ask": "Add a tree."})
    check("Added a tree." in result["conversation"] and not result["undo_disabled"], "the answer can be undone")
    check("Tree" in mcp.node_names() and (project / "tree.gd").exists(), "Tree and tree.gd exist")
    before = result["conversation"]
    result = driver.send({"undo": True})
    check("Undid the last answer" in new_text(before, result["conversation"]), "the panel says it was undone")
    check("Tree" not in mcp.node_names(), "Tree left the scene")
    check("Tree" not in (project / "main.tscn").read_text(encoding="utf-8"), "main.tscn was saved again without Tree")
    check(not (project / "tree.gd").exists(), "tree.gd was deleted")
    check(result["undo_disabled"], "nothing more to undo")


def test_openai(fake, driver, mcp, project):
    print("OpenAI-compatible server")
    path = "/openai/v1/chat/completions"
    call = {
        "id": "call_1",
        "type": "function",
        "function": {
            "name": "edit_file",
            "arguments": json.dumps({"path": "res://player.gd", "old_text": "\tpass", "new_text": '\tprint("hello")'}),
        },
    }
    fake.prepare(
        path,
        (200, openai_message({"role": "assistant", "content": None, "tool_calls": [call]}, "tool_calls")),
        (200, openai_message({"role": "assistant", "content": "The player says hello now."}, "stop")),
    )
    driver.send({"settings": {"erethea/assistant/provider": 2}})
    time.sleep(3)
    shown = driver.send({"ask": "Make the player say hello."})["conversation"]
    check("The player says hello now." in shown, "the answer shows in the panel")
    requests = fake.taken(path)
    check(len(requests) == 2, "two requests")
    check(requests[0]["headers"].get("Authorization") == "Bearer openai-key", "the key goes as Bearer")
    body = json.loads(requests[0]["body"])
    check(body["model"] == "test-model" and body["tools"][0]["type"] == "function", "model and function tools")
    tool_message = json.loads(requests[1]["body"])["messages"][-1]
    check(
        tool_message["role"] == "tool" and tool_message["tool_call_id"] == "call_1", "the result goes as a tool message"
    )
    check('print("hello")' in (project / "player.gd").read_text(encoding="utf-8"), "player.gd was changed")
    before = shown
    result = driver.send({"undo": True})
    check("Undid the last answer" in new_text(before, result["conversation"]), "undone")
    check((project / "player.gd").read_text(encoding="utf-8") == PLAYER_SCRIPT, "player.gd has its old text back")


def test_ignis(fake, driver, mcp, project):
    print("Ignis")
    path = "/ignis/v1/chat/completions"
    actions = {
        "reply": "Adding a lamp.",
        "actions": [
            {"tool": "create_node", "arguments": {"type": "OmniLight3D", "name": "Lamp", "brightness": 3}},
            {"tool": "make_coffee", "arguments": {}},
        ],
    }
    fake.prepare(
        path,
        (
            200,
            openai_message({"role": "assistant", "content": "<think>A light.</think>\n" + json.dumps(actions)}, "stop"),
        ),
        (
            200,
            openai_message(
                {"role": "assistant", "content": json.dumps({"reply": "Done: a lamp.", "actions": []})}, "stop"
            ),
        ),
    )
    driver.send({"settings": {"erethea/assistant/provider": 0}})
    time.sleep(3)
    shown = driver.send({"ask": "Put a lamp here."})["conversation"]
    check("Done: a lamp." in shown, "the answer shows in the panel")
    check("unknown tool" in shown, "an action with an unknown tool is left out, and said")
    check("Lamp" in mcp.node_names(), "Lamp is in the scene")
    requests = fake.taken(path)
    check(len(requests) == 2, "two requests")
    body = json.loads(requests[0]["body"])
    check(body["model"] == "default" and body["response_format"]["type"] == "json_schema", "JSON schema answers")
    check(
        "tools" not in body and "- create_node(" in body["messages"][0]["content"],
        "the tools are listed in the system prompt",
    )
    check("Floresta de teste" in body["messages"][-1]["content"], "game.project.json goes with every question")
    check(
        "Results of your actions" in json.loads(requests[1]["body"])["messages"][-1]["content"], "the results go back"
    )

    # The person changes the scene after the answer: undoing would lose it.
    mcp.call("create_node", {"type": "Node3D", "name": "PersonNode"})
    before = driver.send({"new": True})["conversation"]
    result = driver.send({"undo": True})
    check("Not undone" in new_text(before, result["conversation"]), "an answer isn't undone over the person's changes")
    check("Lamp" in mcp.node_names() and "PersonNode" in mcp.node_names(), "both stay")

    fake.prepare(
        path,
        (503, json.dumps({"error": {"code": "sem_modelo", "message": "Nenhum modelo carregado"}})),
        (503, json.dumps({"error": {"code": "sem_modelo", "message": "Nenhum modelo carregado"}})),
    )
    shown = driver.send({"ask": "Again."})["conversation"]
    check("Prepare" in shown, "with no model, the panel says to prepare Ignis")
    check(len(fake.taken(path)) == 4, "one more try after an error worth retrying")


def read_log(path):
    if not path.exists():
        return []
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def test_claude_code(fake, driver, mcp, project, claude_log):
    print("Claude Code (subscription)")
    driver.send({
        "settings": {
            "erethea/assistant/provider": 3,
            "erethea/assistant/claude_code/path": sys.executable,
            "erethea/assistant/claude_code/arguments": str(HERE / "fake_claude.py"),
        }
    })
    time.sleep(3)
    shown = driver.send({"ask": "Put a statue here."})["conversation"]
    check("Placed a statue." in shown, "the answer shows in the panel")
    check("create_node" in shown and "save_scene" in shown, "the panel lists the tools Claude Code called")
    check("Statue" in mcp.node_names(), "Statue is in the scene")
    check("Statue" in (project / "main.tscn").read_text(encoding="utf-8"), "main.tscn was saved with Statue")

    entries = read_log(claude_log)
    starts = [entry for entry in entries if entry["event"] == "start"]
    check(len(starts) == 1, "Claude Code started once")
    argv = starts[0]["argv"]
    for flag, value in [
        ("--input-format", "stream-json"),
        ("--output-format", "stream-json"),
        ("--allowedTools", "mcp__erethea"),
        ("--permission-mode", "dontAsk"),
    ]:
        check(flag in argv and argv[argv.index(flag) + 1] == value, f"{flag} {value}")
    check(
        "-p" in argv and "--strict-mcp-config" in argv and "--tools=" in argv,
        "print mode, only the editor's MCP server and no built-in tools",
    )
    check("--resume" not in argv and "--bare" not in argv, "a new session, with the subscription login")
    url = starts[0]["mcp_config"]["mcpServers"]["erethea"]["url"]
    check(url == f"http://127.0.0.1:{mcp.port}/mcp", "the editor's MCP server in --mcp-config")
    check(not starts[0]["api_key_in_environment"], "no API key reaches Claude Code")
    messages = [entry for entry in entries if entry["event"] == "message"]
    check(len(messages) == 1 and messages[0]["type"] == "user", "one user message on its input")
    check("Scene being edited: res://main.tscn" in messages[0]["content"], "the message carries the editor's context")
    check(messages[0]["content"].endswith("Put a statue here."), "and ends with the question")

    shown = driver.send({"ask": "Anything else?"})["conversation"]
    check("Nothing to change." in shown, "a second question")
    entries = read_log(claude_log)
    check(len([entry for entry in entries if entry["event"] == "start"]) == 1, "on the same Claude Code")

    # Claude Code goes away between questions: the next one starts it again in the same session.
    os.kill(starts[0]["pid"], signal.SIGTERM)
    time.sleep(2)
    shown = driver.send({"ask": "And now?"})["conversation"]
    check(shown.count("Nothing to change.") == 2, "it answers after starting again")
    starts = [entry for entry in read_log(claude_log) if entry["event"] == "start"]
    check(
        len(starts) == 2 and starts[1]["argv"][starts[1]["argv"].index("--resume") + 1] == "fake-session-1",
        "with --resume and the same session",
    )

    # The last answers only talked, so the statue is what gets undone.
    before = driver.send({"new": True})["conversation"]
    result = driver.send({"undo": True})
    check("Undid the last answer" in new_text(before, result["conversation"]), "the panel says the answer was undone")
    check("Statue" not in mcp.node_names(), "Statue left the scene")
    check(
        "Statue" not in (project / "main.tscn").read_text(encoding="utf-8"), "main.tscn was saved again without Statue"
    )


def run_checks(fake, driver, mcp, project, claude_log):
    reply = mcp.rpc(
        "initialize",
        {
            "protocolVersion": "2025-06-18",
            "capabilities": {},
            "clientInfo": {"name": "test_assistant.py", "version": "1"},
        },
    )
    check(reply["result"]["serverInfo"]["name"] == "erethea", "the MCP server answers")
    mcp.call("open_scene", {"path": "res://main.tscn"})
    driver.send({
        "settings": {
            "erethea/assistant/provider": 1,
            "erethea/assistant/claude/api_key": "test-key",
            "erethea/assistant/claude/url": f"http://127.0.0.1:{fake.port}/anthropic/v1/messages",
            "erethea/assistant/openai/url": f"http://127.0.0.1:{fake.port}/openai/v1",
            "erethea/assistant/openai/api_key": "openai-key",
            "erethea/assistant/openai/model": "test-model",
            "erethea/assistant/ignis/url": f"http://127.0.0.1:{fake.port}/ignis/v1",
        }
    })
    # The settings reach the panel a moment later.
    time.sleep(3)
    test_claude(fake, driver, mcp, project)
    test_claude_undo(fake, driver, mcp, project)
    test_openai(fake, driver, mcp, project)
    test_ignis(fake, driver, mcp, project)
    test_claude_code(fake, driver, mcp, project, claude_log)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("editor", help="the Erethea editor's executable")
    parser.add_argument("--out", default="assistant_test_output", help="folder for the editor's log")
    parser.add_argument("--port", type=int, default=11493, help="port of the editor's MCP server")
    parser.add_argument("--timeout", type=int, default=300, help="seconds to wait for the editor to start")
    arguments = parser.parse_args()

    out = Path(arguments.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="erethea_assistant_"))
    project = work / "project"
    shutil.copytree(HERE / "project", project)
    fake = FakeModels()

    environment = dict(os.environ, ERETHEA_MCP_PORT=str(arguments.port))
    # Set, to check that it doesn't reach Claude Code, which uses the subscription.
    environment["ANTHROPIC_API_KEY"] = "must-not-reach-claude-code"
    claude_log = work / "fake_claude.log"
    environment["FAKE_CLAUDE_LOG"] = str(claude_log)
    command = [
        str(Path(arguments.editor).resolve()),
        "--editor",
        "--headless",
        "--path",
        str(project),
        "--audio-driver",
        "Dummy",
    ]
    log_path = out / "editor.log"
    print("Running:", " ".join(command))

    failure = None
    with open(log_path, "w", encoding="utf-8", errors="replace") as log:
        editor = subprocess.Popen(
            command, env=environment, stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL
        )
        try:
            mcp = Mcp(arguments.port)
            wait_for_mcp(mcp, editor, arguments.timeout)
            driver = Driver(project, editor)
            driver.wait_ready(60)
            run_checks(fake, driver, mcp, project, claude_log)
        except (Failure, OSError, KeyError, TypeError, ValueError, IndexError) as error:
            failure = error
        finally:
            editor.terminate()
            try:
                editor.wait(15)
            except subprocess.TimeoutExpired:
                editor.kill()
                editor.wait()
            fake.server.shutdown()

    log_text = log_path.read_text(encoding="utf-8", errors="replace")
    shutil.rmtree(work, ignore_errors=True)
    if failure:
        print("\nLast lines of the editor's log:\n" + "\n".join(log_text.splitlines()[-80:]))
        print(f"\nFAILED: {failure}")
        return 1
    print("\nERETHEA_ASSISTANT_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
