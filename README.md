<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# AI Pet Passport: Digimon companions

This fork consumes local Kaboo CSV exports to feed three lines of pixel pets over
USB or application-encrypted BLE. It is a playable prototype, not a finished product.

## Play

Open the Digimon entry in the menu. UP/DOWN switch home, daily lunchbox, next evolution, evolution catalog, partner house and family. Click
OK once to eat an earned meal; an empty box makes OK pet/wake the character,
without growth. Eating lasts 1.2 seconds and evolution lasts 1.8 seconds. Idle
pets sleep after 30 seconds without penalties. On family, OK returns home when
there are zero or one records, and cycles through records when there are more;
long OK returns to the hardware menu. Double-click no longer creates food.

| Stage | Meals eaten | Usage days with eaten food |
| --- | --- | --- |
| Digi-Egg | 0 | 0 |
| Botamon | 1 | 1 |
| Koromon | 3 | 2 |
| Agumon | 6 | 3 |
| Greymon | 12 | 6 |
| MetalGreymon | 24 | 10 |
| WarGreymon | 40 | 16 |

Both conditions are required; all three lines use these stage thresholds. Offline
catch-up credits food source dates, but never dates before that partner's adoption:
older inventory counts as one adoption day, not many days of companionship.
The first-ever sync ignores earlier history. Unclaimed
food stays until the next monthly sync, when it expires.

### Daily lunchbox

Five slots show eaten, available and not-yet-earned meals for the displayed source
date. The page shows that date's exact Token snapshot, the remaining Tokens and
progress to the next meal, and older pending food separately. Food is consumed
oldest-first; eating yesterday's food does not fill today's eaten slots. OK on
this page returns home without feeding. At five earned meals the page marks the
daily cap instead of encouraging further usage.

The first positive usage earns food; each subsequent meal requires strictly
exceeding a 20% boundary of the locked daily goal. Downward data corrections do
not revoke earned food. This is a periodically synchronized local snapshot, not
a real-time counter or a Bits-certified total. Before the first sync after boot,
or after ten minutes without sync, the page labels it as the previous lunchbox
and retains its source date. New food produces a four-second home notice without
interrupting eating/evolution or switching pages; identical syncs do not replay
the notice. Food allowances and the SYNC protocol are unchanged.

### Three complete lines and partner house

- Digi-Egg / Botamon / Koromon / Agumon / Greymon / MetalGreymon / WarGreymon.
- Digi-Egg / Punimon / Tsunomon / Gabumon / Garurumon / WereGarurumon / MetalGarurumon.
- Digi-Egg / Poyomon / Tokomon / Patamon / Angemon / MagnaAngemon / Seraphimon.

On a fresh start, choose a line with UP/DOWN and confirm twice to adopt its egg.
In the partner house, OK enters selection; UP/DOWN browse the three partners and
a return item. The confirmation screen explains whether this is a new egg or a
returning partner. UP/DOWN cancels confirmation; OK saves the choice. The preview
for an unadopted line shows its Rookie form, explicitly labeled as a preview of
a line that starts from an egg. Long OK always leaves for the hardware menu.

Each line may be adopted once per month. Switching resumes independent meals,
days and form. All partners share the same daily allowance and inventory; neither
selection nor repeated sync grants extra meals. Only the companion beside you
eats; resting partners do not lose growth. The catalog distinguishes actually
raised forms from previews. Discoveries survive monthly rollover; browsing alone
never unlocks a form. There is no species-changing skin operation.

Existing v2 live progress migrates to Agumon without losing its food or days.
Startup removes obsolete robot archives (`species_id=0`) from the active v3 save,
including during legacy migration. Current partners, food, growth, discoveries
and real Digimon archives remain intact. Original legacy namespaces and private
upgrade backups remain available for recovery. If cleanup cannot be committed,
the original state is retained and writes are blocked until a successful reboot.
The existing
food-intensity `family.route` values and ROUTE diagnostics remain for compatibility
but describe the shared usage ledger, not a species or a branch. No SkullGreymon
branch is implemented, and high Token usage is not treated as bad care.

The embedded graphics are hand-authored pixel fan art, not official sprites or
franchise-original characters. Character rights are not granted by the code
license. No commercial or public redistribution authorization is claimed; review
rights before public distribution. See [asset details](assets/images/README.md).

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

New `ai_pet_v3` alternating CRC-protected slots store the shared pantry and
independent partners. Legacy `ai_pet`, `ai_pet_v2` and pairing saves are not written.
Only an absent v3 save allows migration; unreadable or incompatible saves block
writes instead of silently resetting. One corrupt slot can recover from the other;
CRC-valid unknown species block downgrade. A failed commit keeps the live state
unchanged. Family holds the latest 12 individual records, not 12 months.
Downgrading to v2 resumes its old snapshot, not subsequent v3 progress; do not play
on both versions and expect their diverging saves to merge. Back up before upgrade.
Use app-only updates at `0x10000` after checking the partition table. Never erase
flash; preserve device identity and Recovery.

USB is a trusted local cable protocol, not an authenticated remote API. It uses
the [existing non-blocking console](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32c3/api-guides/stdio.html):

```text
PET2 STATUS
PET2 ROUTE
PET2 HOUSE
PET2 SYNC YYYYMMDD <tokens_today> <daily_goal> <31 digits, each 0..5>
```

Lines are bounded to 159 characters. Reject invalid/backwards dates, future-day
food, unknown commands and mid-month goal changes. Cumulative daily high-water
marks prevent replay/ACK-loss duplicates and never undo earned food after a
source correction. ACK follows NVS commit; failed writes do not change the live
state. There is no serial command to eat: that requires a physical button.

HOUSE is USB-only and reports schema version, active species ID, adopted-bit mask
and storage health. STATUS meals/days/stage refer to the active partner; pending
food remains shared. There are no remote commands to adopt, switch or eat.

First sync in a later month archives every adopted partner, expires leftover
food, preserves lifetime discoveries and asks you to choose a new egg. This is
local rollover, not Bits settlement. Offline, the device waits for host time.

### Adding a line

`main/pet_catalog.c` maps stable IDs (1 Agumon, 2 Gabumon, 3 Patamon) to names and
artwork. Add a unique ID, a catalog entry, all seven forms and three poses per
form; increment the catalog count and asset dimensions, regenerate sprites and
the UI font subset, then extend the host cases and visually check the new names.
The fixed save capacity is eight IDs. Never renumber existing IDs or use menu
positions as identity; exceeding capacity needs a new explicit schema migration.
An old firmware that cannot interpret saved species fails closed. This is a
firmware/content update workflow, not a hot-download plugin system.

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
adapter, branching evolution, encounters, sound and production sprite artwork.
Physical power-loss tests, battery endurance and a three-day human playtest are
separate acceptance steps; host simulations do not prove those outcomes.
Concurrent Wi-Fi scans and the 96 KB recording demo with the persistent radio
have not completed resource acceptance; use AI Pet / Pet Link for daily play.
