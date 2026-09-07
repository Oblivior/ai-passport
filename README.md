<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# AI Pet Passport: daily lunchbox

This fork consumes local Kaboo CSV exports to feed an original pixel pet over
USB or application-encrypted BLE. It is a playable prototype, not a finished product.

## Play

Open AI Pet in the menu. UP/DOWN switch home, next evolution, routes and family. Click
OK once to eat an earned meal; an empty box makes OK pet/wake the character,
without growth. Eating lasts 1.2 seconds and evolution lasts 1.8 seconds. Idle
pets sleep after 30 seconds without penalties. On family, OK browses records;
long OK returns to the hardware menu. Double-click no longer creates food.

| Stage | Meals eaten | Usage days with eaten food |
| --- | --- | --- |
| EGG | 0 | 0 |
| SPARK | 1 | 1 |
| BYTE | 3 | 2 |
| SCOUT | 6 | 3 |
| RANGER | 12 | 6 |
| TITAN | 24 | 10 |
| APEX | 40 | 16 |

Both conditions are required. Offline catch-up credits the original food dates,
not the button-press date. First adoption ignores earlier history. Unclaimed
food stays until the next monthly sync, when it expires.

### Three evolution routes

The route page shows a forecast; OK cycles through final-form previews without
feeding, changing the save or selecting a route. At RANGER (12 eaten meals and
6 usage days), the route locks for the month and stays in the family archive.
The three routes use the same growth requirements; none is a higher rank.

Only completed days since adoption vote, using earned food (not click timing):
1-2 meals is a light day, 3 is mixed, 4-5 is high. Strictly more light days than
each other category selects EXPLORER; strictly more high days selects WILD;
mixed-day wins and ties select ARMOR. Days without food do not vote. Today is
excluded because its allowance can still grow. No completed usage day shows
CORE / waiting. Locked routes do not change with later usage or corrections.

These are local food-intensity routes, not inferences about work quality, task
depth or tool diversity, and not Bits-certified routes. Armor has a shield and
blue plates; Wild has an orange body and red mane; Explorer has a green body and
red scarf. All keep the existing seven stages and original procedural artwork.

The v2 save shape is unchanged: the previously unused current `family.route`
stores the lock. Earlier adult v2 saves acquire a route on their next sync or
meal, without losing stage or food. Pre-RANGER monthly records remain CORE.

## Companion

Requires Python 3.9+, `pyserial==3.5` and an installed `kaboo-cli` supporting
`export --format csv`. Run from this repository with the identified device port:

```bash
python3 tools/pet_companion.py --preview
python3 tools/pet_companion.py --port /dev/cu.usbmodem101
python3 tools/pet_companion.py --port /dev/cu.usbmodem101 --watch
```

Watch mode keeps the serial connection open and retries after disconnection.
Startup retries use a read-only handshake. The first connection may return to
the main menu; subsequent syncs should not repeatedly reboot the device.

Default: one sync. `--watch`: every 300 seconds while the process runs, not an
auto-start service. Stop it before flashing. `--csv path/to/export.csv` reads an
explicit export instead. Never commit real exports.

With an exporter supporting `export --cached`, add `--cached-export` to either
companion and select that executable with `--kaboo-cli /path/to/cli`.
Its dedicated persistent cache reuses unchanged files and parses safe appends;
CSV output is still a cumulative replacement snapshot, never a token delta.
First-run baseline scanning can be slow. Unsupported flags, invalid output and
timeouts stop that cycle without fallback or fake food. The default full-export
mode remains compatible with older CLIs. The exporter cache contains local source
data: keep it private and separate from the reporter cache. No internal exporter
implementation is included here.

First positive usage earns one meal; exceeding each additional 20% of the daily
target unlocks another, capped at five. The target is the previous 14 active
days' median, excluding today, or 1,000,000 without history. `--goal` can override
it on first monthly sync; it is then locked for that month. Dates use Beijing
time. Local coverage is not the official Bits monthly total.

The companion disables CLI auto-update, uses an isolated scan cache, processes
CSV in memory and retains no raw exports, prompts, projects, hostnames or
credentials. The CSV consumer is independently implemented, not copied from
internal Kaboo code. Only date, daily aggregate tokens, target and daily meal
allowances reach the device. Invalid/missing data stops sync, not a fake zero.

## Storage and transport

### Wireless lunch delivery (Phase 2)

The macOS companion targets 12.3 or later. Discovery matches the service and
paired ID locally and does not log information about other nearby devices.

Install `tools/requirements-pet-wireless.txt` in the companion virtual environment.
Stop the USB watch process, then provision once over the connected device's USB:

```bash
python3 -m pip install -r tools/requirements-pet-wireless.txt
python3 tools/pet_wireless.py --pair-usb /dev/cu.usbmodem101
python3 tools/pet_wireless.py --watch
```

