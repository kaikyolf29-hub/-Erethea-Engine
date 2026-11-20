#!/usr/bin/env python3
"""Apply the Erethea Engine branding on top of the Godot source tree.

Run it from any folder after every merge from the `upstream` branch:

    python erethea/tools/rebrand.py          # apply the branding
    python erethea/tools/rebrand.py --check  # only report, exit 1 if something is missing

What it changes (and only this, so merging new Godot versions stays easy):

- the engine name, short name and website in `version.py` (window titles,
  "About" dialog, user data folder `%APPDATA%\\Erethea`, executable metadata);
- the executable name: `bin/erethea.windows.editor.x86_64.exe` and so on;
- the icon, splash screen and editor logos, copied from `erethea/branding/`;
- a short list of editor texts that talk about the program itself.

What it never changes, to keep Godot projects opening as before:
`project.godot`, the `res://` and `user://` schemes, GDScript, the C# `Godot`
namespace, the GDExtension interface, class names and file formats.

When Godot changes one of the lines below, the rule stops matching and this
script reports it. Fix the rule here, never by hand-editing the target file.
"""

from __future__ import annotations

import argparse
import filecmp
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BRANDING = Path("erethea/branding")

NAME = "Erethea Engine"
SHORT_NAME = "erethea"
WEBSITE = "https://github.com/kaikyolf29-hub/-Erethea-Engine"


@dataclass(frozen=True)
class Rule:
    """Replace `old` with `new` in `path`, every occurrence."""

    path: str
    old: str
    new: str


RULES: list[Rule] = [
    # Engine identity.
    Rule("version.py", 'short_name = "godot"', f'short_name = "{SHORT_NAME}"'),
    Rule("version.py", 'name = "Godot Engine"', f'name = "{NAME}"'),
    Rule("version.py", 'website = "https://godotengine.org"', f'website = "{WEBSITE}"'),
    # Custom module discovery must still skip nested engine checkouts.
    Rule(
        "methods.py",
        """if 'short_name = "godot"' in f.read():""",
        """if 'short_name = "godot"' in (content := f.read()) or 'short_name = "erethea"' in content:""",
    ),
    # Executable name.
    Rule("platform/windows/SCsub", '"#bin/godot"', '"#bin/erethea"'),
    Rule("platform/linuxbsd/SCsub", '"#bin/godot"', '"#bin/erethea"'),
    Rule("platform/macos/SCsub", '"#bin/godot"', '"#bin/erethea"'),
    Rule("methods.py", 'env.File(f"#bin/godot{', 'env.File(f"#bin/erethea{'),
    Rule(".github/workflows/windows_builds.yml", "bin/godot.", "bin/erethea."),
    Rule(".github/workflows/linux_builds.yml", "bin/godot.", "bin/erethea."),
    Rule(".github/workflows/macos_builds.yml", "bin/godot.", "bin/erethea."),
    # Editor texts about the program itself. Texts about Godot versions,
    # Godot projects or the Godot community stay as they are.
    Rule("editor/docks/editor_dock_manager.cpp", 'TTR("%s - Godot Engine")', 'TTR("%s - Erethea Engine")'),
    Rule("editor/run/game_view_plugin.cpp", 'TTR("%s - Godot Engine")', 'TTR("%s - Erethea Engine")'),
    Rule("editor/project_manager/project_manager.cpp", 'TTR("About Godot")', 'TTR("About Erethea")'),
    Rule("editor/editor_node.cpp", 'TTRC("About Godot...")', 'TTRC("About Erethea...")'),
    Rule(
        "editor/gui/editor_version_button.cpp",
        'TTR("Copied Godot editor version.")',
        'TTR("Copied Erethea editor version.")',
    ),
    Rule(
        "editor/editor_node.cpp",
        'TTR("Files have been modified outside Godot")',
        'TTR("Files have been modified outside Erethea")',
    ),
    Rule(
        "editor/script/script_editor_plugin.cpp",
        'TTRC("Files have been modified outside Godot")',
        'TTRC("Files have been modified outside Erethea")',
    ),
    Rule(
        "editor/project_manager/project_manager.cpp",
        'TTR("It looks like Godot crashed when opening this project the last time.',
        'TTR("It looks like Erethea crashed when opening this project the last time.',
    ),
    Rule(
        "editor/run/editor_run_bar.cpp",
        'TTR("Godot opened the project in Recovery Mode,',
        'TTR("Erethea opened the project in Recovery Mode,',
    ),
    Rule(
        "editor/gui/credits_roll.cpp",
        'TTRC("Thank you for choosing Godot Engine!")',
        'TTRC("Thank you for choosing Erethea Engine!")',
    ),
]

