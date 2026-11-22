#!/usr/bin/env python3
"""End-to-end test of the test mode for agents (modules/erethea_agent_test).

Copies the project next to this script (a player that walks with
"move_forward" and jumps with "jump") and runs the game with --agent-test,
as the Fornalha or an agent would:

- with a file of inputs: the player walks and jumps, the test passes and the
  game exits with 0;
- with an error printed by the game: the report has it, with the line of the
  script, and the game exits with 1;
- without a file: the project's own actions are pressed in turn;
- with a broken file: the report says why, and the game exits with 2;
- a game that quits by itself still leaves its report;
- with --window, in a window: screenshots and rendering numbers.

    python erethea/tests/agent_test/test_agent_test.py <editor executable> [--out <folder>] [--window]
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
PREFIX = "ERETHEA_AGENT_TEST "

INPUTS = {
    "screenshots": [1.0, 3.5],
    "steps": [
        {"at": 0.5, "for": 2.0, "action": "move_forward"},
        {"at": 3.0, "action": "jump"},
        {"at": 3.5, "for": 0.5, "key": "W"},
    ],
}


class Failure(Exception):
    pass


def check(condition, message):
    if not condition:
        raise Failure(message)
    print(f"  ok: {message}")


class Game:
    def __init__(self, editor, project, out, timeout, display_args):
        self.editor = editor
        self.project = project
        self.out = out
        self.timeout = timeout
        self.display_args = display_args

    def run(self, name, arguments, user_arguments=()):
        """Runs the game in test mode; returns the exit code, the report and the line printed at the end."""
        folder = self.out / name
        shutil.rmtree(folder, ignore_errors=True)
        command = [self.editor, "--path", str(self.project), *self.display_args, f"--agent-test={folder}", *arguments]
        if user_arguments:
            command += ["--", *user_arguments]
        print(f"{name}: " + " ".join(command))
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=self.timeout,
            stdin=subprocess.DEVNULL,
        )
        output = result.stdout + result.stderr
        (self.out / f"{name}.log").write_text(output, encoding="utf-8")
        report_file = folder / "report.json"
        check(report_file.is_file(), f"the report is written (exit code {result.returncode})")
        report = json.loads(report_file.read_text(encoding="utf-8"))
        line = next((line for line in output.splitlines() if line.startswith(PREFIX)), None)
        printed = json.loads(line[len(PREFIX) :]) if line else None
        summary = {key: report.get(key) for key in ("ended", "passed", "seconds", "error_count", "fps", "player")}
        print(f"    {json.dumps(summary)}")
        return result.returncode, report, printed


def step_results(report):
    return [(step.get("action") or step.get("key"), step["result"]) for step in report["inputs"]]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("editor", help="Erethea editor executable")
    parser.add_argument("--out", help="folder for the reports and logs (default: a temporary folder)")
    parser.add_argument("--window", action="store_true", help="also run the game in a window")
    parser.add_argument("--timeout", type=int, default=180)
    parser.add_argument("--keep", action="store_true", help="keep the copy of the project")
    args = parser.parse_args()

    editor = str(Path(args.editor).resolve())
    work = Path(tempfile.mkdtemp(prefix="erethea_agent_test_"))
    out = Path(args.out).resolve() if args.out else work / "out"
    out.mkdir(parents=True, exist_ok=True)
    project = work / "project"
    shutil.copytree(HERE / "project", project)
    inputs_file = work / "inputs.json"
    inputs_file.write_text(json.dumps(INPUTS), encoding="utf-8")

    try:
        print("Importing")
        subprocess.run(
            [editor, "--headless", "--import", "--path", str(project)],
            capture_output=True,
            timeout=args.timeout,
            stdin=subprocess.DEVNULL,
        )
        game = Game(editor, project, out, args.timeout, ["--headless"])

        print("With a file of inputs")
        code, report, printed = game.run("inputs", ["--agent-test-seconds=4.5", f"--agent-test-inputs={inputs_file}"])
        check(code == 0, f"the game exits with 0 (got {code})")
        check(report["finished"] and report["ended"] == "time", "the test ran to the end")
        check(report["passed"] and report["error_count"] == 0, "and passed, without errors")
        check(printed is not None and printed["passed"], "the report is printed too, on one line")
        check(report["seconds"] >= 4.5, f"it lasted the seconds asked ({report['seconds']} s)")
        check(report["fps"]["frames"] > 10 and report["fps"]["average_fps"] > 0, "the frame rate is measured")
        check(report["headless"] and report["rendering"] is None, "a game without a window has no rendering numbers")
        check(report["scene"] == "res://main.tscn", "the main scene ran")
        check(
            step_results(report) == [("move_forward", "done"), ("jump", "done"), ("W", "done")],
            "every input was pressed and released",
        )
        player = report["player"]
        check(player is not None and player["group"] == "player", "the player is found by its group")
        # 2.5 s at 3 m/s, with the action and then the key.
        check(5.0 < player["moved"] < 9.0, f"the player walked ({player['moved']} m)")
        check(not player["fell"] and not player["freed"], "and didn't fall off the world")
        check(report["screenshots"] == [], "no screenshots without a window")

        print("With an error")
        code, report, _ = game.run(
            "error", ["--agent-test-seconds=4.5", f"--agent-test-inputs={inputs_file}"], ["--error"]
        )
        check(code == 1, f"the game exits with 1 (got {code})")
        check(report["finished"] and not report["passed"], "the test ran to the end and failed")
        check(report["error_count"] == 1, "with one error")
        error = report["errors"][0]
        check(error["message"] == "The player jumped into an error.", "the game's own message")
        check(
            error.get("script_file") == "res://player.gd" and error.get("script_line", 0) > 0,
            f"and where the script printed it ({error.get('script_file')}:{error.get('script_line')})",
        )
        check(2.0 < error["first_seconds"], "and when")

        print("Without a file of inputs")
        code, report, _ = game.run("default", ["--agent-test-seconds=4"])
        check(code == 0 and report["passed"], f"the test passes (exit code {code})")
        actions = {step["action"] for step in report["inputs"] if step["result"] == "done"}
        check(actions == {"jump", "move_forward"}, f"the project's actions are pressed ({sorted(actions)})")
        check(report["player"]["moved"] > 1.0, f"so the player walks ({report['player']['moved']} m)")

        print("With a broken file of inputs")
        broken = work / "broken.json"
        broken.write_text(json.dumps([{"at": 1, "action": "jump"}, {"for": 2}]), encoding="utf-8")
        code, report, _ = game.run("broken", [f"--agent-test-inputs={broken}"])
        check(code == 2, f"the game exits with 2 (got {code})")
        check("Step 2" in report.get("error", ""), f"the report says which step is wrong ({report.get('error')})")
        check(not report["passed"], "and the test didn't pass")

        print("A game that quits by itself")
        code, report, _ = game.run("quit", ["--agent-test-seconds=10"], ["--quit-at=1"])
        check(report["finished"] and report["ended"] == "game_quit", "the report says the game quit")
        check(report["seconds"] < 5.0, f"before the end ({report['seconds']} s)")

        if args.window:
            print("In a window")
            display = ["--rendering-method", "gl_compatibility", "--audio-driver", "Dummy"]
            if sys.platform == "win32":
                display += ["--rendering-driver", "opengl3_angle"]
            game.display_args = display
            code, report, _ = game.run("window", ["--agent-test-seconds=4", f"--agent-test-inputs={inputs_file}"])
            check(code == 0 and report["passed"], f"the test passes (exit code {code})")
            check(not report["headless"], "in a window")
            check(len(report["screenshots"]) == 2, "with the two screenshots asked")
            check(
                all(Path(file).is_file() and Path(file).stat().st_size > 1000 for file in report["screenshots"]),
                "saved as images",
            )
            check(report["rendering"] is not None and report["rendering"]["draw_calls_max"] > 0, "and drawing numbers")
            check(report["player"]["moved"] > 3.0, f"the player walked ({report['player']['moved']} m)")
    except Failure as failure:
        print(f"\nFAILED: {failure}\nLogs and reports in {out}")
        return 1
    finally:
        if args.keep:
            print(f"The project is in {project}")
        elif not args.out:
            print(f"Logs and reports in {out}")
        if not args.keep:
            shutil.rmtree(project, ignore_errors=True)

    print("\nERETHEA_AGENT_TEST_OK")
    return 0


if __name__ == "__main__":
    os.environ.setdefault("PYTHONUTF8", "1")
    sys.exit(main())
