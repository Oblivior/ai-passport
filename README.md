<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# AI Pet Passport: daily lunchbox

This fork consumes local Kaboo CSV exports to feed an original pixel pet over
USB. It is a playable prototype, not the finished wireless product.

## Play

Open AI Pet in the menu. UP/DOWN switch home, next evolution and family. Click
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

Remaining: BLE transport, Bits settlement, independent Flux adapter, selectable
species, branching evolution, encounters, sound and production sprite artwork.
Physical power-loss tests, battery endurance and a three-day human playtest are
separate acceptance steps; host simulations do not prove those outcomes.
