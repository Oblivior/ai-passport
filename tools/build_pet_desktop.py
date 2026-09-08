#!/usr/bin/env python3
"""Build an offline-runtime macOS App from explicit, verified firmware inputs."""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys

from pet_install import image_bytes, load_bundle

ROOT = Path(__file__).resolve().parents[1]
NAME = "AI Pet Passport"


def icon(path):
    """Original lunchbox icon; no licensed character artwork in the desktop icon."""
    import AppKit as A
    image = A.NSImage.alloc().initWithSize_((512, 512))
    image.lockFocus()
    def box(x, y, w, h, rgb, radius=0):
        A.NSColor.colorWithSRGBRed_green_blue_alpha_(*(v / 255 for v in rgb), 1).setFill()
        A.NSBezierPath.bezierPathWithRoundedRect_xRadius_yRadius_(((x, y), (w, h)), radius, radius).fill()
    box(16, 16, 480, 480, (33, 92, 229), 96)
    box(122, 116, 282, 264, (22, 48, 76), 28)
    box(106, 132, 282, 264, (242, 201, 76), 28)
    box(196, 380, 102, 38, (242, 201, 76), 12)
    box(106, 316, 282, 14, (22, 48, 76))
    box(174, 241, 30, 40, (22, 48, 76))
    box(290, 241, 30, 40, (22, 48, 76))
    box(218, 198, 58, 14, (22, 48, 76))
    box(173, 72, 166, 16, (88, 163, 106), 8)
    image.unlockFocus()
    representation = A.NSBitmapImageRep.imageRepWithData_(image.TIFFRepresentation())
    png = representation.representationUsingType_properties_(A.NSBitmapImageFileTypePNG, {})
    path.write_bytes(bytes(png))
    iconset = path.with_suffix(".iconset")
    iconset.mkdir()
    for size in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            target = iconset / (f"icon_{size}x{size}" + ("@2x" if scale == 2 else "") + ".png")
            subprocess.run(["sips", "-z", str(size * scale), str(size * scale), str(path),
                            "--out", str(target)], check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(path.with_suffix(".icns"))], check=True)


def build(firmware, version, output):
    if sys.platform != "darwin" or platform.machine() != "arm64":
        raise ValueError("This release recipe is validated for Apple Silicon macOS only")
    if output.exists():
        raise ValueError("Choose a new output directory; existing artifacts are never overwritten")
    output.mkdir(parents=True)
    resources = output / "resources"
    bundled = resources / "firmware"
    bundled.mkdir(parents=True)
    merged = firmware.read_bytes()
    app = image_bytes(merged[0x10000:])
    manifest = {"schema": 1, "chip": "esp32c3", "firmware_version": version,
                "firmware_sha256": hashlib.sha256(merged).hexdigest(),
                "app_sha256": hashlib.sha256(app).hexdigest(), "app_size": len(app)}
    (bundled / "FoloToy-AI-Passport-full.bin").write_bytes(merged)
    (bundled / "manifest.json").write_text(json.dumps(manifest, indent=2))
    load_bundle(bundled)
    notices = resources / "notices"
    notices.mkdir()
    for source, name in ((ROOT / "LICENSE", "Project-LICENSE"),
                         (ROOT / "assets/fonts/LICENSE.txt", "Font-LICENSE.txt"),
                         (ROOT / "assets/images/README.md", "Character-assets.md"),
                         (ROOT / "assets/images/README.zh_CN.md", "Character-assets.zh_CN.md"),
                         (Path(sys.base_prefix) / "lib/python3.12/LICENSE.txt", "Python-LICENSE.txt")):
        shutil.copyfile(source, notices / name)
    # Runtime and build dependency notices, copied only from this build environment.
    versions = {}
    for dist in importlib.metadata.distributions():
        name = dist.metadata["Name"]
        versions[name] = dist.version
        for file in dist.files or ():
            if any(word in file.name.lower() for word in ("license", "copying", "notice")):
                source = Path(dist.locate_file(file))
                if source.is_file():
                    target = notices / name / str(file).replace("/", "_")
                    target.parent.mkdir(exist_ok=True)
                    shutil.copyfile(source, target)
    (notices / "dependencies.json").write_text(json.dumps(versions, indent=2, sort_keys=True))
    icon(resources / "icon.png")
    spec = output / "desktop.spec"
    spec.write_text(f'''from PyInstaller.utils.hooks import collect_data_files, copy_metadata
a = Analysis([{str(ROOT / "tools/pet_desktop.py")!r}],
    pathex=[{str(ROOT / "tools")!r}],
    datas=[({str(bundled)!r}, "firmware"), ({str(notices)!r}, "notices")]
          + collect_data_files("esptool") + copy_metadata("esptool"),
    hiddenimports=["AppKit", "Foundation", "objc", "PyObjCTools.AppHelper",
                   "CoreBluetooth", "libdispatch", "bleak.backends.corebluetooth"],
    excludes=["tkinter", "pytest", "unittest", "pip", "setuptools"], noarchive=False)
pyz = PYZ(a.pure)
exe = EXE(pyz, a.scripts, [], exclude_binaries=True, name={NAME!r},
          debug=False, strip=False, upx=False, console=False, target_arch="arm64")
coll = COLLECT(exe, a.binaries, a.datas, strip=False, upx=False, name={NAME!r})
app = BUNDLE(coll, name={NAME + ".app"!r}, icon={str(resources / "icon.icns")!r},
    bundle_identifier="io.oblivior.aipetpassport", version="0.2.0",
    info_plist={{"NSHighResolutionCapable": True, "LSMinimumSystemVersion": "12.3",
                "NSBluetoothAlwaysUsageDescription": "通过蓝牙向你已配对的 Passport 发送 Kaboo 饭盒，并确认送达。",
                "NSBluetoothPeripheralUsageDescription": "连接你已配对的 Passport 胸牌。"}})
''')
    subprocess.run([sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean",
                    "--distpath", str(output / "dist"), "--workpath", str(output / "build"), str(spec)], check=True)
    built = output / "dist" / (NAME + ".app")
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(built)], check=True)
    subprocess.run([str(built / "Contents/MacOS" / NAME), "--smoke-test"], check=True)
    delivery = output / "AI-Pet-Passport-Mac-0.2.0-arm64"
    delivery.mkdir()
    shutil.copytree(built, delivery / built.name, symlinks=True)
    shutil.copyfile(ROOT / "tools/pet-desktop-readme.txt", delivery / "先读我.txt")
    # Keep manual relative links valid in the extracted pilot directory.
    for stem in ("pet-desktop", "pet-mac-install"):
        for suffix in (".md", ".zh_CN.md"):
            shutil.copyfile(ROOT / "docs/assets" / (stem + suffix), delivery / (stem + suffix))
    (delivery / "firmware-manifest.json").write_text(json.dumps(manifest, indent=2))
    archive = output / (delivery.name + ".zip")
    subprocess.run(["ditto", "-c", "-k", "--keepParent", str(delivery), str(archive)], check=True)
    archive.with_suffix(".sha256").write_text(hashlib.sha256(archive.read_bytes()).hexdigest() + "  " + archive.name + "\n")
    print("App built and smoke-tested:", built)
    print("Pilot ZIP:", archive)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.firmware.resolve(), args.version, args.output.resolve())