# Branding files copied over Godot's own (same file names, so the build
# scripts do not change).
ASSETS: list[tuple[str, str]] = [
    ("erethea.ico", "platform/windows/godot.ico"),
    ("erethea_console.ico", "platform/windows/godot_console.ico"),
    ("app_icon.png", "main/app_icon.png"),
    ("splash.png", "main/splash.png"),
    ("logo.svg", "editor/icons/Logo.svg"),
    ("title_bar_logo.svg", "editor/icons/TitleBarLogo.svg"),
    ("editor_icon_16.svg", "editor/icons/Godot.svg"),
    ("icon.svg", "editor/icons/DefaultProjectIcon.svg"),
]


def apply_rules(check: bool) -> tuple[list[str], list[str]]:
    """Return (changed, broken) lists of human-readable messages."""
    changed: list[str] = []
    broken: list[str] = []
    by_path: dict[str, list[Rule]] = {}
    for rule in RULES:
        by_path.setdefault(rule.path, []).append(rule)

    for rel, rules in by_path.items():
        path = ROOT / rel
        if not path.is_file():
            broken.append(f"{rel}: file not found")
            continue
        text = path.read_text(encoding="utf-8")
        original = text
        for rule in rules:
            if rule.old in text:
                text = text.replace(rule.old, rule.new)
                changed.append(f"{rel}: {rule.old!r} -> {rule.new!r}")
            elif rule.new not in text:
                broken.append(f"{rel}: neither the Godot text nor the Erethea text was found: {rule.old!r}")
        if text != original and not check:
            path.write_text(text, encoding="utf-8", newline="")
    return changed, broken


def apply_assets(check: bool) -> tuple[list[str], list[str]]:
    changed: list[str] = []
    broken: list[str] = []
    for src_name, dst_rel in ASSETS:
        src = ROOT / BRANDING / src_name
        dst = ROOT / dst_rel
        if not src.is_file():
            broken.append(f"{BRANDING / src_name}: branding file missing")
            continue
        if not dst.parent.is_dir():
            broken.append(f"{dst_rel}: target folder missing")
            continue
        if dst.is_file() and filecmp.cmp(src, dst, shallow=False):
            continue
        changed.append(f"{dst_rel}: replaced with {BRANDING / src_name}")
        if not check:
            shutil.copyfile(src, dst)
    return changed, broken


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="do not write anything; exit 1 if branding is missing")
    args = parser.parse_args()

    changed, broken = apply_rules(args.check)
    asset_changed, asset_broken = apply_assets(args.check)
    changed += asset_changed
    broken += asset_broken

    verb = "would change" if args.check else "changed"
    for line in changed:
        print(f"{verb}: {line}")
    for line in broken:
        print(f"ERROR: {line}", file=sys.stderr)

    if broken:
        print(f"\n{len(broken)} rule(s) no longer match. Update erethea/tools/rebrand.py.", file=sys.stderr)
        return 1
    if args.check and changed:
        print(f"\nBranding is incomplete ({len(changed)} change(s) pending). Run erethea/tools/rebrand.py.")
        return 1
    if not changed:
        print("Branding is up to date.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
