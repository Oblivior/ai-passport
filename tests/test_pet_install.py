from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import pet_install as p
from test_verify_firmware import sample_table


def flash_fixture():
    flash = bytearray(b"\xff" * p.FLASH_SIZE)
    flash[:len(p.RECOVERY_BOOT_MARKER)] = p.RECOVERY_BOOT_MARKER
    flash[0x8000:0x8C00] = sample_table()
    flash[0x356000] = 1
    flash[0x700000] = 0xE9
    return bytes(flash)


class FakeDevice:
    def __init__(self, flash=None):
        self.flash = flash_fixture() if flash is None else flash
        self.device_id = "public-test-id"
        self.writes = []
        self.reset = Mock()

    def read(self, offset, size):
        return self.flash[offset:offset + size]

    def write_app(self, path):
        data = path.read_bytes()
        self.writes.append(data)
        self.flash = self.flash[:p.APP_OFFSET] + data + self.flash[p.APP_OFFSET + len(data):]


class InstallTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.home = Path(self.temp.name)
        self.output = redirect_stdout(io.StringIO())
        self.output.__enter__()

    def tearDown(self):
        self.output.__exit__(None, None, None)
        self.temp.cleanup()

    def run_install(self, device, write=True, answer="INSTALL"):
        return p.install(device, sample_table(), b"new-app", self.home,
                         confirm=lambda _: answer, write=write)

    def test_valid_flash_preserves_and_verifies(self):
        device = FakeDevice()
        before = device.flash
        backup = self.run_install(device)
        self.assertEqual(device.writes, [b"new-app"])
        self.assertEqual((backup / "flash-before.bin").read_bytes(), before)
        self.assertTrue(json.loads((backup / "verified.json").read_text())["protected_regions_equal"])
        self.assertEqual((backup / "flash-before.bin").stat().st_mode & 0o777, 0o600)
        self.assertEqual(backup.stat().st_mode & 0o777, 0o700)
        device.reset.assert_called_once()

    def test_check_only_and_cancel_never_write(self):
        for write, answer in ((False, "INSTALL"), (True, "yes")):
            device = FakeDevice()
            backup = self.run_install(device, write=write, answer=answer)
            self.assertFalse(device.writes)
            self.assertFalse((backup / "new-app.bin").exists())
            device.reset.assert_called_once()

    def test_incomplete_backup_never_writes(self):
        device = FakeDevice(b"short")
        with self.assertRaisesRegex(ValueError, "8 MiB"):
            self.run_install(device)
        self.assertFalse(device.writes)

    def test_bad_partition_md5_never_writes(self):
        flash = bytearray(flash_fixture())
        flash[0x8000 + 28] ^= 1
        device = FakeDevice(bytes(flash))
        with self.assertRaises(ValueError):
            self.run_install(device)
        self.assertFalse(device.writes)

    def test_missing_identity_recovery_or_hook_never_writes(self):
        for start, end in ((0x356000, 0x35A000), (0x700000, 0x800000), (0, 100)):
            flash = bytearray(flash_fixture())
            flash[start:end] = b"\xff" * (end - start)
            device = FakeDevice(bytes(flash))
            with self.assertRaises(ValueError):
                self.run_install(device)
            self.assertFalse(device.writes)

    def test_inconsistent_second_read_never_writes(self):
        device = FakeDevice()
        device.read = Mock(side_effect=[device.flash, b"changed"])
        with self.assertRaisesRegex(ValueError, "两次"):
            self.run_install(device)
        self.assertFalse(device.writes)

    def test_changed_protected_region_fails_without_reset(self):
        for offset in (0, 0x9000, 0x356000, 0x700000, p.APP_OFFSET):
            device = FakeDevice()
            original = device.write_app
            def corrupt(path):
                original(path)
                bad = bytearray(device.flash)
                bad[offset] ^= 1
                device.flash = bytes(bad)
            device.write_app = corrupt
            with self.assertRaisesRegex(ValueError, "刷后校验失败"):
                self.run_install(device)
            device.reset.assert_not_called()

    def test_disconnect_is_not_retried(self):
        device = FakeDevice()
        device.write_app = Mock(side_effect=OSError("disconnected"))
        with self.assertRaises(OSError):
            self.run_install(device)
        device.write_app.assert_called_once()
        device.reset.assert_not_called()

    def test_exclusive_lock_and_stale_file(self):
        with p.exclusive(self.home):
            with self.assertRaises(ValueError):
                with p.exclusive(self.home):
                    self.fail("second owner")
        with p.exclusive(self.home):
            pass

    def test_private_file_does_not_overwrite(self):
        path = self.home / "private"
        p.save_private(path, b"first")
        with self.assertRaises(FileExistsError):
            p.save_private(path, b"second")
        self.assertEqual(path.read_bytes(), b"first")

    def test_private_directory_rejects_symlink(self):
        path = self.home / "link"
        path.symlink_to(self.home, target_is_directory=True)
        with self.assertRaises(ValueError):
            p.private_dir(path)

    def test_bootstrap_cancel_and_failure_do_not_mark_ready(self):
        import pet_bootstrap as b
        import subprocess
        with patch.object(b.subprocess, "run") as run:
            self.assertIsNone(b.setup(self.home, confirm=lambda _: "no"))
            run.assert_not_called()
        with patch.object(b.subprocess, "run", side_effect=subprocess.CalledProcessError(1, "test")):
            with self.assertRaises(subprocess.CalledProcessError):
                b.setup(self.home, confirm=lambda _: "YES")
        self.assertFalse((self.home / "runtime-v1/.ready").exists())

    def test_bootstrap_reuses_ready_runtime_and_obeys_lock(self):
        import pet_bootstrap as b
        runtime = p.private_dir(self.home / "runtime-v1")
        (runtime / "bin").mkdir()
        (runtime / "bin/python").touch()
        (runtime / ".ready").write_bytes(Path(b.__file__).with_name("requirements-pet-installer.txt").read_bytes())
        with patch.object(b.subprocess, "run") as run:
            self.assertEqual(b.setup(self.home), runtime / "bin/python")
            run.assert_not_called()
            with p.exclusive(self.home), self.assertRaises(ValueError):
                b.setup(self.home)

    def test_bundle_manifest_corruption_rejected(self):
        (self.home / "manifest.json").write_text(json.dumps({"schema": 1, "chip": "esp32c3", "firmware_sha256": "bad"}))
        (self.home / "FoloToy-AI-Passport-full.bin").write_bytes(b"not firmware")
        with self.assertRaisesRegex(ValueError, "摘要"):
            p.load_bundle(self.home)

    def test_kaboo_missing_today_empty_future_and_valid(self):
        import pet_companion as c
        today = p.dt.datetime.now(c.ZONE).date()
        with patch.object(p.shutil, "which", return_value="/public/kaboo"):
            for totals in ({}, {today - p.dt.timedelta(days=1): 1}, {today + p.dt.timedelta(days=1): 1}):
                with patch.object(c, "load_source", return_value=totals), self.assertRaises(ValueError):
                    p.check_kaboo("kaboo")
            with patch.object(c, "load_source", return_value={today: 1}) as source:
                self.assertEqual(p.check_kaboo("kaboo"), "/public/kaboo")
                self.assertFalse(source.call_args.args[0].cached_export)
                self.assertIsNone(source.call_args.args[0].settlement_provider)


