"""Headless desktop services: private preferences, opt-in login and cancellable feeding."""
from __future__ import annotations

import asyncio
import contextlib
import datetime as dt
import json
import os
from pathlib import Path
import plistlib
import shutil
import signal
import tempfile
import threading
from types import SimpleNamespace

import pet_companion as c
from pet_install import DATA_HOME, private_dir

LOGIN_LABEL = "io.oblivior.aipetpassport.login"
MAX_EXPORT = 64 * 1024 * 1024


class UserProblem(ValueError):
    """Safe, deliberately written text suitable for a desktop alert."""


def atomic_private(path, data):
    path = Path(path)
    private_dir(path.parent)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as file:
        file.write(data)
        file.flush()
        os.fsync(file.fileno())
    try:
        os.replace(file.name, path)
    finally:
        with contextlib.suppress(FileNotFoundError):
            Path(file.name).unlink()


def stage_name(stage):
    names = ("数码蛋", "幼年Ⅰ", "幼年Ⅱ", "成长期", "成熟期", "完全体", "究极体")
    return names[int(stage)] if str(stage) in tuple(map(str, range(7))) else "—"


def feeding_control(busy, kind, stopping, problem):
    """Presentation only: never imply that a running worker proves a live badge."""
    if busy and kind == "usb":
        return "设备操作中…", False, "USB 操作中", "muted"
    if busy and stopping:
        return "正在停止…", False, "正在停止", "muted"
    if busy:
        return ("停止检测" if kind == "source" else "暂停送饭", True,
                "等待重试" if problem else "检测 Kaboo" if kind == "source" else "自动送饭中",
                "warning" if problem else "grass")
    return "开始送饭", True, "需要检查" if problem else "送饭已暂停", "warning" if problem else "muted"


def delivery_caption(value, now=None):
    try:
        stamp = dt.datetime.fromisoformat(value).astimezone()
        today = now or dt.datetime.now().astimezone()
        pattern = "%H:%M:%S" if stamp.date() == today.date() else "%m-%d %H:%M" if stamp.year == today.year else "%Y-%m-%d %H:%M"
        return "最近送达  " + stamp.strftime(pattern) + "  ·  数据快照，非实时状态"
    except (ValueError, TypeError):
        return "尚未收到胸牌确认  ·  数据以最近一次送达为准"


class Preferences:
    def __init__(self, home=DATA_HOME):
        self.home = private_dir(home)
        self.path = self.home / "desktop.json"
        self.data = {"schema": 1, "kaboo": "", "config": "", "login": False, "last": {}}
        if self.path.exists() or self.path.is_symlink():
            if self.path.is_symlink() or self.path.stat().st_mode & 0o077:
                raise UserProblem("桌面配置权限异常，请先检查本机私有配置，不会覆盖。")
            try:
                loaded = json.loads(self.path.read_text())
                if loaded.get("schema") != 1 or type(loaded.get("login")) is not bool:
                    raise ValueError()
                for field in ("kaboo", "config"):
                    if not isinstance(loaded.get(field), str):
                        raise ValueError()
                last = loaded.get("last", {})
                if not isinstance(last, dict):
                    raise ValueError()
                self.data.update({k: loaded[k] for k in ("kaboo", "config", "login")})
                # Never persist arbitrary ACK fields (goal, raw usage, etc.).
                self.data["last"] = {k: str(last[k])[:80] for k in ("at", "pending", "stage", "days") if k in last}
            except (ValueError, TypeError, AttributeError):
                raise UserProblem("桌面配置无法读取，请保留文件联系维护者，不会重置配对。") from None

    def save(self, **changes):
        new = dict(self.data, **changes)
        atomic_private(self.path, json.dumps(new, ensure_ascii=False, indent=2).encode())
        self.data = new

    def remember_ack(self, result):
        last = {k: str(result[k]) for k in ("pending", "stage", "days") if k in result}
        last["at"] = dt.datetime.now().astimezone().isoformat(timespec="seconds")
        self.save(last=last)


