#!/usr/bin/env python3
"""Conservative macOS pilot installer; no erase-all, cloud or automatic restore."""
from __future__ import annotations

import argparse
import asyncio
from contextlib import contextmanager
import datetime as dt
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import shutil
import sys
import tempfile
from types import SimpleNamespace

from verify_firmware import parse_partition_table, RECOVERY_BOOT_MARKER

FLASH_SIZE = 0x800000
APP_OFFSET = 0x10000
APP_LIMIT = 0x300000
DATA_HOME = Path.home() / "Library/Application Support/AI Pet Passport"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def private_dir(path):
    path = Path(path).expanduser()
    path.mkdir(parents=True, exist_ok=True, mode=0o700)
    if path.is_symlink() or not path.is_dir():
        raise ValueError("私有目录不能是软链接")
    if path.stat().st_mode & 0o077:
        raise ValueError("私有目录权限过宽，请由本人设为 700：" + str(path))
    return path


def save_private(path, data):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "wb") as out:
        out.write(data)
        out.flush()
        os.fsync(out.fileno())


@contextmanager
def exclusive(home):
    import fcntl
    home = private_dir(home)
    fd = os.open(home / "installer.lock", os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
    try:
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise ValueError("另一个安装／同步向导正在运行，请先关闭它") from None
        yield
    finally:
        os.close(fd)


def validate_table(table):
    parts, md5 = parse_partition_table(table)
    actual = [(p.kind, p.subtype, p.offset, p.size, p.label) for p in parts]
    expected = [(1, 2, 0x9000, 0x6000, "nvs"), (1, 1, 0xF000, 0x1000, "phy_init"),
                (0, 0, APP_OFFSET, APP_LIMIT, "factory"),
                (1, 2, 0x356000, 0x4000, "cardid"),
                (0, 0x20, 0x700000, 0x100000, "recovery")]
    if not md5 or actual != expected:
        raise ValueError("分区不兼容：此内测包只支持已验证的五分区布局；不会改分区或擦除设备")
    # This pilot supports no encrypted partitions or unknown table flags.
    if any(int.from_bytes(table[i * 32 + 28:i * 32 + 32], "little") for i in range(len(parts))):
        raise ValueError("不支持带标志位的分区")


def image_bytes(raw):
    from esptool.bin_image import ESP32C3FirmwareImage
    try:
        image = ESP32C3FirmwareImage(io.BytesIO(raw))
        if (image.chip_id != 5 or not image.append_digest or
                image.stored_digest != image.calc_digest or image.checksum != image.calculate_checksum()):
            raise ValueError("芯片、校验和或镜像摘要不匹配")
        size = image.data_length + 32
        if size > len(raw) or any(x != 0xFF for x in raw[size:]):
            raise ValueError("应用后包含未知数据／资源")
        return raw[:size]
    except (ValueError, RuntimeError, EOFError, IndexError) as exc:
        raise ValueError("固件镜像无效：" + str(exc)) from None


def load_bundle(bundle):
    bundle = Path(bundle)
    manifest = json.loads((bundle / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("schema") != 1 or manifest.get("chip") != "esp32c3":
        raise ValueError("安装包清单不兼容")
    merged = (bundle / "FoloToy-AI-Passport-full.bin").read_bytes()
    if digest(merged) != manifest.get("firmware_sha256"):
        raise ValueError("安装包摘要不符，请重新下载可信发布包")
    if not APP_OFFSET < len(merged) <= APP_OFFSET + APP_LIMIT:
        raise ValueError("此安装器不支持包含外部资源分区的固件")
    table = merged[0x8000:0x8C00]
    validate_table(table)
    if RECOVERY_BOOT_MARKER not in merged[:0x8000]:
        raise ValueError("发布固件缺少 Recovery 启动契约")
    app = image_bytes(merged[APP_OFFSET:])
    if digest(app) != manifest.get("app_sha256") or len(app) != manifest.get("app_size"):
        raise ValueError("应用清单与固件不匹配")
    return manifest, table, app


def validate_backup(flash, table):
    if len(flash) != FLASH_SIZE:
        raise ValueError("备份不足完整 8 MiB；禁止刷写")
    validate_table(flash[0x8000:0x8C00])
    if flash[0x8000:0x8C00] != table:
        raise ValueError("原机分区与固件不一致；禁止刷写，请走厂商兼容安装流程")
    if RECOVERY_BOOT_MARKER not in flash[:0x8000]:
        raise ValueError("原机缺少已知 Recovery 启动钩子；本工具不会替换 bootloader")
    if flash[0x356000:0x35A000] == b"\xff" * 0x4000:
        raise ValueError("设备身份区为空；请先通过厂商流程恢复，不在这里修复")
    if flash[0x700000] != 0xE9:
        raise ValueError("Recovery 区没有可识别固件，未刷写。请保留备份并联系维护者；不要全盘擦除")


class EspDevice:
    """One pinned-esptool connection from identification through verification."""
    def __init__(self, port):
        import esptool
        if esptool.__version__ != "4.12.0":
            raise ValueError("需要 esptool 4.12.0，请重新运行启动器安装依赖")
        self.esp = esptool.detect_chip(port, baud=115200)
        try:
            esp = self.esp
            if esp.CHIP_NAME != "ESP32-C3" or esp.secure_download_mode:
                raise ValueError("仅支持非安全下载模式的 ESP32-C3 Passport")
            if esp.get_secure_boot_enabled() or esp.get_flash_encryption_enabled():
                raise ValueError("设备开启安全启动或 Flash 加密，停止操作")
            self.device_id = bytes(esp.read_mac()).hex()
            self.esp = esp.run_stub()
            if esptool.cmds.detect_flash_size(self.esp) != "8MB":
                raise ValueError("设备 Flash 不是 8 MB")
            self.esp.change_baud(460800)
            # A reconnect must return to the wizard and take a fresh backup,
            # never silently write to a different device at the reused port.
            self.esp.WRITE_FLASH_ATTEMPTS = 1
        except BaseException:
            self.close()
            raise

    def read(self, offset, size):
        last = -1
        def progress(received, total):
            nonlocal last
            step = min(10, received * 10 // total)
            if step != last:
                last = step
                print("读取校验 %d%%" % (step * 10), flush=True)
        return self.esp.read_flash(offset, size, progress)

    def write_app(self, path):
        from esptool.cmds import write_flash
        with Path(path).open("rb") as file:
            args = SimpleNamespace(addr_filename=[(APP_OFFSET, file)],
                                   compress=True, no_compress=False, no_stub=False,
                                   force=False, encrypt=False, encrypt_files=None,
                                   ignore_flash_encryption_efuse_setting=False,
                                   flash_size="keep", flash_mode="keep", flash_freq="keep",
                                   erase_all=False)
            write_flash(self.esp, args)

    def reset(self):
        self.esp.hard_reset()

    def close(self):
        self.esp._port.close()


def install(device, table, app, home, confirm=input, write=False):
    backups = private_dir(Path(home) / "backups")
    directory = Path(tempfile.mkdtemp(prefix=dt.datetime.now().strftime("%Y%m%d-%H%M%S-"), dir=backups))
    print("正在备份整机，请勿拔线。备份只保存在本机：", directory, flush=True)
    before = device.read(0, FLASH_SIZE)
    save_private(directory / "flash-before.bin", before)
    save_private(directory / "backup.json", json.dumps({"device": device.device_id,
                 "sha256": digest(before), "size": len(before)}, indent=2).encode())
    validate_backup(before, table)
    # Re-read from flash to validate transport consistency, not only disk writes.
    if digest(device.read(0, FLASH_SIZE)) != digest(before):
        raise ValueError("两次整机读取不一致，已停止；未刷写")
    if not write:
        print("检查与备份通过；本次没有刷写。Recovery 内容完整性仍需实际启动验证。")
        device.reset()
        return directory
    print("将替换此设备的应用；原来的其他应用会被替换，成长存档不擦除。")
    print("只写 0x10000 应用区，不写 bootloader、分区表、NVS、身份或 Recovery。")
    if confirm("确认是你的 Passport，输入 INSTALL 开始（其他输入取消）：").strip() != "INSTALL":
        device.reset()
        return directory
    save_private(directory / "new-app.bin", app)
    device.write_app(directory / "new-app.bin")
    after = device.read(0, FLASH_SIZE)
    if (len(after) != FLASH_SIZE or after[APP_OFFSET:APP_OFFSET + len(app)] != app or
            after[:APP_OFFSET] != before[:APP_OFFSET] or
            after[APP_OFFSET + APP_LIMIT:] != before[APP_OFFSET + APP_LIMIT:]):
        raise ValueError("刷后校验失败，未宣告成功；请保留备份，勿自行全盘恢复／反复刷写")
    save_private(directory / "verified.json", json.dumps({"app_sha256": digest(app),
                 "protected_regions_equal": True}).encode())
    device.reset()
    print("应用刷写及保护区校验通过。接下来仍需启动检查与独立配对。")
    return directory


def choose_port():
    from serial.tools.list_ports import comports
    ports = [p for p in comports() if p.vid == 0x303A and p.pid == 0x1001 and p.device.startswith("/dev/cu.")]
    if len(ports) != 1:
        raise ValueError("请只接一台 Passport，并使用数据线；当前识别到 %d 个 ESP USB 串口" % len(ports))
    print("识别到 ESP USB 串口：", ports[0].device)
    return ports[0].device


def device_config(home, port):
    from serial.tools.list_ports import comports
    candidates = [p for p in comports() if p.device == port]
    if len(candidates) != 1 or not candidates[0].serial_number:
        raise ValueError("USB 设备没有稳定序列号，不能安全建立独立配对档案")
    identity = digest(candidates[0].serial_number.encode())[:24]
    private_dir(Path(home) / "devices")
    return private_dir(Path(home) / "devices" / identity) / "link.json"


def select_config(home):
    configs = sorted((Path(home) / "devices").glob("*/link.json"))
    if not configs:
        raise ValueError("尚未配对，请先选择 3")
    for i, config in enumerate(configs, 1):
        print("%d 胸牌 %s" % (i, config.parent.name[-6:]))
    answer = input("选择送饭的胸牌编号：").strip()
    if not answer.isdigit() or not 1 <= int(answer) <= len(configs):
        raise ValueError("没有选择有效的胸牌")
    return configs[int(answer) - 1]


def pair(port, config, confirm=input):
    import pet_companion as c
    import pet_wireless as w
    private_dir(Path(config).parent)
    serial = c.open_port(port)
    try:
        house = c.exchange(serial, "PET2 HOUSE", "HOUSE")
        if house.get("version") != "5" or house.get("storage") != "1":
            raise ValueError("固件版本或存储状态不兼容；未进行配对")
        link = c.exchange(serial, "PET2 LINK", "LINK")
        if link.get("paired") == "1" and not Path(config).exists():
            raise ValueError("设备已配对，但此电脑没有对应密钥。请使用原电脑的私有配对文件；不会重置或覆盖")
        if link.get("paired") not in ("0", "1"):
            raise ValueError("未知配对状态")
        if confirm("将通过 USB 配对此设备，密钥只留在本机。输入 PAIR 继续：").strip() != "PAIR":
            return False
        key = w.load_key(config, create=True)
        result = c.exchange(serial, "PET2 PAIR " + key.hex(), "PAIRED")
        if result.get("id") != w.key_id(key):
            raise ValueError("配对确认不匹配；请保留本机密钥后重试")
        print("固件启动及存储检查通过，USB 配对成功。请勿分享或删除配对文件。")
        return True
    finally:
        serial.close()


def source_args(executable):
    return SimpleNamespace(csv=None, kaboo_cli=executable, cached_export=False,
                           goal=None, watch=True, interval=60, settlement_provider=None)


def check_kaboo(executable):
    import pet_companion as c
    executable = os.path.expanduser(executable)
    program = shutil.which(executable)
    if program is None:
        raise ValueError("未找到 Kaboo CLI。请先安装你有权使用的 Kaboo，或填写其完整路径")
    try:
        totals = c.load_source(source_args(program))
    except (OSError, ValueError, c.subprocess.TimeoutExpired):
        raise ValueError("Kaboo 导出失败、超时或 CSV 格式不兼容；未连接胸牌，请确认 CLI 支持 export --format csv") from None
    today = dt.datetime.now(c.ZONE).date()
    if not totals or max(totals) < today:
        raise ValueError("导出没有今天的数据，不能确认实时接入；请先正常使用 Kaboo 后重试，不发送假零值")
    if max(totals) > today:
        raise ValueError("导出包含未来日期，停止同步")
    print("Kaboo CSV 格式及今日日期检查通过；没有保存或展示原始导出。")
    return program


async def sync_loop(config, executable):
    import pet_wireless as w
    from bleak.exc import BleakError
    from cryptography.exceptions import InvalidTag
    import subprocess
    key = w.load_key(config)
    args = source_args(executable)
    print("开始约每分钟送饭；请保持窗口运行。Ctrl-C 停止，不安装自启服务。")
    while True:
        try:
            await w.sync_once(args, key, require_current_day=True)
            print("胸牌已认证确认本次饭盒同步。", flush=True)
        except (OSError, ValueError, TimeoutError, asyncio.TimeoutError,
                BleakError, InvalidTag, subprocess.TimeoutExpired) as exc:
            # Library/source exceptions can contain local data: report type only.
            print("本轮未确认同步（%s）；请检查胸牌、蓝牙权限及 Kaboo。60 秒后重试。" % type(exc).__name__, flush=True)
        await asyncio.sleep(60)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, default=Path(__file__).resolve().parent.parent / "firmware")
    parser.add_argument("--home", type=Path, default=DATA_HOME)
    parser.add_argument("--config", type=Path, help="existing private key file; never reset or copy automatically")
    parser.add_argument("--check-bundle", action="store_true", help="validate package only; no USB access")
    args = parser.parse_args()
    manifest, table, app = load_bundle(args.bundle)
    print("数码宝贝 Passport · Mac 内测安装向导\n固件版本：", manifest["firmware_version"])
    if args.check_bundle:
        print("安装包校验通过；没有连接或修改设备。")
        return
    if sys.platform != "darwin" or tuple(map(int, platform.mac_ver()[0].split(".")[:2])) < (12, 3):
        raise ValueError("本内测包只支持 macOS 12.3+，Windows 尚未验收")
    os.umask(0o077)
    with exclusive(args.home):
        config = args.config.expanduser() if args.config else None
        print("1 安装／升级固件\n2 检查设备并备份（不刷写，会重启）\n3 启动检查与 USB 配对\n4 检测 Kaboo 并持续送饭\n其他 退出")
        choice = input("请选择：").strip()
        if choice in ("1", "2"):
            print("请停止所有旧的 USB／蓝牙送饭脚本，只连接一台 Passport。")
            if input("此操作会暂时重启设备。输入 CHECK 开始检测和备份：").strip() != "CHECK":
                return
            device = EspDevice(choose_port())
            try:
                install(device, table, app, args.home, write=choice == "1")
            finally:
                device.close()
            print("若已刷写，等待胸牌启动后重新打开向导选择 3；若未启动，请保留备份求助。")
        elif choice == "3":
            port = choose_port()
            pair(port, config or device_config(args.home, port))
        elif choice == "4":
            config = config or select_config(args.home)
            executable = input("Kaboo CLI 路径（直接回车自动查找 kaboo-cli）：").strip() or "kaboo-cli"
            executable = check_kaboo(executable)
            print("首次蓝牙权限提示请按系统引导授权；USB 可拔掉。")
            asyncio.run(sync_loop(config, executable))


if __name__ == "__main__":
    try:
        main()
    except (KeyboardInterrupt, EOFError):
        print("\n已停止。若刷写中断，请保留备份，不要全盘擦除。")
        sys.exit(130)
    except Exception as exc:
        # Do not dump stack traces, packet contents or credentials to shared logs.
        print("操作停止：" + (str(exc) if isinstance(exc, ValueError) else type(exc).__name__))
        sys.exit(1)