try:
    import serial
    import pet_wireless as w
    HAVE_WIRELESS = True
except ImportError:
    HAVE_WIRELESS = False


@unittest.skipUnless(HAVE_WIRELESS, "optional installer dependencies")
class PairingTests(unittest.TestCase):
    def test_multiple_devices_refused(self):
        from serial.tools import list_ports
        device = SimpleNamespace(vid=0x303A, pid=0x1001, device="/dev/cu.test")
        for devices in ([], [device, device]):
            with patch.object(list_ports, "comports", return_value=devices), self.assertRaises(ValueError):
                p.choose_port()

    def test_different_serial_numbers_get_different_profiles(self):
        from serial.tools import list_ports
        with tempfile.TemporaryDirectory() as directory:
            configs = []
            for number in ("test-a", "test-b"):
                with patch.object(list_ports, "comports", return_value=[SimpleNamespace(device="port", serial_number=number)]):
                    configs.append(p.device_config(directory, "port"))
            self.assertNotEqual(*configs)

    def test_existing_pair_without_key_never_provisions(self):
        import pet_companion as c
        with tempfile.TemporaryDirectory() as directory, \
                patch.object(c, "open_port", return_value=Mock()), \
                patch.object(c, "exchange", side_effect=[{"version": "5", "storage": "1"}, {"paired": "1"}]) as exchange, \
                patch.object(w, "load_key") as key:
            with self.assertRaisesRegex(ValueError, "已配对"):
                p.pair("port", Path(directory) / "link.json")
            key.assert_not_called()
            self.assertEqual(exchange.call_count, 2)

    def test_storage_unhealthy_never_provisions(self):
        import pet_companion as c
        with tempfile.TemporaryDirectory() as directory, \
                patch.object(c, "open_port", return_value=Mock()), \
                patch.object(c, "exchange", return_value={"version": "5", "storage": "0"}), \
                patch.object(w, "load_key") as key:
            with self.assertRaises(ValueError):
                p.pair("port", Path(directory) / "link.json")
            key.assert_not_called()

    def test_usb_pair_requires_matching_ack_and_preserves_key_on_failure(self):
        import pet_companion as c
        with tempfile.TemporaryDirectory() as directory, \
                patch.object(c, "open_port", return_value=Mock()), \
                patch.object(c, "exchange", side_effect=[{"version": "5", "storage": "1"}, {"paired": "0"}, {"id": "wrong"}]):
            config = Path(directory) / "link.json"
            with self.assertRaises(ValueError):
                p.pair("port", config, lambda _: "PAIR")
            self.assertTrue(config.exists())