def discover_kaboo():
    found = shutil.which("kaboo-cli")
    if found:
        return found
    # Finder/login PATH does not load shell rc files. Probe only known CLI install locations.
    options = [Path("/opt/homebrew/bin/kaboo-cli"), Path("/usr/local/bin/kaboo-cli")]
    options += sorted((Path.home() / ".nvm/versions/node").glob("*/bin/kaboo-cli"), reverse=True)
    return next((str(p) for p in options if p.is_file() and os.access(p, os.X_OK)), "")


def export_env(executable, cache_path):
    env = os.environ.copy()
    env["KABOO_SKIP_AUTO_UPDATE"] = "1"
    env["KABOO_SCAN_CACHE_PATH"] = str(cache_path)
    env["PATH"] = str(Path(executable).absolute().parent) + os.pathsep + env.get("PATH", "/usr/bin:/bin")
    # Frozen-Python hints must not leak to an independently installed Kaboo executable.
    for field in ("PYTHONHOME", "PYTHONPATH", "_MEIPASS2"):
        env.pop(field, None)
    return env


async def _bounded_stdout(stream):
    chunks, size = [], 0
    while True:
        chunk = await stream.read(65536)
        if not chunk:
            return b"".join(chunks)
        size += len(chunk)
        if size > MAX_EXPORT:
            raise UserProblem("Kaboo 导出超过 64 MiB，请联系维护者检查数据规模。")
        chunks.append(chunk)


async def load_kaboo(executable, timeout=120):
    executable = os.path.expanduser(executable)
    if not os.path.isabs(executable) or not os.path.isfile(executable) or not os.access(executable, os.X_OK):
        raise UserProblem("请先选择已安装、可执行的 Kaboo CLI 程序。")
    with tempfile.TemporaryDirectory(prefix="passport-kaboo-") as cache:
        process = await asyncio.create_subprocess_exec(
            executable, "export", "--format", "csv",
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL,
            env=export_env(executable, Path(cache) / "scan-cache.db"), start_new_session=True)
        try:
            async def collect():
                data = await _bounded_stdout(process.stdout)
                await process.wait()
                return data
            data = await asyncio.wait_for(collect(), timeout)
            if process.returncode:
                raise UserProblem("Kaboo 导出失败，请确认 CLI 可用且支持 export --format csv。")
            try:
                totals = c.aggregate_csv(data.decode("utf-8-sig"))
            except (ValueError, UnicodeError, KeyError, TypeError):
                raise UserProblem("Kaboo 导出格式不兼容，未向胸牌发送数据。") from None
            today = dt.datetime.now(c.ZONE).date()
            if today not in totals or max(totals) > today:
                raise UserProblem("还没有可靠的今日数据。正常使用 Kaboo 后会自动重试，不发送假零值。")
            return totals
        finally:
            if process.returncode is None:
                # Only the process group created above; never target an existing Kaboo app.
                with contextlib.suppress(ProcessLookupError):
                    os.killpg(process.pid, signal.SIGTERM)
                try:
                    await asyncio.wait_for(process.wait(), 3)
                except asyncio.TimeoutError:
                    with contextlib.suppress(ProcessLookupError):
                        os.killpg(process.pid, signal.SIGKILL)
                    await process.wait()


def login_path(directory=None):
    return (Path(directory) if directory else Path.home() / "Library/LaunchAgents") / (LOGIN_LABEL + ".plist")


def login_payload(executable):
    return {"Label": LOGIN_LABEL, "ProgramArguments": [str(executable), "--login"],
            "RunAtLoad": True, "KeepAlive": False, "ProcessType": "Interactive"}


def check_owned_login(path):
    if path.is_symlink():
        raise UserProblem("登录项路径异常，不会覆盖。")
    if path.exists():
        try:
            value = plistlib.loads(path.read_bytes())
            args = value.get("ProgramArguments", [])
            if (value.get("Label") != LOGIN_LABEL or not isinstance(args, list)
                    or len(args) != 2 or args[1] != "--login" or not isinstance(args[0], str)
                    or not args[0].endswith("/AI Pet Passport.app/Contents/MacOS/AI Pet Passport")):
                raise ValueError()
        except (ValueError, TypeError, plistlib.InvalidFileException, AttributeError):
            raise UserProblem("发现不属于本应用的同名登录项，未修改。") from None


