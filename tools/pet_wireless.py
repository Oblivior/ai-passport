#!/usr/bin/env python3
"""USB-provisioned, application-encrypted BLE lunch delivery. No cloud service."""
import argparse
import asyncio
import datetime as dt
import hashlib
import json
import os
import secrets
import stat
import struct
from pathlib import Path

from cryptography.exceptions import InvalidTag
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
import pet_companion as c

SERVICE = "2b251000-8db0-4bdb-8b45-947717e4e6fa"
INFO = "2b251001-8db0-4bdb-8b45-947717e4e6fa"
REQUEST = "2b251002-8db0-4bdb-8b45-947717e4e6fa"
RESPONSE = "2b251003-8db0-4bdb-8b45-947717e4e6fa"


def key_id(key):
    return hashlib.sha256(key).hexdigest()[:8]


def load_key(path, create=False):
    path = Path(path).expanduser()
    if create and not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        key = secrets.token_bytes(32)
        # Persist before provisioning: a lost USB ACK can safely retry this key.
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "w") as file:
            json.dump({"key": key.hex(), "id": key_id(key)}, file)
            file.flush()
            os.fsync(file.fileno())
    if path.is_symlink() or not path.is_file():
        raise ValueError("pairing file missing; run --pair-usb first")
    if os.name == "posix" and (stat.S_IMODE(path.stat().st_mode) & 0o077):
        raise ValueError("pairing file must have private permissions (chmod 600)")
    try:
        record = json.loads(path.read_text())
        key = bytes.fromhex(record["key"])
        if len(key) != 32 or record["id"] != key_id(key):
            raise ValueError()
    except (ValueError, KeyError, TypeError):
        raise ValueError("invalid pairing file; existing device pairing was not changed") from None
    return key


def seal(key, challenge, sequence, line, direction=b"PET3-C"):
    encoded = line.encode("ascii")
    if len(challenge) != 16 or not 0 < sequence <= 0xFFFFFFFF or not 0 < len(encoded) < 160:
        raise ValueError("invalid encrypted frame")
    nonce = secrets.token_bytes(12)
    return nonce + AESGCM(key).encrypt(nonce, struct.pack(">I", sequence) + encoded, direction + challenge)


def unseal(key, challenge, sequence, frame, direction=b"PET3-S"):
    if not 33 <= len(frame) <= 192:
        raise ValueError("invalid encrypted response length")
    plain = AESGCM(key).decrypt(frame[:12], frame[12:], direction + challenge)
    if plain[:4] != struct.pack(">I", sequence):
        raise ValueError("stale response sequence")
    return plain[4:].decode("ascii")


def parse_reply(line, expected):
    if not line.startswith("PET2 " + expected + " "):
        raise ValueError("device rejected wireless request: " + line)
    return dict(part.split("=", 1) for part in line.split()[2:])


async def connect_device(key):
    from bleak import BleakClient, BleakScanner
    expected = "AIPet-" + key_id(key)
    device = await BleakScanner.find_device_by_filter(
        lambda device, adv: adv.local_name == expected, service_uuids=[SERVICE], timeout=15)
    if device is None:
        raise TimeoutError("paired Passport not found; check power, range and Bluetooth permission")
    client = BleakClient(device, timeout=15)
    try:
        await client.connect()
        info = bytes(await client.read_gatt_char(INFO))
        if len(info) != 25 or info[0] != 3 or info[1:9] != key_id(key).encode():
            raise ValueError("device identity/protocol mismatch; no usage sent")
        return client, info[9:]
    except BaseException:
        await client.disconnect()
        raise


async def exchange(client, key, challenge, sequence, line, expected):
    await client.write_gatt_char(REQUEST, seal(key, challenge, sequence, line), response=True)
    deadline = asyncio.get_running_loop().time() + 8
    while asyncio.get_running_loop().time() < deadline:
        response = bytes(await client.read_gatt_char(RESPONSE))
        if response:
            return parse_reply(unseal(key, challenge, sequence, response), expected)
        await asyncio.sleep(.1)
    raise TimeoutError("no authenticated device ACK; no success reported, retry is safe")


async def sync_once(args, key):
    # Scan the local data before connecting; do not occupy the radio while scanning files.
    totals = await asyncio.to_thread(c.load_source, args)
    today = dt.datetime.now(c.ZONE).date()
    goal = args.goal or c.choose_goal(totals, today)
    client, challenge = await connect_device(key)
    try:
        state = await exchange(client, key, challenge, 1, "PET2 STATUS", "STATUS")
        if int(state["date"]) // 100 == int(today.strftime("%Y%m")):
            goal = int(state["goal"])
            if args.goal and args.goal != goal:
                raise ValueError("monthly goal is locked; no sync sent")
        result = await exchange(client, key, challenge, 2, c.make_snapshot(totals, today, goal), "ACK")
        print(json.dumps({"source": "kaboo-local", "transport": "ble-aes256gcm", "device": result}), flush=True)
        return result
    finally:
        await client.disconnect()


async def watch(args, key):
    from bleak.exc import BleakError
    while True:
        try:
            await sync_once(args, key)
        except (OSError, ValueError, TimeoutError, InvalidTag, BleakError, c.subprocess.TimeoutExpired) as exc:
            # Never print raw packet or key content.
            print("Wireless sync failed: " + (str(exc) or type(exc).__name__), flush=True)
            if not args.watch:
                raise SystemExit(1) from None
        if not args.watch:
            return
        await asyncio.sleep(args.interval)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", default="~/.config/ai-passport/link.json")
    parser.add_argument("--pair-usb", metavar="PORT", help="provision once over trusted USB; never prints the key")
    parser.add_argument("--watch", action="store_true")
    parser.add_argument("--interval", type=int, default=300)
    parser.add_argument("--csv")
    parser.add_argument("--kaboo-cli", default="kaboo-cli")
    parser.add_argument("--goal", type=int)
    args = parser.parse_args()
    if args.interval < 60:
        parser.error("--interval must be at least 60 seconds")
    key = load_key(args.config, create=bool(args.pair_usb))
    if args.pair_usb:
        port = c.open_port(args.pair_usb)
        try:
            result = c.exchange(port, "PET2 PAIR " + key.hex(), "PAIRED")
            if result.get("id") != key_id(key):
                raise ValueError("pairing identity mismatch")
            print("Paired AIPet-" + key_id(key) + "; key saved privately; USB can now be unplugged.")
        finally:
            port.close()
        return
    asyncio.run(watch(args, key))


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
    except (OSError, ValueError, TimeoutError) as exc:
        raise SystemExit(str(exc)) from None
