<p align="right"><a href="pet-mac-install.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Mac pilot installation

This is a terminal wizard, not a signed/notarized application or an offline,
dependency-free installer. It targets macOS 12.3+ with Python 3.9–3.13 (3.12
recommended), a data-capable USB cable and internet access for first-time Python
dependencies. Users do not need Git, ESP-IDF, Codex or a GitHub account when given
the ZIP directly. Another clean Mac and Windows remain unverified.

## User flow

Verify the ZIP SHA-256 against the trusted distributor's value, unzip it, and
keep the directory intact. Double-click the launcher with the `.command` suffix.
If macOS blocks it, verify provenance and follow the system security prompts;
do not disable Gatekeeper. If Python is missing, install Python 3.12 from
python.org; the launcher does not install Xcode or Homebrew. It requests consent
before installing pinned top-level dependencies into a private virtualenv.

1. Stop old feeding processes and connect only one Passport. Choose option 1.
2. Type `CHECK` to inspect/reboot and back up the device. Only type `INSTALL`
   after the backup and compatibility checks pass. Other input cancels writing.
3. Wait for the badge to boot, reopen the wizard, choose option 3 and type `PAIR`.
   It checks house v5/storage health, provisions a private USB key and verifies
   the acknowledgement. Choose the pet application and adopt an egg on-device.
4. Choose option 4, select the paired badge and provide the user's authorized
   Kaboo CLI path, or press Enter to search `kaboo-cli` on PATH. The exporter
   must support `export --format csv` with this fork's required schema. Normal
   use must produce today's data before this preflight passes.
5. Grant Bluetooth permission through macOS when prompted. Keep the window open
   for approximately one-minute synchronization. Only an authenticated device
   ACK is reported as successful delivery. Ctrl-C stops it; no autostart service
   is installed. Sleep or loss of Bluetooth defers delivery without punishment.

The wizard serializes its own instances. It cannot stop or detect every legacy
feeding script; stop those manually. Pairing profiles are separated by a hash of
the native USB serial number, not by a reused serial port name. A paired badge
with a missing private key is rejected, never reset. The advanced `--config`
argument can explicitly select its existing private key file; do not reuse a
different badge's key or share it with others.

## Safety and recovery

Only the exact five-partition layout checked by this installer is supported.
Other factory/application layouts are rejected: this is not a universal factory
flasher. Use the manufacturer's compatible installation flow for those devices;
the mini-program path for this pet build remains unverified.

The tool requires ESP32-C3, 8 MB flash, no secure boot/flash encryption, matching
partition bytes and MD5, a known UP Recovery hook, a non-empty identity region
and an ESP image header at Recovery. These checks do not prove Recovery boots.
It reads all 8 MiB twice and compares them before permitting an app-only write at
`0x10000`. A single serial connection is retained, with no automatic write retry
after disconnection. Bootloader, partition table, NVS, device identity and
Recovery are not written. The existing application is replaced; arbitrary
third-party application saves are not guaranteed compatible.

Before reboot, it reads the entire flash back, verifies the app bytes and both
protected ranges outside the 3 MB app slot. This is distinct from the subsequent
boot/pairing check and from real BLE/Kaboo delivery. Any failure stops the flow.
No automatic full-flash restore, downgrade or erase-all command is provided.
Keep the backup and ask the maintainer to identify the failed stage and the
same physical device before attempting restoration.

Private backups, per-device pairing keys and the runtime remain under
`~/Library/Application Support/AI Pet Passport/` with private permissions.
Do not send that folder, raw exports or sensitive terminal output to support.
The ZIP whitelist contains no user backup, device identity, pairing key, internal
Kaboo implementation or official-monthly provider. It does not automatically
enable official reconciliation. The standard exporter uses an isolated temporary
scan cache; the maintainer's private cached exporter is not part of this package.
No data is uploaded by the installer. Kaboo itself retains its own behavior.

SHA-256 detects corruption, not malicious replacement of the ZIP and manifest.
Obtain packages from a trusted source. Character assets are fan content; a code
license does not establish character or commercial redistribution rights.

## Maintainer packaging

Use the same verified merged firmware as the corresponding CI build, with a
pinned esptool environment. The tool refuses to overwrite an existing ZIP:

```bash
python3 tools/package_pet_mac.py --firmware /path/to/FoloToy-AI-Passport-full.bin --version c8bcd65-pilot1 --output /path/to/AI-Pet-Passport-Mac.zip
```

The package keeps the merged artifact required by Recovery, but the USB wizard
extracts and validates only the app. It never raw-flashes the merged file.
The manifest records both checksums and the app size. The ZIP includes a
per-file checksum list, Chinese quick-start text and both language manuals.
Packaging is separate from publishing: creating a ZIP does not create a tag,
GitHub Release or community listing. The current firmware is unchanged by this
installer work; rebuilding/flashing it is not necessary to test host-only logic.
