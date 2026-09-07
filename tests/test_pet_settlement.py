import datetime as dt
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import AsyncMock, Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import pet_settlement as s


class ProviderTests(unittest.TestCase):
    def setUp(self):
        self.today = dt.date(2026, 10, 7)
        self.record = {"schemaVersion": 1, "scope": "personal", "month": 202609,
                       "complete": True, "tokens": 1000, "coveredThrough": "2026-09-30"}

    def test_complete_zero_and_leap_month(self):
        self.assertEqual(s.normalize(self.record, 202609, self.today), "PET2 SETTLE 202609 1000 20260930")
        self.record["tokens"] = 0
        self.assertIn(" 0 ", s.normalize(self.record, 202609, self.today))
        self.record.update(month=202402, coveredThrough="2024-02-28")
        self.assertIsNone(s.normalize(self.record, 202402, self.today))
        self.record["coveredThrough"] = "2024-02-29"
        self.assertIsNotNone(s.normalize(self.record, 202402, self.today))

    def test_missing_is_not_zero(self):
        for value in (None, "NA", "1000", 1.5, True, -1, 9007199254740992):
            with self.subTest(value=value), self.assertRaises(ValueError):
                s.normalize(dict(self.record, tokens=value), 202609, self.today)
        self.assertIsNone(s.normalize(dict(self.record, complete=False), 202609, self.today))
        self.assertIsNone(s.normalize(dict(self.record, coveredThrough="2026-09-29"), 202609, self.today))

    def test_wrong_scope_month_watermark_schema(self):
        for changes in ({"scope": "team"}, {"month": 202608}, {"month": "202609"},
                        {"schemaVersion": True}, {"complete": 1}, {"coveredThrough": "2026-10-08"},
                        {"coveredThrough": "2026-09-31"}, {"coveredThrough": None}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                s.normalize(dict(self.record, **changes), 202609, self.today)
        with self.assertRaises(ValueError):
            s.normalize(dict(self.record, month=202610), 202610, self.today)

    def test_archive_list(self):
        self.assertEqual(s.archive_months({"months": "none"}), [])
        self.assertEqual(s.archive_months({"months": "202608,202609"}), [202608, 202609])
        for value in (None, "", "202613", "202609,202609", "202609;ls", "２０２６０９"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                s.archive_months({"months": value})

    def test_provider_uses_no_shell_and_hides_error_body(self):
        with patch.object(s.subprocess, "run", return_value=Mock(returncode=0, stdout=json.dumps(self.record))) as run:
            line = s.load_provider(sys.executable, 202609, self.today)
        self.assertIsNotNone(line)
        self.assertEqual(run.call_args.args[0], [sys.executable, "202609"])
        self.assertNotIn("shell", run.call_args.kwargs)
        with patch.object(s.subprocess, "run", return_value=Mock(returncode=1, stdout="private", stderr="secret")):
            with self.assertRaisesRegex(ValueError, "monthly provider failed"):
                s.load_provider(sys.executable, 202609, self.today)


class DeliveryTests(unittest.IsolatedAsyncioTestCase):
    async def test_fetch_disconnected_then_require_matching_ack(self):
        w = Mock()  # Transport is mocked; cryptographic tests live in test_pet_wireless.py.
        client = AsyncMock()
        def provider(*args):
            self.assertEqual(client.disconnect.await_count, 1)
            return "PET2 SETTLE 202608 123 20260831"
        with patch.dict(sys.modules, {"pet_wireless": w}), patch.object(w, "connect_device", AsyncMock(return_value=(client, bytes(16)))), \
                patch.object(w, "exchange", AsyncMock(side_effect=[{"months": "202608"}, {"month": "202608"}])), \
                patch.object(s, "load_provider", side_effect=provider):
            self.assertEqual(await s.reconcile("/private/provider", bytes(32)), {"monthly_settled": [202608], "monthly_pending": []})
        self.assertEqual(client.disconnect.await_count, 2)

    async def test_missing_data_never_sends_settle(self):
        w = Mock()
        client = AsyncMock()
        with patch.dict(sys.modules, {"pet_wireless": w}), patch.object(w, "connect_device", AsyncMock(return_value=(client, bytes(16)))), \
                patch.object(w, "exchange", AsyncMock(return_value={"months": "202608"})) as exchange, \
                patch.object(s, "load_provider", return_value=None):
            result = await s.reconcile("/private/provider", bytes(32))
        self.assertEqual(exchange.await_count, 1)
        self.assertEqual(result["monthly_pending"], [202608])

    async def test_mismatched_receipt_never_reports_success(self):
        w = Mock()
        client = AsyncMock()
        with patch.dict(sys.modules, {"pet_wireless": w}), patch.object(w, "connect_device", AsyncMock(return_value=(client, bytes(16)))), \
                patch.object(w, "exchange", AsyncMock(side_effect=[{"months": "202608"}, {"month": "202607"}])), \
                patch.object(s, "load_provider", return_value="PET2 SETTLE 202608 123 20260831"):
            with self.assertRaisesRegex(ValueError, "receipt month mismatch"):
                await s.reconcile("/private/provider", bytes(32))
        self.assertEqual(client.disconnect.await_count, 2)


if __name__ == "__main__":
    unittest.main()
