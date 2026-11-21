#!/usr/bin/env python3
"""Download AMD's FSR runtime for Vulkan and Direct3D 12 into a folder.

The `erethea_upscale` module loads these DLLs from the folder of the
executable to upscale with AMD FSR (FSR 4 where the graphics card supports it,
FSR 3.1 elsewhere), a loader and an upscaler for each rendering driver. The engine builds don't contain them: the installer, the
release and the tests put them next to `erethea.exe` with this script.

    python erethea/tools/fetch_amd_fsr.py <folder>

The files come from a fixed commit of AMD's FSR SDK (MIT license) and are
checked against their SHA-256, so a changed download fails instead of
shipping. `AMD_FSR_LICENSE.txt` is written next to them.
"""

import hashlib
import sys
import urllib.request
from pathlib import Path

SDK_VERSION = "2.4.0"
SDK_COMMIT = "7539ae8b6568ad29ed78a753404351daf3b5cca7"
BASE_URL = (
    f"https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/{SDK_COMMIT}/Kits/FidelityFX/signedbin/"
)

FILES = {
    "amd_fidelityfx_loader_vk.dll": "38a56a01fb3565c0757d9f5c1e4e69de4b65e5f2e5769b323b200dc1f142bab9",
    "amd_fidelityfx_upscaler_vk.dll": "4a7e895af6e0b615d40a84d59eb8c2b1054ad91cdd0c739567940270d51bdab7",
    "amd_fidelityfx_loader_dx12.dll": "e2191d3c0828be45eb8affd4e3ec0378f2293adc345b772a7170edcf3b021ecb",
    "amd_fidelityfx_upscaler_dx12.dll": "aad4fccccb29084a655f9a4f9f613bd22c8c03fb4a0f8840148cc6543b4ccace",
}

LICENSE = f"""AMD FSR SDK {SDK_VERSION} runtime ({", ".join(FILES)})
https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK

Copyright (C) Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda: file.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def fetch(folder: Path) -> None:
    folder.mkdir(parents=True, exist_ok=True)
    for name, expected in FILES.items():
        target = folder / name
        if target.exists() and sha256(target) == expected:
            print(f"{name}: already there")
            continue
        partial = target.with_name(name + ".part")
        print(f"{name}: downloading")
        with urllib.request.urlopen(BASE_URL + name, timeout=120) as response, partial.open("wb") as file:
            while block := response.read(1 << 20):
                file.write(block)
        actual = sha256(partial)
        if actual != expected:
            partial.unlink()
            raise SystemExit(f"{name}: SHA-256 is {actual}, expected {expected}")
        partial.replace(target)
    (folder / "AMD_FSR_LICENSE.txt").write_text(LICENSE, encoding="utf-8")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    fetch(Path(sys.argv[1]))
