<p align="right"><a href="pet-desktop.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Mac desktop companion 0.2.0

An Apple Silicon macOS 12.3+ pilot with a native Chinese window, menu-bar
controls and embedded Python 3.12 runtime. Recipients need neither Python, Git,
ESP-IDF nor a terminal to operate the App. They still need their own authorized
Kaboo CLI with compatible `export --format csv`, Bluetooth, and a data USB cable
for first installation/pairing. Intel and Windows are not supported by this App
package. A clean second Mac and factory badge remain acceptance gates.

## First use

1. Verify the distributor's ZIP SHA-256. Unzip and move `AI Pet Passport.app`
   into `/Applications` or your own `~/Applications`. Open the App. This pilot
   has an ad-hoc integrity signature, **not** a Developer ID signature or Apple
   notarization. Follow system security prompts only after verifying the source;
   never disable Gatekeeper. Managed Macs may require IT approval.
2. Stop legacy feeding scripts. Connect only one badge. For a badge not yet on
   this pet firmware, use **Install firmware**. It backs up and checks compatibility
   before asking for final write confirmation. Other applications are replaced;
   this is not a universal factory flasher. See [installation safety](pet-mac-install.md).
3. After boot, select the pet on the badge and adopt an egg. Use **USB pairing**.
   Existing users choose **Use legacy pairing** or **Load pairing** instead. Lost keys are
   not silently reset, and different badges must never share a key.
4. The App detects common Kaboo installations, including nvm. Otherwise select
   the user's CLI executable. Click **Check** after normal usage produces today's
   data, then **Start feeding**. Grant the expected macOS Bluetooth permission.
5. Success requires an authenticated badge ACK. Last delivery, pending food,
   stage and care days are snapshots, not continuous badge telemetry. Press OK
   **on the badge** to eat; the desktop does not fake meals or pet growth.

## Everyday behavior

- Approximately one minute after each completed attempt, fetch cumulative local
  usage again. Source errors, missing today's data or Bluetooth failures retry;
  no fabricated zero is sent. Repeated snapshots cannot duplicate food.
- **Stop feeding** cancels export/discovery/sync and preserves growth. Close the
  window to leave the menu-bar app running. Use the menu to show/start/stop/quit.
  Quitting drains the cancellable worker; ongoing USB checks/writes block quit.
- Login auto-start is **off by default**. Only the user's explicit checkbox
  confirmation creates a per-user login plist. The App must already be in an
  Applications folder. Disable the checkbox to remove that item. No system
  daemon, auto-update mechanism, cloud service or privileged helper is installed.
- Login behavior is tested with isolated plist fixtures, not by logging out this
  computer. Sleep/wake, fresh-account permissions and managed-device policies
  need another user's installation test.

## Kaboo only; no official settlement

This product flow uses only Kaboo feeding. It does not run an official monthly
provider or promise an official settlement. Local month rollover and the family
album remain. New pending archives are displayed as local growth records, not as
waiting for a non-existent service. Existing v5 saves and optional advanced
reconciliation protocol are retained for compatibility; no archive is deleted.

## Private data and failure recovery

The App keeps preferences and last ACK summaries under
`~/Library/Application Support/AI Pet Passport/` (directory 700, files 600).
Pairing keys and full-device backups are private. Only aggregate snapshots are
sent to the user's paired badge with the existing encrypted application protocol.
Raw CSV stays in memory and temporary scan caches are removed after each run.
Export output is bounded to 64 MiB and 120 seconds. Stop terminates only the
process group started for that export, never an existing Kaboo app.

Do not share keys, backups, raw exports or the whole private folder. For support,
send App version, macOS/chip, the visible error and which step failed. The package
contains no personal configuration, internal exporter or official-monthly provider.
Kaboo itself retains its own data/network behavior. Character artwork is fan
content; code licensing does not grant character redistribution rights.

If partition/Recovery/identity checks fail, installation stops without an app
write. Keep the backup and contact the maintainer; do not erase or use an unsafe
flasher to bypass this check. The maintainer's current badge has an already-empty
Recovery region and cannot prove a successful factory-install path. Its existing
pet firmware still supports daily feeding. A valid image header does not prove
Recovery can actually boot.

## Maintainer build and verification

Use a redistributable standalone CPython 3.12 on Apple Silicon, not the Apple
Command Line Tools Python. Install `tools/requirements-pet-desktop-build.txt` in
an isolated build environment. Record the Python source archive SHA-256. Then:

```bash
python tools/build_pet_desktop.py --firmware /path/to/FoloToy-AI-Passport-full.bin --version VERIFIED_COMMIT --output /new/build-directory
python tests/test_pet_desktop.py
python tests/test_pet_wireless.py
./tools/validate.sh --static
```

The build validates the firmware, embeds runtime dependencies and notices,
creates a native lunchbox icon and Bluetooth usage descriptions, verifies the
ad-hoc signature and runs the frozen bundle smoke test. Firmware must come from
the corresponding verified CI artifact. Build output is never overwritten.

Test extraction and relocated App launch with a minimal PATH, source discovery,
private pairing reuse, actual authenticated BLE ACK, stop/retry, window close,
menu-bar reopen and quit. Repeat signature/smoke validation on the extracted ZIP.
Test the existing installer's positive/negative cases without flashing a badge
with failed preflight. Formal Developer ID signing/notarization is a separate
release step requiring the owner's Apple account/certificate; never treat ad-hoc
verification as public-distribution approval.
