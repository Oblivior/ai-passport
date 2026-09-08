#!/usr/bin/env python3
"""Build a whitelist-only macOS pilot ZIP from a verified merged artifact."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import tempfile
import zipfile

from pet_install import image_bytes, load_bundle

ROOT = Path(__file__).resolve().parents[1]
FILES = (
    "tools/pet_bootstrap.py",
    "tools/pet_install.py", "tools/pet_companion.py", "tools/pet_wireless.py",
    "tools/verify_firmware.py", "tools/requirements-pet-installer.txt",
    "LICENSE", "docs/assets/pet-mac-install.md", "docs/assets/pet-mac-install.zh_CN.md",
    "assets/images/README.md", "assets/images/README.zh_CN.md",
    "assets/fonts/LICENSE.txt",
)


def package(merged_path, version, output):
    if not re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9._-]{0,63}", version):
        raise ValueError("invalid version")
    merged = Path(merged_path).read_bytes()
    app = image_bytes(merged[0x10000:])
    manifest = {"schema": 1, "chip": "esp32c3", "firmware_version": version,
                "firmware_sha256": hashlib.sha256(merged).hexdigest(),
                "app_sha256": hashlib.sha256(app).hexdigest(), "app_size": len(app)}
    files = {name: (ROOT / name).read_bytes() for name in FILES}
    files["开始使用.command"] = (ROOT / "tools/pet-mac.command").read_bytes()
    files["先读我.txt"] = (ROOT / "tools/pet-mac-readme.txt").read_bytes()
    files["firmware/FoloToy-AI-Passport-full.bin"] = merged
    files["firmware/manifest.json"] = json.dumps(manifest, indent=2).encode()
    # Validate the exact files that will be packaged. No device backups or
    # machine configuration are inputs; output exists -> refuse to overwrite.
    with tempfile.TemporaryDirectory() as temporary:
        firmware = Path(temporary)
        (firmware / "FoloToy-AI-Passport-full.bin").write_bytes(merged)
        (firmware / "manifest.json").write_bytes(files["firmware/manifest.json"])
        load_bundle(firmware)
    hashes = {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}
    files["SHA256SUMS.json"] = json.dumps(hashes, ensure_ascii=False, indent=2).encode()
    prefix = "AI-Pet-Passport-Mac-" + version
    with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(prefix + "/" + name)
            info.create_system = 3
            info.external_attr = (0o100755 if name.endswith(".command") else 0o100644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    print("Created", output, "SHA256", hashlib.sha256(Path(output).read_bytes()).hexdigest())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    package(args.firmware, args.version, args.output)