def set_login(enabled, executable, directory=None):
    path = login_path(directory)
    check_owned_login(path)
    if enabled:
        executable = Path(executable).absolute()
        app = executable.parent.parent.parent
        allowed = (Path("/Applications"), Path.home() / "Applications")
        if (app.parent not in allowed or app.name != "AI Pet Passport.app"
                or executable.parts[-3:] != ("Contents", "MacOS", "AI Pet Passport")
                or not executable.is_file()):
            raise UserProblem("请先把 App 移到 Applications 文件夹，再开启登录自启。")
        path.parent.mkdir(parents=True, exist_ok=True)
        # LaunchAgents may already be 755; do not change the user's directory permissions.
        with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as file:
            file.write(plistlib.dumps(login_payload(executable)))
            file.flush()
            os.fsync(file.fileno())
        os.replace(file.name, path)
    elif path.exists():
        path.unlink()  # Only this app's validated, opt-in login item; never a directory.


def configure_login(prefs, enabled, executable, directory=None):
    # Commit the preference first. If the login file cannot be changed, restore it.
    # A stale launch item can never auto-feed when the saved preference is false.
    old = prefs.data["login"]
    prefs.save(login=enabled)
    try:
        set_login(enabled, executable, directory)
    except Exception:
        prefs.save(login=old)
        raise


def user_error(exc):
    if isinstance(exc, UserProblem):
        return str(exc)
    message = str(exc)
    if "Recovery" in message:
        return "胸牌恢复区检查未通过，已停止，未继续刷写。请保留备份联系维护者。"
    if "分区" in message:
        return "胸牌分区不兼容，本工具不会改分区或全盘擦除。"
    if "已配对" in message or "pairing" in message:
        return "请载入这台胸牌原有的私有配对文件，不要重置或使用别人的密钥。"
    if isinstance(exc, (TimeoutError, asyncio.TimeoutError)):
        return "本轮超时，请检查胸牌电源、距离、蓝牙权限及 Kaboo。运行中会自动重试。"
    return "操作未确认成功，请检查设备连接、蓝牙权限和程序配置后重试。"


class Worker:
    """Exactly one worker; only feeding/export can be cancelled, never flashing."""
    def __init__(self, emit):
        self.emit = emit
        self.thread = None
        self.loop = None
        self.task = None
        self.kind = None
        self.stop_requested = threading.Event()

    @property
    def busy(self):
        return self.thread is not None and self.thread.is_alive()

    def start(self, kind, function):
        if self.busy:
            return False
        self.kind = kind
        self.stop_requested.clear()
        def run():
            try:
                function()
            except BaseException as exc:
                if not isinstance(exc, asyncio.CancelledError):
                    self.emit("error", user_error(exc))
            finally:
                self.loop = self.task = None
                self.emit("finished", None)
        self.thread = threading.Thread(target=run, daemon=True, name="Passport worker")
        self.thread.start()
        return True

    def stop(self):
        if self.kind not in ("feed", "source"):
            return False
        self.stop_requested.set()
        if self.loop is not None and self.task is not None:
            with contextlib.suppress(RuntimeError):
                self.loop.call_soon_threadsafe(self.task.cancel)
        return True

    def run_async(self, factory):
        async def run():
            self.loop = asyncio.get_running_loop()
            self.task = asyncio.current_task()
            if self.stop_requested.is_set():
                return
            await factory()
        asyncio.run(run())

    async def feed(self, executable, config, interval=60, once=False):
        import pet_wireless as w
        key = w.load_key(config)
        args = SimpleNamespace(goal=None)
        while not self.stop_requested.is_set():
            try:
                self.emit("phase", "source")
                totals = await load_kaboo(executable)
                result = await w.sync_once(args, key, totals=totals, require_current_day=True,
                                           quiet=True, progress=lambda phase: self.emit("phase", phase))
                self.emit("ack", result)
                self.emit("phase", "waiting")
            except asyncio.CancelledError:
                raise
            except Exception as exc:
                self.emit("error", user_error(exc))
            if once:
                return
            await asyncio.sleep(interval)
