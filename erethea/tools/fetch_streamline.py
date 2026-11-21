#!/usr/bin/env python3
"""Download NVIDIA's Streamline and DLSS files into a folder.

The `erethea_upscale` module loads these DLLs from the folder of the
executable to upscale with DLSS Super Resolution. Erethea doesn't ship them:
NVIDIA's license (NVIDIA RTX SDKs License) only lets a developer distribute
them inside their own application. So each developer who wants DLSS downloads
them from NVIDIA with this script, after reading and accepting that license:

    python erethea/tools/fetch_streamline.py <folder> --accept-nvidia-license

Put them next to the editor's executable: running the project from the editor
then uses DLSS, and Windows exports copy them into the game. A game that ships
them has to follow NVIDIA's license, which the script prints: among others,
NVIDIA's attribution and marks on the splash screen and in the credits, and a
notification to NVIDIA before a commercial release.

The files come from a fixed release of NVIDIA's Streamline SDK and are checked
against their SHA-256, so a changed download fails instead of being used.
NVIDIA's license is written next to them as `nvngx_dlss.license.txt`.
"""

import hashlib
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

SDK_VERSION = "2.14.16"
SDK_URL = f"https://github.com/NVIDIA-RTX/Streamline/releases/download/v{SDK_VERSION}/streamline-sdk-v{SDK_VERSION}.zip"
SDK_SHA256 = "32041e51db0cab709a271b72a352281b690c2abbd669a827809401a5dd23956b"

# The production (signed) files: Streamline's loader, its common plugin, the
# DLSS plugin and DLSS itself.
FILES = {
    "sl.interposer.dll": "b1d7395aebd7b9722841d64dcf7433680020b6a8e421f851c16d5ba0c2182edf",
    "sl.common.dll": "38eea3b4209afc455636ea372b5933d65fcc13becec6c84d101dcfc4d8648f75",
    "sl.dlss.dll": "18871eea87c90345da97d8109faa81ea7d5840e8d99b51b55b0e46cfa177f84e",
    "nvngx_dlss.dll": "d00b5b0ab29c3ca101b70c3a5ecb3a01b7ef33766a3383a28a18623083e6123e",
    "nvngx_dlss.license.txt": "3027f23ca5a46dd9cb8183fbd522983a86f64d7daac5982912bf9f214671f294",
}
SDK_FOLDER = "bin/x64/"

LICENSE_NOTICE = f"""\
DLSS and Streamline are NVIDIA's, under the NVIDIA RTX SDKs License, not under
Erethea's MIT license. Read it before accepting: it comes in NVIDIA's download
({SDK_URL}) as bin/x64/nvngx_dlss.license.txt, and this script writes it next
to the files. Among others, it asks a game that ships these files to:

  - ship them only inside the game, never on their own;
  - attribute DLSS and show NVIDIA's marks on the splash screen, in the about
    box and in the credits;
  - notify NVIDIA before a commercial release
    (https://developer.nvidia.com/sw-notification).

To accept it and download, run again with --accept-nvidia-license.
"""


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_sha256(path: Path) -> str:
    return sha256(path.read_bytes()) if path.exists() else ""


def download_sdk() -> zipfile.ZipFile:
    digest = hashlib.sha256()
    # About 280 MB, so it goes to a temporary file instead of memory.
    archive = tempfile.TemporaryFile()
    with urllib.request.urlopen(SDK_URL, timeout=300) as response:
        while block := response.read(1 << 20):
            digest.update(block)
            archive.write(block)
    actual = digest.hexdigest()
    if actual != SDK_SHA256:
        raise SystemExit(f"Streamline SDK: SHA-256 is {actual}, expected {SDK_SHA256}")
    archive.seek(0)
    return zipfile.ZipFile(archive)


def fetch(folder: Path) -> None:
    missing = [name for name, expected in FILES.items() if file_sha256(folder / name) != expected]
    if not missing:
        print(f"NVIDIA Streamline {SDK_VERSION}: already there")
        return

    print(f"NVIDIA Streamline {SDK_VERSION}: downloading")
    sdk = download_sdk()
    folder.mkdir(parents=True, exist_ok=True)
    for name in missing:
        data = sdk.read(SDK_FOLDER + name)
        actual = sha256(data)
        if actual != FILES[name]:
            raise SystemExit(f"{name}: SHA-256 is {actual}, expected {FILES[name]}")
        partial = folder / (name + ".part")
        partial.write_bytes(data)
        partial.replace(folder / name)
        print(f"{name}: written")


if __name__ == "__main__":
    arguments = sys.argv[1:]
    accepted = "--accept-nvidia-license" in arguments
    folders = [argument for argument in arguments if not argument.startswith("--")]
    if len(folders) != 1 or len(folders) + int(accepted) != len(arguments):
        raise SystemExit(__doc__)
    if not accepted:
        raise SystemExit(LICENSE_NOTICE)
    fetch(Path(folders[0]))
