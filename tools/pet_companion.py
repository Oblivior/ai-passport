#!/usr/bin/env python3
"""Local Kaboo buckets -> daily food entitlements -> Passport USB.

No API credentials, prompts, project names or raw export files are retained.
The default is a one-shot sync; --watch keeps syncing while this process runs.
"""
import argparse
import csv
import datetime as dt
import io
import json
import os
import statistics
import subprocess
import tempfile
import time
from pathlib import Path

ZONE = dt.timezone(dt.timedelta(hours=8))
DETAILS = ("inputTokens", "outputTokens", "cachedInputTokens",
           "cacheCreationInputTokens", "reasoningOutputTokens")
MAX_TOKENS = 9007199254740991


def token_count(value):
    if isinstance(value, bool) or not str(value).isdigit():
        raise ValueError("invalid token count; sync stopped")
    n = int(value)
    if n > MAX_TOKENS:
        raise ValueError("token count exceeds supported range")
    return n


def aggregate_csv(content):
    reader = csv.DictReader(io.StringIO(content))
    required = {"bucketStart", "totalTokens", "source", "model", "project", "hostname"}
    if not required.issubset(set(reader.fieldnames or [])):
        raise ValueError("Kaboo CSV schema unavailable; no zero snapshot will be sent")
    totals, seen = {}, {}
    for row in reader:
        stamp = dt.datetime.fromisoformat(row["bucketStart"].replace("Z", "+00:00"))
        if stamp.tzinfo is None:
            raise ValueError("bucket timestamp must include timezone")
        key = tuple(row[k] for k in ("source", "model", "project", "hostname", "bucketStart"))
        count = max(token_count(row["totalTokens"]),
                    sum(token_count(row.get(k) or "0") for k in DETAILS))
        if key in seen:
            if seen[key] != count:
                raise ValueError("conflicting duplicate bucket; sync stopped")
            continue
        seen[key] = count
        date = stamp.astimezone(ZONE).date()
        totals[date] = totals.get(date, 0) + count
        if totals[date] > MAX_TOKENS:
            raise ValueError("daily total exceeds supported range")
    return totals


def food(tokens, goal):
    # First positive use earns the first meal; 20/40/60/80% unlock the rest.
    return min(5, (tokens * 5 + goal - 1) // goal) if tokens else 0


def choose_goal(totals, today):
    days = sorted(d for d in totals if d < today and totals[d] > 0)[-14:]
    return max(1, min(10**12, int(statistics.median(totals[d] for d in days)))) if days else 1000000


def make_snapshot(totals, today, goal):
    if type(goal) is not int or not 1 <= goal <= 10**12:
        raise ValueError("daily goal must be 1..1000000000000")
    earned = []
    for day in range(1, 32):
        count = totals.get(today.replace(day=day), 0) if day <= today.day else 0
        earned.append(str(food(count, goal)))
    tokens = totals.get(today, 0)
    date = today.strftime("%Y%m%d")
    return f"PET2 SYNC {date} {tokens} {goal} {''.join(earned)}"


def load_source(args):
    if args.csv:
        return aggregate_csv(Path(args.csv).read_text(encoding="utf-8-sig"))
    env = os.environ.copy()
    env["KABOO_SKIP_AUTO_UPDATE"] = "1"
    # Export full scans may update the scan cache. Isolate it from the reporter.
    with tempfile.TemporaryDirectory(prefix="passport-kaboo-") as directory:
        env["KABOO_SCAN_CACHE_PATH"] = str(Path(directory) / "scan-cache.db")
        result = subprocess.run([args.kaboo_cli, "export", "--format", "csv"],
                                capture_output=True, text=True, timeout=120, env=env)
    if result.returncode:
        raise ValueError("Kaboo export failed; existing pet state retained")
    return aggregate_csv(result.stdout)


def exchange(port, line, expected):
    port.write(("\n" + line + "\n").encode("ascii"))
    port.flush()
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        response = port.readline(512).decode("ascii", errors="replace").strip()
        if response.startswith("PET2 "):
            if not response.startswith("PET2 " + expected + " "):
                raise ValueError("device rejected sync: " + response)
            return dict(part.split("=", 1) for part in response.split()[2:])
    raise TimeoutError("device did not acknowledge; retry is safe")


def sync_once(args, port=None):
    totals = load_source(args)
    today = dt.datetime.now(ZONE).date()
    goal = args.goal or choose_goal(totals, today)
    if args.preview:
        print(json.dumps({"source": "kaboo-local", "date": str(today),
                          "today_tokens": totals.get(today, 0), "daily_goal": goal,
                          "today_food": food(totals.get(today, 0), goal),
                          "active_history_days": sum(v > 0 for d, v in totals.items() if d < today)},
                         ensure_ascii=False))
        return
    owned = port is None
    if owned:
        port = open_port(args.port)
    try:
        state = exchange(port, "PET2 STATUS", "STATUS")
        if int(state["date"]) // 100 == int(today.strftime("%Y%m")):
            goal = int(state["goal"])
            if args.goal and args.goal != goal:
                raise ValueError("monthly goal is already locked; change it next month")
        reply = exchange(port, make_snapshot(totals, today, goal), "ACK")
        print(json.dumps({"source": "kaboo-local", "date": str(today), "device": reply}, ensure_ascii=False))
    finally:
        if owned:
            port.close()


def open_port(path):
    import serial  # Optional for pure aggregation/tests/preview.
    port = serial.Serial(port=None, baudrate=115200, timeout=0.3, write_timeout=3)
    # The C3 USB control lines are not conventional UART flow control. Keeping
    # DTR asserted avoids the reset observed with both lines deasserted on macOS.
    port.dtr = True
    port.rts = False
    port.port = path
    try:
        port.open()
        # A reconnect may overlap boot. Retry the read-only handshake, never eat.
        for attempt in range(2):
            try:
                exchange(port, "PET2 STATUS", "STATUS")
                return port
            except TimeoutError:
                if attempt:
                    raise
    except BaseException:
        port.close()
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="explicit Passport serial port (no automatic device selection)")
    parser.add_argument("--csv", help="explicit Kaboo CSV export; default runs local kaboo-cli export")
    parser.add_argument("--kaboo-cli", default="kaboo-cli")
    parser.add_argument("--goal", type=int, help="monthly daily target; otherwise median of 14 active days")
    parser.add_argument("--preview", action="store_true", help="print aggregates only; do not connect to device")
    parser.add_argument("--watch", action="store_true", help="keep syncing in this process, Ctrl-C to stop")
    parser.add_argument("--interval", type=int, default=300)
    args = parser.parse_args()
    if not args.preview and not args.port:
        parser.error("--port is required for device writes")
    if args.interval < 60:
        parser.error("--interval must be at least 60 seconds")
    port = None
    try:
        while True:
            try:
                # Keep one connection across watch cycles; repeated opens can
                # toggle USB reset lines and interrupt the pet screen.
                if not args.preview and args.watch and port is None:
                    port = open_port(args.port)
                sync_once(args, port)
            except (ValueError, OSError, subprocess.TimeoutExpired, TimeoutError) as exc:
                print(str(exc), flush=True)
                if isinstance(exc, (OSError, TimeoutError)) and port is not None:
                    port.close()
                    port = None
                if not args.watch:
                    raise SystemExit(1) from None
            if not args.watch:
                break
            time.sleep(args.interval)
    finally:
        if port is not None:
            port.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
