import asyncio
import contextlib
import datetime as dt
import io
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import sys
import tempfile
import threading
from types import SimpleNamespace
import unittest
import zipfile
from unittest.mock import AsyncMock, Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import pet_desktop_core as d


class PreferencesTests(unittest.TestCase):
    def test_controls_offer_stop_without_enabling_usb_cancellation(self):
        self.assertEqual(d.feeding_control(False, None, False, False)[:2], ("开始送饭", True))
        self.assertEqual(d.feeding_control(True, "feed", False, False)[:2], ("暂停送饭", True))
        self.assertEqual(d.feeding_control(True, "source", False, False)[:2], ("停止检测", True))
        self.assertFalse(d.feeding_control(True, "usb", False, False)[1])
        self.assertFalse(d.feeding_control(True, "feed", True, False)[1])
        self.assertEqual(d.feeding_control(True, "feed", False, True)[2], "等待重试")
        self.assertEqual(d.feeding_control(False, None, False, True)[2], "需要检查")

    def test_delivery_time_distinguishes_old_dates_and_invalid_data(self):
        now = dt.datetime.now().astimezone().replace(hour=12, minute=30, second=0)
        self.assertIn("12:30:00", d.delivery_caption(now.isoformat(), now))
        previous = now - dt.timedelta(days=1)
        self.assertIn(previous.strftime("%m-%d"), d.delivery_caption(previous.isoformat(), now))
        self.assertIn("非实时", d.delivery_caption(now.isoformat(), now))
        self.assertIn("尚未收到", d.delivery_caption(None))
        self.assertIn("尚未收到", d.delivery_caption("bad"))

    def test_zip_preserves_utf8_permissions_and_internal_symlinks(self):
        from build_pet_desktop import write_zip
        with tempfile.TemporaryDirectory() as home:
            root = Path(home) / "package"
            root.mkdir()
            file = root / "先读我.txt"
            file.write_text("公开说明", encoding="utf-8")
            file.chmod(0o755)
            (root / "alias").symlink_to(file.name)
            output = Path(home) / "pilot.zip"
            write_zip(root, output)
            with zipfile.ZipFile(output) as archive:
                info = archive.getinfo("package/先读我.txt")
                self.assertTrue(info.flag_bits & 0x800)
                self.assertEqual(info.external_attr >> 16 & 0o777, 0o755)
                self.assertEqual(archive.read("package/alias").decode(), file.name)
                self.assertEqual(archive.getinfo("package/alias").external_attr >> 16 & 0o170000, 0o120000)
                self.assertIsNone(archive.testzip())
            with self.assertRaises(FileExistsError):
                write_zip(root, output)
            (root / "outside").symlink_to(Path(home) / "private")
            with self.assertRaises(ValueError):
                write_zip(root, Path(home) / "unsafe.zip")
            self.assertFalse((Path(home) / "unsafe.zip").exists())

    def test_private_roundtrip_without_raw_usage(self):
        with tempfile.TemporaryDirectory() as home:
            prefs = d.Preferences(home)
            self.assertFalse(prefs.data["login"])
            prefs.remember_ack({"pending": "3", "stage": "4", "days": "7", "tokens": "private", "goal": "private"})
            other = d.Preferences(home)
            self.assertEqual(other.data["last"]["pending"], "3")
            self.assertNotIn("private", prefs.path.read_text())
            self.assertEqual(prefs.path.stat().st_mode & 0o777, 0o600)

    def test_invalid_future_schema_and_permissions_are_not_overwritten(self):
        for payload in ("bad", "[]", '{"schema": 999, "login": false}'):
            with tempfile.TemporaryDirectory() as home:
                path = Path(home) / "desktop.json"
                path.write_text(payload)
                path.chmod(0o600)
                with self.assertRaises(d.UserProblem):
                    d.Preferences(home)
                self.assertEqual(path.read_text(), payload)
        with tempfile.TemporaryDirectory() as home:
            path = Path(home) / "desktop.json"
            path.symlink_to(Path(home) / "missing")
            with self.assertRaises(d.UserProblem):
                d.Preferences(home)

    def test_bad_permissions_and_atomic_failure_leave_old_file(self):
        with tempfile.TemporaryDirectory() as home:
            prefs = d.Preferences(home)
            prefs.save(kaboo="original")
            with patch.object(d.os, "replace", side_effect=OSError()):
                with self.assertRaises(OSError):
                    prefs.save(kaboo="changed")
            self.assertEqual(prefs.data["kaboo"], "original")
            self.assertEqual(len(list(Path(home).iterdir())), 1)
            prefs.path.chmod(0o644)
            with self.assertRaises(d.UserProblem):
                d.Preferences(home)

    def test_stage_is_not_a_level_number(self):
        self.assertEqual(d.stage_name("0"), "数码蛋")
        self.assertEqual(d.stage_name("6"), "究极体")
        self.assertEqual(d.stage_name("-1"), "—")

    def test_login_never_replaces_unknown_or_symlink(self):
        with tempfile.TemporaryDirectory() as home:
            path = d.login_path(home)
            path.write_bytes(plistlib.dumps({"Label": "someone.else"}))
            with self.assertRaises(d.UserProblem):
                d.set_login(False, "/unused", home)
            self.assertTrue(path.exists())
            path.unlink()
            path.symlink_to(Path(home) / "missing")
            with self.assertRaises(d.UserProblem):
                d.set_login(False, "/unused", home)

    def test_login_opt_in_and_removal_only_own_item(self):
        with tempfile.TemporaryDirectory() as home:
            root = Path(home)
            executable = root / "Applications/AI Pet Passport.app/Contents/MacOS/AI Pet Passport"
            executable.parent.mkdir(parents=True)
            executable.touch()
            agents = root / "agents"
            with patch.object(d.Path, "home", return_value=root):
                d.set_login(True, executable, agents)
                self.assertEqual(plistlib.loads(d.login_path(agents).read_bytes()), d.login_payload(executable))
                d.set_login(False, executable, agents)
                self.assertFalse(d.login_path(agents).exists())
            with self.assertRaises(d.UserProblem):
                d.set_login(True, "/tmp/uninstalled", agents)

    def test_failed_login_change_rolls_back_preferences(self):
        with tempfile.TemporaryDirectory() as home:
            prefs = d.Preferences(home)
            with patch.object(d, "set_login", side_effect=d.UserProblem("failed")):
                with self.assertRaises(d.UserProblem):
                    d.configure_login(prefs, True, "/not-installed")
            self.assertFalse(d.Preferences(home).data["login"])

    def test_export_env_does_not_require_shell_profile(self):
        with patch.dict(os.environ, {"PATH": "/usr/bin", "PYTHONHOME": "bad", "PYTHONPATH": "bad"}):
            env = d.export_env("/own/node/bin/kaboo-cli", "/temporary/cache")
        self.assertEqual(env["PATH"], "/own/node/bin:/usr/bin")
        self.assertEqual(env["KABOO_SKIP_AUTO_UPDATE"], "1")
        self.assertNotIn("PYTHONHOME", env)
        self.assertNotIn("PYTHONPATH", env)

    def test_worker_stops_async_job_and_serializes_jobs(self):
        began = threading.Event()
        events = []
        worker = d.Worker(lambda *event: events.append(event))
        async def job():
            began.set()
            try:
                await asyncio.sleep(20)
            finally:
                events.append(("cleaned", None))
        worker.start("source", lambda: worker.run_async(job))
        self.assertTrue(began.wait(2))
        self.assertFalse(worker.start("feed", lambda: None))
        self.assertTrue(worker.stop())
        worker.thread.join(3)
        self.assertFalse(worker.busy)
        self.assertEqual(events, [("cleaned", None), ("finished", None)])

    def test_usb_is_not_cancellable_and_unknown_errors_are_redacted(self):
        worker = d.Worker(Mock())
        worker.kind = "usb"
        self.assertFalse(worker.stop())
        self.assertNotIn("private-path", d.user_error(RuntimeError("private-path")))
        self.assertIn("恢复区", d.user_error(ValueError("Recovery failed")))