After pairing, unplug USB and keep the badge powered, computer Bluetooth enabled,
and companion running. Each cycle waits 300 seconds, exports local usage, connects,
syncs and disconnects. No autostart service is installed. macOS may request terminal
Bluetooth permission; only the user may grant it. Out-of-range/sleep delays sync
without losing food; the next cycle retries. Collection uses full scans by
default; `--cached-export` opts into the cached cumulative export described above.
Discovery/connection timeouts get up to three attempts per cycle with 2/4-second
backoff and fresh discovery. Identity/authentication failures are not retried by
this connection policy. Cold connection latency remains variable on macOS.

`--config` selects a pairing file; default `~/.config/ai-passport/link.json` must
be mode 0600. The key is saved locally before USB provisioning, so a lost ACK can
retry the same file. A paired device rejects another key. Keep the only key copy:
wireless reset/rebinding is not implemented. Pet Link shows connection/errors;
leaving it does not stop the radio. Progress distinguishes USB / WIRELESS SYNC OK.
Authentication failures never fall back to plaintext or USB.

Security boundary: standard AES-256-GCM application encryption/authentication,
not OS BLE bonding. Each connection has a fresh 16-byte challenge. Direction plus
challenge forms the AAD; frames have a random 12-byte nonce and 16-byte tag.
Plaintext contains a 4-byte big-endian increasing sequence followed by PET2 text.
Wrong keys, tampering, cross-session or old-sequence replay cannot feed the pet.
Only STATUS/ROUTE/SYNC are accepted; PAIR and LINK diagnostics are USB-only. Idle
unauthenticated clients time out; a one-item queue and failure limit bound work.
This does not prevent radio interference or sustained denial of service. Pairing
files and device NVS are not encrypted at rest: physical access or local file access
can expose the key. Never upload pairing files, NVS backups or raw exports.

Service: `2b251000-8db0-4bdb-8b45-947717e4e6fa`; replace `1000` with
`1001/1002/1003` for INFO/REQUEST/RESPONSE. INFO contains byte 3, eight ASCII
pairing-ID bytes and the challenge. Request AAD is `PET3-C` plus challenge;
response AAD is `PET3-S` plus challenge. Frame order: nonce, ciphertext, tag.
A successful GATT write is not delivery: require the authenticated application
ACK. Daily cumulative entitlements and commit-before-ACK remain unchanged.

Legacy `ai_pet` saves remain untouched. New `ai_pet_v2` alternating CRC-protected
slots import old pets as DEMO MEMORY; the live game starts as an egg. Family
browses 12 recent records; older legacy records remain in the original save.
Unreadable v2 slots block writes rather than silently resetting progression.
Use app-only updates at `0x10000` after checking the partition table. Never erase
flash; preserve device identity and Recovery.

USB is a trusted local cable protocol, not an authenticated remote API. It uses
the [existing non-blocking console](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32c3/api-guides/stdio.html):

```text
PET2 STATUS
PET2 ROUTE
PET2 SYNC YYYYMMDD <tokens_today> <daily_goal> <31 digits, each 0..5>
```

Lines are bounded to 159 characters. Reject invalid/backwards dates, future-day
food, unknown commands and mid-month goal changes. Cumulative daily high-water
marks prevent replay/ACK-loss duplicates and never undo earned food after a
source correction. ACK follows NVS commit; failed writes do not change the live
state. There is no serial command to eat: that requires a physical button.

First sync in a later month archives the pet as LOCAL CHAPTER and starts an egg.
This is local rollover, not Bits settlement. While offline, the device waits for
host time instead of inventing a new month.

## Verification and boundaries

Run `./tools/validate.sh --static` and the real LVGL tests in
[build and test](docs/development/build-and-test.md). Page tests emit PPM images
and cover layout, eating, evolution, sleep, repeated keys and teardown. Pure
tests cover three-day progression, 16-day evolution gates, replay, catch-up,
date validation, month rollover and legacy import.

Also run `python3 tests/test_pet_wireless.py` with the wireless dependencies:
authentication, tampering, direction isolation, stale sequences, bounds, private
configuration and ACK handling. Firmware rejection behavior, real wireless ACKs
and radio-enabled memory require separate device checks.

ROUTE is read-only and returns `route`, `locked` and `stage`; route IDs are
0=CORE (unformed), 1=ARMOR, 2=WILD, 3=EXPLORER. Existing STATUS/SYNC fields stay unchanged.

Remaining: Bits settlement and certified feature-based routes, independent Flux
adapter, selectable species, encounters, sound and production sprite artwork.
Physical power-loss tests, battery endurance and a three-day human playtest are
separate acceptance steps; host simulations do not prove those outcomes.
Concurrent Wi-Fi scans and the 96 KB recording demo with the persistent radio
have not completed resource acceptance; use AI Pet / Pet Link for daily play.