try:
    import esptool
    HAVE_ESPTOOL = True
except ImportError:
    HAVE_ESPTOOL = False


@unittest.skipUnless(HAVE_ESPTOOL, "optional esptool dependency")
class PackageTests(unittest.TestCase):
    def setUp(self):
        from esptool.bin_image import ESP32C3FirmwareImage, ImageSegment
        self.temp = tempfile.TemporaryDirectory()
        self.home = Path(self.temp.name)
        image = ESP32C3FirmwareImage()
        image.chip_id = 5
        image.segments = [ImageSegment(0x3FC80000, b"public-test-data")]
        image.segments[0].name = ".data"
        image.save(self.home / "app.bin")
        self.app = (self.home / "app.bin").read_bytes()

    def tearDown(self):
        self.temp.cleanup()

    def test_image_digest_chip_and_trailing_bytes(self):
        self.assertEqual(p.image_bytes(self.app + b"\xff" * 32), self.app)
        for data in (self.app[:-1], self.app + b"unknown", self.app[:-1] + bytes([self.app[-1] ^ 1])):
            with self.assertRaises(ValueError):
                p.image_bytes(data)

    def test_package_whitelist_permissions_and_roundtrip(self):
        import package_pet_mac as pack
        merged = flash_fixture()[:0x10000] + self.app
        source = self.home / "full.bin"
        source.write_bytes(merged)
        output = self.home / "pilot.zip"
        with redirect_stdout(io.StringIO()):
            pack.package(source, "test-pilot", output)
        with zipfile.ZipFile(output) as archive:
            self.assertIsNone(archive.testzip())
            prefix = "AI-Pet-Passport-Mac-test-pilot/"
            names = {n.removeprefix(prefix) for n in archive.namelist()}
            expected = set(pack.FILES) | {"开始使用.command", "先读我.txt", "firmware/manifest.json",
                        "firmware/FoloToy-AI-Passport-full.bin", "SHA256SUMS.json"}
            self.assertEqual(names, expected)
            hashes = json.loads(archive.read(prefix + "SHA256SUMS.json"))
            for name, checksum in hashes.items():
                self.assertEqual(p.digest(archive.read(prefix + name)), checksum)
            launcher = archive.getinfo(prefix + "开始使用.command")
            self.assertEqual((launcher.external_attr >> 16) & 0o777, 0o755)
            self.assertTrue(launcher.flag_bits & 0x800)
            archive.extractall(self.home / "unpacked")
        manifest, _, app = p.load_bundle(self.home / "unpacked" / prefix / "firmware")
        self.assertEqual(app, self.app)
        self.assertEqual(manifest["firmware_version"], "test-pilot")
        with self.assertRaises(FileExistsError):
            pack.package(source, "test-pilot", output)

    def test_backend_refuses_wrong_chip_security_and_flash_size(self):
        for chip, secure, encryption, size in (("ESP32", False, False, "8MB"),
                                               ("ESP32-C3", True, False, "8MB"),
                                               ("ESP32-C3", False, True, "8MB"),
                                               ("ESP32-C3", False, False, "4MB")):
            esp = Mock(CHIP_NAME=chip, secure_download_mode=False)
            esp.get_secure_boot_enabled.return_value = secure
            esp.get_flash_encryption_enabled.return_value = encryption
            esp.read_mac.return_value = (0, 1, 2, 3, 4, 5)
            esp.run_stub.return_value = esp
            with patch.object(esptool, "detect_chip", return_value=esp), \
                    patch.object(esptool.cmds, "detect_flash_size", return_value=size), self.assertRaises(ValueError):
                p.EspDevice("port")
            esp._port.close.assert_called_once()

    def test_backend_never_uses_full_erase_force_or_other_offsets(self):
        esp = Mock(CHIP_NAME="ESP32-C3", secure_download_mode=False)
        esp.get_secure_boot_enabled.return_value = False
        esp.get_flash_encryption_enabled.return_value = False
        esp.read_mac.return_value = (0, 1, 2, 3, 4, 5)
        esp.run_stub.return_value = esp
        with patch.object(esptool, "detect_chip", return_value=esp), \
                patch.object(esptool.cmds, "detect_flash_size", return_value="8MB"), \
                patch.object(esptool.cmds, "write_flash") as write:
            device = p.EspDevice("port")
            self.assertEqual(esp.WRITE_FLASH_ATTEMPTS, 1)
            device.write_app(self.home / "app.bin")
            args = write.call_args.args[1]
            self.assertEqual(len(args.addr_filename), 1)
            self.assertEqual(args.addr_filename[0][0], 0x10000)
            self.assertFalse(args.force)
            self.assertFalse(args.erase_all)
            self.assertEqual(args.flash_size, "keep")
            device.close()


if __name__ == "__main__":
    unittest.main()