class AsyncTests(unittest.IsolatedAsyncioTestCase):
    async def test_export_checks_today_and_does_not_persist_data(self):
        today = dt.datetime.now(d.c.ZONE).date()
        csv = "bucketStart,totalTokens,source,model,project,hostname\n" + f"{today}T12:00:00+08:00,123,k,m,p,h\n"
        with tempfile.TemporaryDirectory() as home:
            path = Path(home) / "kaboo-cli"
            path.write_text("#!/bin/sh\nprintf '%s' '" + csv + "'\n")
            path.chmod(0o700)
            self.assertEqual(await d.load_kaboo(str(path)), {today: 123})
            self.assertEqual(len(list(Path(home).iterdir())), 1)
            path.write_text("#!/bin/sh\nprintf '%s' 'bucketStart,totalTokens,source,model,project,hostname\n'\n")
            with self.assertRaisesRegex(d.UserProblem, "今日"):
                await d.load_kaboo(str(path))

    async def test_cancel_export_terminates_only_its_child(self):
        with tempfile.TemporaryDirectory() as home:
            path = Path(home) / "kaboo-cli"
            path.write_text("#!/bin/sh\nexec sleep 30\n")
            path.chmod(0o700)
            with patch.object(d.os, "killpg", wraps=os.killpg) as kill:
                with self.assertRaises(asyncio.TimeoutError):
                    await d.load_kaboo(str(path), timeout=0.05)
                self.assertEqual(kill.call_count, 1)
                self.assertGreater(kill.call_args.args[0], 0)

    async def test_export_bounds_and_bad_exit(self):
        stream = asyncio.StreamReader()
        stream.feed_data(b"12345")
        stream.feed_eof()
        with patch.object(d, "MAX_EXPORT", 4):
            with self.assertRaises(d.UserProblem):
                await d._bounded_stdout(stream)
        with self.assertRaises(d.UserProblem):
            await d.load_kaboo("not-an-absolute-file")
        with self.assertRaises(d.UserProblem):
            await d.load_kaboo("/usr/bin/false")

    @unittest.skipUnless(importlib.util.find_spec("cryptography"), "requires desktop runtime dependencies")
    async def test_feed_only_reports_authenticated_ack_and_no_monthly(self):
        import pet_wireless as w
        events = []
        worker = d.Worker(lambda *event: events.append(event))
        with patch.object(d, "load_kaboo", AsyncMock(return_value={"test": 1})), \
                patch.object(w, "load_key", return_value=bytes(32)), \
                patch.object(w, "sync_once", AsyncMock(return_value={"pending": "2"})) as sync:
            await worker.feed("/unused", "/unused", once=True)
            self.assertIn(("ack", {"pending": "2"}), events)
            self.assertTrue(sync.call_args.kwargs["require_current_day"])
            self.assertEqual(vars(sync.call_args.args[0]), {"goal": None})
            events.clear()
            sync.side_effect = TimeoutError()
            await worker.feed("/unused", "/unused", once=True)
            self.assertNotIn("ack", [kind for kind, _ in events])

    @unittest.skipUnless(importlib.util.find_spec("cryptography"), "requires desktop runtime dependencies")
    async def test_wireless_supplied_totals_progress_quiet_and_disconnect(self):
        import pet_wireless as w
        today = dt.datetime.now(d.c.ZONE).date()
        client, phases = AsyncMock(), []
        with patch.object(w.c, "load_source") as source, \
                patch.object(w, "connect_device", AsyncMock(return_value=(client, bytes(16)))), \
                patch.object(w, "exchange", AsyncMock(side_effect=[{"date": "0"}, {"pending": "1"}])):
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = await w.sync_once(SimpleNamespace(goal=None), bytes(32), totals={today: 1},
                                           require_current_day=True, quiet=True, progress=phases.append)
            self.assertEqual(result, {"pending": "1"})
            source.assert_not_called()
            self.assertEqual(output.getvalue(), "")
            self.assertEqual(phases, ["discovering", "sending"])
            client.disconnect.assert_awaited_once()


if __name__ == "__main__":
    unittest.main()
