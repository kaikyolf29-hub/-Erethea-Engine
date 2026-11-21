#!/usr/bin/env python3
"""Download Intel's XeSS runtime (`libxess.dll`) into a folder.

The `erethea_upscale` module loads this DLL from the folder of the executable
to upscale with XeSS Super Resolution. The engine builds don't contain it: the
installer, the release and the tests put it next to `erethea.exe` with this
script.

    python erethea/tools/fetch_xess.py <folder>

The file comes from a fixed commit of Intel's XeSS SDK and is checked against
its SHA-256, so a changed download fails instead of shipping. Intel's license
for it (Intel Simplified Software License: redistribution of the unmodified
binary, with the notice) is written next to it as `XESS_LICENSE.txt`.
"""

import hashlib
import sys
import urllib.request
from pathlib import Path

SDK_VERSION = "XeSS 3 SDK (XeSS-SR 2.0.2)"
SDK_COMMIT = "de0fb9c1c510661c571164e1418ceca8101dab69"
BASE_URL = f"https://raw.githubusercontent.com/intel/xess/{SDK_COMMIT}/"

FILES = {
    "libxess.dll": "251659dd84a3e84de67c886a4186e01f3eca49b00641906fe38bb6b807e5d5b7",
}
LICENSE_PATH = "LICENSE.txt"
LICENSE_SHA256 = "784be42c39a4a4d03cf85d88e15e59d75828fca773ce45b8dbfffbd7eaf208df"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda: file.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def download(url: str, target: Path, expected: str) -> None:
    partial = target.with_name(target.name + ".part")
    with urllib.request.urlopen(url, timeout=120) as response, partial.open("wb") as file:
        while block := response.read(1 << 20):
            file.write(block)
    actual = sha256(partial)
    if actual != expected:
        partial.unlink()
        raise SystemExit(f"{target.name}: SHA-256 is {actual}, expected {expected}")
    partial.replace(target)


def fetch(folder: Path) -> None:
    folder.mkdir(parents=True, exist_ok=True)
    for name, expected in FILES.items():
        target = folder / name
        if target.exists() and sha256(target) == expected:
            print(f"{name}: already there")
            continue
        print(f"{name}: downloading")
        download(BASE_URL + "bin/" + name, target, expected)

    license_file = folder / "XESS_LICENSE.txt"
    if not (license_file.exists() and sha256(license_file) == LICENSE_SHA256):
        download(BASE_URL + LICENSE_PATH, license_file, LICENSE_SHA256)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    fetch(Path(sys.argv[1]))
