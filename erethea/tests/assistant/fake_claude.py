#!/usr/bin/env python3
"""A stand-in for Claude Code, for test_assistant.py.

The assistant panel starts it the way it starts Claude Code: with JSON lines on
its input and output (--input-format/--output-format stream-json) and the
editor's MCP server in the --mcp-config file. Like Claude Code, it acts on the
editor only through that server. Each start and each message is written to the
file in FAKE_CLAUDE_LOG, so the test can check the command line, that no API key
reached it, and what the panel sent.

What it does depends on the question (after the editor's context):
- "statue": creates the node Statue through the MCP server and saves the scene;
- anything else: answers that there's nothing to change.
"""

import json
import os
import sys
import urllib.request


def log(entry):
    path = os.environ.get("FAKE_CLAUDE_LOG")
    if path:
        with open(path, "a", encoding="utf-8") as file:
            file.write(json.dumps(entry) + "\n")


def say(message):
    sys.stdout.write(json.dumps(message) + "\n")
    sys.stdout.flush()


def option(arguments, name):
    return arguments[arguments.index(name) + 1] if name in arguments else None


class Mcp:
    def __init__(self, url):
        self.url = url
        self.session = None
        self.last_id = 0

    def rpc(self, method, params):
        self.last_id += 1
        body = json.dumps({"jsonrpc": "2.0", "id": self.last_id, "method": method, "params": params}).encode()
        headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
        if self.session:
            headers["Mcp-Session-Id"] = self.session
        request = urllib.request.Request(self.url, data=body, headers=headers, method="POST")
        with urllib.request.urlopen(request, timeout=60) as response:
            if method == "initialize":
                self.session = response.headers.get("Mcp-Session-Id")
            return json.loads(response.read())

    def call(self, tool, arguments):
        return self.rpc("tools/call", {"name": tool, "arguments": arguments})["result"]


def act(mcp, session_id, calls):
    """Calls the tools as Claude Code would, and says so on the output."""
    for number, (tool, arguments) in enumerate(calls, 1):
        call_id = f"toolu_{number}"
        say({
            "type": "assistant",
            "session_id": session_id,
            "parent_tool_use_id": None,
            "message": {
                "role": "assistant",
                "content": [{"type": "tool_use", "id": call_id, "name": f"mcp__erethea__{tool}", "input": arguments}],
            },
        })
        result = mcp.call(tool, arguments)
        say({
            "type": "user",
            "session_id": session_id,
            "parent_tool_use_id": None,
            "message": {
                "role": "user",
                "content": [
                    {
                        "type": "tool_result",
                        "tool_use_id": call_id,
                        "content": result["content"],
                        "is_error": result["isError"],
                    }
                ],
            },
        })


def main():
    arguments = sys.argv[1:]
    config_path = option(arguments, "--mcp-config")
    config = json.load(open(config_path, encoding="utf-8")) if config_path else {}
    session_id = option(arguments, "--resume") or "fake-session-1"
    log({
        "event": "start",
        "pid": os.getpid(),
        "argv": arguments,
        "mcp_config": config,
        "api_key_in_environment": "ANTHROPIC_API_KEY" in os.environ,
    })
    url = config.get("mcpServers", {}).get("erethea", {}).get("url")
    mcp = Mcp(url) if url else None
    if mcp:
        mcp.rpc(
            "initialize",
            {
                "protocolVersion": "2025-06-18",
                "capabilities": {},
                "clientInfo": {"name": "fake_claude", "version": "1"},
            },
        )
    say({
        "type": "system",
        "subtype": "init",
        "session_id": session_id,
        "tools": ["mcp__erethea__create_node", "mcp__erethea__save_scene"],
        "mcp_servers": [{"name": "erethea", "status": "connected" if mcp else "failed"}],
    })

    for line in sys.stdin:
        if not line.strip():
            continue
        message = json.loads(line)
        content = message.get("message", {}).get("content", "")
        log({"event": "message", "type": message.get("type"), "content": content})
        # The question, after the editor's context.
        question = content.rsplit("[End of what the editor shows]", 1)[-1]
        if "statue" in question.lower():
            act(
                mcp,
                session_id,
                [("create_node", {"type": "Node3D", "name": "Statue"}), ("save_scene", {})],
            )
            answer = "Placed a statue."
        else:
            answer = "Nothing to change."
        say({
            "type": "assistant",
            "session_id": session_id,
            "parent_tool_use_id": None,
            "message": {"role": "assistant", "content": [{"type": "text", "text": answer}]},
        })
        say({
            "type": "result",
            "subtype": "success",
            "is_error": False,
            "result": answer,
            "session_id": session_id,
            "num_turns": 1,
        })


if __name__ == "__main__":
    main()
