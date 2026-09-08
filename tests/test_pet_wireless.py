import asyncio
import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import AsyncMock, Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import pet_wireless as w
from cryptography.exceptions import InvalidTag


class WirelessTests(unittest.TestCase):
    def setUp(self):
        # Public test inputs, not pairing credentials.
        self.key = bytes(range(32))
        self.challenge = bytes(range(16))

    def test_roundtrip_and_unique_nonces(self):
        first = w.seal(self.key, self.challenge, 1, "PET2 STATUS", b"PET3-S")
        second = w.seal(self.key, self.challenge, 1, "PET2 STATUS", b"PET3-S")
        self.assertNotEqual(first[:12], second[:12])
        self.assertEqual(w.unseal(self.key, self.challenge, 1, first), "PET2 STATUS")

    def test_wrong_key_challenge_tampering_and_direction(self):
        frame = w.seal(self.key, self.challenge, 1, "PET2 STATUS", b"PET3-S")
        for key, challenge, packet, direction in (
                (bytes(32), self.challenge, frame, b"PET3-S"),
                (self.key, bytes(16), frame, b"PET3-S"),
                (self.key, self.challenge, frame[:-1] + bytes([frame[-1] ^ 1]), b"PET3-S"),
                (self.key, self.challenge, frame, b"PET3-C")):
            with self.assertRaises(InvalidTag):
                w.unseal(key, challenge, 1, packet, direction)

    def test_stale_sequence_and_bounds(self):
        frame = w.seal(self.key, self.challenge, 1, "PET2 STATUS", b"PET3-S")
        with self.assertRaises(ValueError):
            w.unseal(self.key, self.challenge, 2, frame)
        for size in (0, 160):
            with self.assertRaises(ValueError):
                w.seal(self.key, self.challenge, 1, "x" * size)
        frame = w.seal(self.key, self.challenge, 1, "x" * 159, b"PET3-S")
        self.assertEqual(w.unseal(self.key, self.challenge, 1, frame), "x" * 159)

    def test_private_pairing_file_is_reused(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "link.json"
            first = w.load_key(path, create=True)
            self.assertEqual(w.load_key(path, create=True), first)
            self.assertEqual(os.stat(path).st_mode & 0o777, 0o600)
            os.chmod(path, 0o644)
            with self.assertRaises(ValueError):
                w.load_key(path)

    def test_no_silent_overwrite_of_invalid_pairing(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "link.json"
            path.write_text("invalid")
            os.chmod(path, 0o600)
            with self.assertRaises(ValueError):
                w.load_key(path, create=True)
            self.assertEqual(path.read_text(), "invalid")


class ExchangeTests(unittest.IsolatedAsyncioTestCase):
    async def test_installer_rejects_missing_current_day_before_radio(self):
        from types import SimpleNamespace
        import datetime as dt
        today = dt.datetime.now(w.c.ZONE).date()
        for totals in ({}, {today - dt.timedelta(days=1): 1}, {today: 1, today + dt.timedelta(days=1): 1}):
            with patch.object(w.c, "load_source", return_value=totals), \
                    patch.object(w, "connect_device", AsyncMock()) as connect:
                with self.assertRaisesRegex(ValueError, "current-day"):
                    await w.sync_once(SimpleNamespace(), bytes(32), require_current_day=True)
                connect.assert_not_called()

    async def test_monthly_retry_is_throttled_without_stopping_lunch(self):
        import pet_settlement as s
        from types import SimpleNamespace
        args = SimpleNamespace(watch=True, interval=60, settlement_provider="/private/provider")
        with patch.object(w, "sync_once", AsyncMock()) as lunch, \
                patch.object(s, "reconcile", AsyncMock(side_effect=ValueError("source unavailable"))) as monthly, \
                patch.object(w.asyncio, "sleep", AsyncMock(side_effect=[None, asyncio.CancelledError()])):
            with self.assertRaises(asyncio.CancelledError):
                await w.watch(args, bytes(32))
        self.assertEqual(lunch.await_count, 2)
        self.assertEqual(monthly.await_count, 1)

    async def test_connection_timeout_retries_with_bounded_backoff(self):
        result = (Mock(), bytes(16))
        with patch.object(w, "_connect_once", AsyncMock(side_effect=[TimeoutError(), asyncio.TimeoutError(), result])) as connect, \
                patch.object(w.asyncio, "sleep", AsyncMock()) as sleep:
            self.assertIs(await w.connect_device(bytes(32)), result)
        self.assertEqual(connect.await_count, 3)
        self.assertEqual([call.args[0] for call in sleep.await_args_list], [2, 4])

    async def test_connection_retries_stop_after_three_timeouts(self):
        with patch.object(w, "_connect_once", AsyncMock(side_effect=TimeoutError())) as connect, \
                patch.object(w.asyncio, "sleep", AsyncMock()):
            with self.assertRaises(TimeoutError):
                await w.connect_device(bytes(32))
        self.assertEqual(connect.await_count, 3)

    async def test_connection_identity_errors_are_not_retried(self):
        with patch.object(w, "_connect_once", AsyncMock(side_effect=ValueError("identity mismatch"))) as connect:
            with self.assertRaises(ValueError):
                await w.connect_device(bytes(32))
        self.assertEqual(connect.await_count, 1)

    async def test_watch_survives_async_timeout_on_python39(self):
        from types import SimpleNamespace
        args = SimpleNamespace(watch=True, interval=300)
        with patch.object(w, "sync_once", AsyncMock(side_effect=asyncio.TimeoutError())) as sync, \
                patch.object(w.asyncio, "sleep", AsyncMock(side_effect=asyncio.CancelledError())):
            with self.assertRaises(asyncio.CancelledError):
                await w.watch(args, bytes(32))
        self.assertEqual(sync.await_count, 1)

    async def test_cold_discovery_matches_name_and_service_locally(self):
        key = bytes(range(32))
        client = AsyncMock()
        client.read_gatt_char.return_value = bytes([3]) + w.key_id(key).encode() + bytes(16)
        scanner = Mock()
        scanner.find_device_by_filter = AsyncMock(return_value=Mock())
        with patch.dict(sys.modules, {"bleak": Mock(BleakClient=Mock(return_value=client), BleakScanner=scanner)}):
            connected, challenge = await w.connect_device(key)
        self.assertIs(connected, client)
        call = scanner.find_device_by_filter.call_args
        self.assertNotIn("service_uuids", call.kwargs)
        predicate = call.args[0]
        name = "AIPet-" + w.key_id(key)
        self.assertTrue(predicate(Mock(name=name), Mock(local_name=name, service_uuids=[w.SERVICE])))
        self.assertFalse(predicate(Mock(), Mock(local_name="other", service_uuids=[w.SERVICE])))
        self.assertFalse(predicate(Mock(), Mock(local_name=name, service_uuids=[])))

    async def test_requires_authenticated_application_ack(self):
        key, challenge = bytes(range(32)), bytes(range(16))
        response = w.seal(key, challenge, 1, "PET2 ACK pending=2", b"PET3-S")
        client = AsyncMock()
        client.read_gatt_char.side_effect = [b"", response]
        self.assertEqual(await w.exchange(client, key, challenge, 1, "PET2 STATUS", "ACK"), {"pending": "2"})
        self.assertTrue(client.write_gatt_char.call_args.kwargs["response"])

    async def test_plaintext_success_is_rejected(self):
        client = AsyncMock()
        client.read_gatt_char.return_value = b"PET2 ACK pending=2"
        with self.assertRaises(ValueError):
            await w.exchange(client, bytes(32), bytes(16), 1, "PET2 STATUS", "ACK")


if __name__ == "__main__":
    unittest.main()
