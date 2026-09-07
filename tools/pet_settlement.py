#!/usr/bin/env python3
"""Reconcile closed archives through a trusted, local personal-data provider.

No internal endpoints or credentials live here. Provider stdout is never logged.
Missing/incomplete data is not zero. The paired computer is the trust boundary;
these records are not cryptographically certified by a statistics platform.
"""
import calendar
import datetime as dt
import json
from pathlib import Path
import subprocess

import pet_companion as c


def month_value(value):
    if type(value) is not int or not 200001 <= value <= 209912:
        raise ValueError("invalid archive month")
    dt.date(value // 100, value % 100, 1)
    return value


def archive_months(reply):
    value = reply.get("months")
    if value == "none":
        return []
    if not isinstance(value, str) or len(value) > 83:
        raise ValueError("invalid archive list")
    parts = value.split(",")
    if any(len(p) != 6 or not p.isascii() or not p.isdigit() for p in parts):
        raise ValueError("invalid archive list")
    months = [month_value(int(p)) for p in parts]
    if len(months) > 12 or len(set(months)) != len(months):
        raise ValueError("invalid archive list")
    return months


def normalize(record, month, today):
    month_value(month)
    if month >= int(today.strftime("%Y%m")):
        raise ValueError("only closed months can be reconciled")
    if not isinstance(record, dict) or type(record.get("schemaVersion")) is not int or record["schemaVersion"] != 1 or \
            type(record.get("month")) is not int or record["month"] != month or record.get("scope") != "personal" or \
            type(record.get("complete")) is not bool:
        raise ValueError("invalid personal monthly provider response")
    if not record["complete"]:
        return None
    value = record.get("tokens")
    if type(value) is not int or not 0 <= value <= c.MAX_TOKENS:
        raise ValueError("missing or invalid monthly tokens; no settlement sent")
    raw = record.get("coveredThrough")
    if not isinstance(raw, str) or len(raw) != 10:
        raise ValueError("missing coverage watermark")
    covered = dt.date.fromisoformat(raw)
    if covered.isoformat() != raw:
        raise ValueError("invalid coverage watermark")
    end = dt.date(month // 100, month % 100, calendar.monthrange(month // 100, month % 100)[1])
    if covered < end:
        return None
    if covered > today:
        raise ValueError("future coverage watermark")
    return f"PET2 SETTLE {month} {value} {covered:%Y%m%d}"


def load_provider(executable, month, today):
    path = Path(executable).expanduser()
    if not path.is_absolute() or not path.is_file():
        raise ValueError("monthly provider must be an explicit local executable path")
    # No shell, no implicit installation/auth or raw stderr forwarding.
    result = subprocess.run([str(path), str(month)], capture_output=True, text=True, timeout=90)
    if result.returncode or len(result.stdout) > 65536:
        raise ValueError("monthly provider failed; archived growth retained")
    try:
        record = json.loads(result.stdout)
    except (ValueError, TypeError):
        raise ValueError("monthly provider returned invalid JSON") from None
    return normalize(record, month, today)


async def reconcile(executable, key):
    import asyncio
    import pet_wireless as w
    today = dt.datetime.now(c.ZONE).date()
    client, challenge = await w.connect_device(key)
    try:
        months = archive_months(await w.exchange(client, key, challenge, 1, "PET2 MONTHS", "MONTHS"))
    finally:
        await client.disconnect()
    # Do not hold the badge radio while waiting for the statistics provider.
    settled, pending = [], []
    for month in months:
        line = await asyncio.to_thread(load_provider, executable, month, today)
        if line is None:
            pending.append(month)
            continue
        client, challenge = await w.connect_device(key)
        try:
            ack = await w.exchange(client, key, challenge, 1, line, "SETTLED")
            if ack.get("month") != str(month):
                raise ValueError("settlement receipt month mismatch")
            settled.append(month)
        finally:
            await client.disconnect()
    result = {"monthly_settled": settled, "monthly_pending": pending}
    print(json.dumps(result), flush=True)
    return result
