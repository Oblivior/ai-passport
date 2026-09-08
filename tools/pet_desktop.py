#!/usr/bin/env python3
"""Native AppKit companion. The bundle embeds Python; users need no terminal."""
from __future__ import annotations

import argparse
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import threading

import AppKit as A
import Foundation as F
import objc
from PyObjCTools import AppHelper

import pet_install as installer
import pet_desktop_core as core

VERSION = "0.2.0"
WIDTH, HEIGHT = 864, 700
COLORS = {"sky": "E9F4FC", "blue": "215CE5", "ink": "16304C",
          "meal": "F2C94C", "grass": "58A36A", "paper": "FFFFFF", "muted": "586C81"}


def color(name):
    value = COLORS[name]
    return A.NSColor.colorWithSRGBRed_green_blue_alpha_(
        int(value[:2], 16) / 255, int(value[2:4], 16) / 255, int(value[4:], 16) / 255, 1)


def rect(x, y, width, height):
    return F.NSMakeRect(x, HEIGHT - y - height, width, height)


def label(parent, text, x, y, width, height=24, size=14, tone="ink", bold=False):
    view = A.NSTextField.labelWithString_(text)
    view.setFrame_(rect(x, y, width, height))
    view.setFont_(A.NSFont.fontWithName_size_("PingFangSC-Semibold" if bold else "PingFangSC-Regular", size)
                  or A.NSFont.systemFontOfSize_(size))
    view.setTextColor_(color(tone))
    view.setMaximumNumberOfLines_(0)
    view.setLineBreakMode_(A.NSLineBreakByWordWrapping)
    parent.addSubview_(view)
    return view


def panel(parent, x, y, width, height, tone):
    view = A.NSView.alloc().initWithFrame_(rect(x, y, width, height))
    view.setWantsLayer_(True)
    view.layer().setBackgroundColor_(color(tone).CGColor())
    view.layer().setCornerRadius_(12)
    parent.addSubview_(view)
    return view


class ProgressWriter(io.TextIOBase):
    def __init__(self, emit):
        self.emit = emit
        self.pending = ""

    def write(self, text):
        self.pending = (self.pending + text)[-1024:]
        if "\n" in self.pending:
            for line in self.pending.splitlines():
                if line.startswith("读取校验 ") or line.startswith("Writing at "):
                    self.emit("progress", line)
                elif line.startswith("正在备份整机"):
                    self.emit("progress", "正在保存本机私有备份，请勿拔线。")
            self.pending = ""
        return len(text)


class DesktopDelegate(F.NSObject):
    @objc.python_method
    def configure(self, home, bundle, login):
        self.home, self.bundle, self.login = home, bundle, login
        self.prefs = core.Preferences(home)
        self.worker = core.Worker(lambda kind, data: AppHelper.callAfter(self.event, kind, data))
        self.phase = "paused"
        self.problem = False
        self.quit_pending = False
        self.controls = []
        self.maintenance = []

    def applicationDidFinishLaunching_(self, _):
        self.build()
        if self.login and self.prefs.data["login"]:
            self.start_(None)

    @objc.python_method
    def button(self, text, action, x, y, width, height=32, primary=False):
        view = A.NSButton.buttonWithTitle_target_action_(text, self, action)
        view.setFrame_(rect(x, y, width, height))
        view.setBezelStyle_(A.NSBezelStyleRounded)
        view.setFont_(A.NSFont.systemFontOfSize_(14))
        if primary:
            view.setBezelColor_(color("blue"))
            view.setContentTintColor_(color("paper"))
        self.content.addSubview_(view)
        return view

    @objc.python_method
    def build(self):
        self.window = A.NSWindow.alloc().initWithContentRect_styleMask_backing_defer_(
            F.NSMakeRect(0, 0, WIDTH, HEIGHT),
            A.NSWindowStyleMaskTitled | A.NSWindowStyleMaskClosable | A.NSWindowStyleMaskMiniaturizable,
            A.NSBackingStoreBuffered, False)
        self.window.setTitle_("AI Pet Passport · 伙伴补给站")
        self.window.setReleasedWhenClosed_(False)
        self.window.setAppearance_(A.NSAppearance.appearanceNamed_(A.NSAppearanceNameAqua))
        self.window.setBackgroundColor_(color("sky"))
        self.content = self.window.contentView()
        panel(self.content, 24, 20, 816, 94, "blue")
        label(self.content, "伙伴补给站", 46, 31, 500, 44, 29, "paper", True)
        label(self.content, "Kaboo 的日常使用，变成随身伙伴的食物。", 48, 79, 620, 22, 13, "paper")
        panel(self.content, 680, 39, 136, 34, "meal")
        label(self.content, "饭盒  /  " + VERSION, 697, 43, 115, 24, 12, "ink", True)
        panel(self.content, 24, 130, 510, 234, "paper")
        panel(self.content, 550, 130, 290, 234, "paper")
        self.status = label(self.content, "准备好送饭了吗？", 46, 148, 462, 32, 21, bold=True)
        self.detail = label(self.content, "选好 Kaboo 和胸牌配对，再开始自动送饭。", 46, 189, 460, 48, 13, "muted")
        self.values = []
        for x, name in ((46, "待吃食物"), (204, "伙伴阶段"), (368, "陪伴天数")):
            label(self.content, name, x, 249, 142, 23, 12, "muted")
            self.values.append(label(self.content, "—", x, 276, 140, 42, 29, "ink", True))
        self.last = label(self.content, "尚未收到胸牌的送达确认", 46, 329, 470, 22, 12, "muted")
        self.start_button = self.button("开始自动送饭", "start:", 572, 151, 246, 44, True)
        self.stop_button = self.button("停止送饭", "stop:", 572, 207, 246, 38)
        self.login_check = A.NSButton.checkboxWithTitle_target_action_("登录后启动并自动送饭", self, "loginChanged:")
        self.login_check.setFrame_(rect(576, 268, 244, 28))
        self.login_check.setFont_(A.NSFont.systemFontOfSize_(12))
        self.login_check.setState_(int(self.prefs.data["login"]))
        self.content.addSubview_(self.login_check)
        label(self.content, "约每分钟同步 · 不用就休息\n关闭窗口后可从菜单栏操作", 577, 307, 237, 42, 12, "muted")
        panel(self.content, 24, 382, 816, 154, "paper")
        label(self.content, "本机连接", 46, 395, 200, 26, 16, bold=True)
        label(self.content, "Kaboo 程序", 46, 435, 96, 24, 13, "muted")
        self.kaboo = A.NSTextField.alloc().initWithFrame_(rect(146, 432, 470, 27))
        self.kaboo.setPlaceholderString_("选择你已安装的 Kaboo CLI")
        self.kaboo.setStringValue_(self.prefs.data["kaboo"] or core.discover_kaboo())
        self.kaboo.setFont_(A.NSFont.systemFontOfSize_(12))
        self.content.addSubview_(self.kaboo)
        self.controls += [self.button("选择…", "chooseKaboo:", 626, 430, 83),
                          self.button("检测", "checkSource:", 720, 430, 96)]
        label(self.content, "胸牌配对", 46, 484, 96, 24, 13, "muted")
        self.config_label = label(self.content, "尚未选择", 146, 483, 280, 30, 12, "muted")
        self.controls += [self.button("载入配对…", "chooseKey:", 448, 478, 112),
                          self.button("使用旧版配对", "legacyKey:", 568, 478, 130),
                          self.button("USB 配对", "pairUSB:", 704, 478, 114)]
        panel(self.content, 24, 554, 816, 99, "paper")
        label(self.content, "安装与维护", 46, 570, 170, 26, 16, bold=True)
        label(self.content, "仅通过 USB 操作，备份先行。分区或恢复区不兼容时会停止。", 46, 615, 760, 22, 12, "muted")
        self.maintenance += [self.button("检查并备份", "backup:", 350, 567, 140),
                             self.button("安装固件", "installFirmware:", 504, 567, 136),
                             self.button("查看备份", "showBackups:", 654, 567, 162)]
        label(self.content, "仅用本机 Kaboo 数据 · 不接官方月结 · 不上传原始用量", 36, 671, 800, 20, 12, "muted")
        self.menus()
        self.show_preferences()
        self.refresh_(None)
        self.timer = F.NSTimer.scheduledTimerWithTimeInterval_target_selector_userInfo_repeats_(
            0.25, self, "refresh:", None, True)
        self.window.center()
        self.show_(None)

    @objc.python_method
    def menus(self):
        menubar = A.NSMenu.alloc().init()
        root = A.NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("AI Pet Passport", None, "")
        menu = A.NSMenu.alloc().initWithTitle_("AI Pet Passport")
        for title, action, key in (("显示补给站", "show:", "0"), ("退出补给站", "quit:", "q")):
            item = menu.addItemWithTitle_action_keyEquivalent_(title, action, key)
            item.setTarget_(self)
        root.setSubmenu_(menu)
        menubar.addItem_(root)
        edit_root = A.NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("编辑", None, "")
        edit_menu = A.NSMenu.alloc().initWithTitle_("编辑")
        for title, action, key in (("剪切", "cut:", "x"), ("复制", "copy:", "c"), ("粘贴", "paste:", "v"), ("全选", "selectAll:", "a")):
            edit_menu.addItemWithTitle_action_keyEquivalent_(title, action, key)
        edit_root.setSubmenu_(edit_menu)
        menubar.addItem_(edit_root)
        A.NSApp.setMainMenu_(menubar)
        self.tray = A.NSStatusBar.systemStatusBar().statusItemWithLength_(A.NSVariableStatusItemLength)
        self.tray.button().setTitle_("饭盒 · 暂停")
        tray_menu = A.NSMenu.alloc().initWithTitle_("饭盒")
        for title, action in (("显示补给站", "show:"), ("开始自动送饭", "start:"), ("停止送饭", "stop:"), ("退出", "quit:")):
            tray_menu.addItemWithTitle_action_keyEquivalent_(title, action, "").setTarget_(self)
        self.tray.setMenu_(tray_menu)

    @objc.python_method
    def alert(self, title, message, confirm="知道了", cancel=None):
        alert = A.NSAlert.alloc().init()
        alert.setMessageText_(title)
        alert.setInformativeText_(message)
        alert.addButtonWithTitle_(confirm)
        if cancel:
            alert.addButtonWithTitle_(cancel)
        return alert.runModal() == A.NSAlertFirstButtonReturn

    @objc.python_method
    def confirm_from_worker(self, message, answer):
        done, value = threading.Event(), [""]
        def ask():
            try:
                if self.alert("请确认设备操作", message, "确认继续", "取消"):
                    value[0] = answer
            finally:
                done.set()
        AppHelper.callAfter(ask)
        done.wait()
        return value[0]

    @objc.python_method
    def show_preferences(self):
        config = self.prefs.data["config"]
        self.config_label.setStringValue_("已载入本机私有配对" if config else "首次使用请通过 USB 配对")
        last = self.prefs.data["last"]
        self.values[0].setStringValue_(str(last.get("pending", "—")))
        self.values[1].setStringValue_(core.stage_name(last.get("stage")))
        self.values[1].setFont_(A.NSFont.systemFontOfSize_(23))
        self.values[2].setStringValue_(str(last.get("days", "—")))
        if last.get("at"):
            self.last.setStringValue_("上次确认：" + last["at"].replace("T", " ")[:19] + " · 非实时在线状态")

    @objc.python_method
    def event(self, kind, data):
        if kind == "ack":
            try:
                self.prefs.remember_ack(data)
            except Exception:
                self.alert("已送达，但本机记录未保存", "请检查配置目录是否可写。不会重复产生食物。")
            self.show_preferences()
            self.problem = False
        elif kind == "phase":
            self.phase = data
            texts = {"source": ("正在读取 Kaboo", "只读取本机累计导出，不保存原始内容。"),
                     "discovering": ("正在寻找你的胸牌", "请保持胸牌开机、电脑蓝牙开启。只连接已配对设备。"),
                     "sending": ("正在确认饭盒送达", "蓝牙写入不等于投食成功，正在等待胸牌认证确认。"),
                     "waiting": ("饭盒已确认送达", "稍后自动同步下一轮。食物留在胸牌上，按确定喂给伙伴。")}
            title, detail = texts[data]
            self.status.setStringValue_(title)
            self.detail.setStringValue_(detail)
        elif kind == "error":
            self.problem = True
            self.status.setStringValue_("这一步还没完成")
            self.detail.setStringValue_(data)
        elif kind == "progress":
            self.detail.setStringValue_(data)
        elif kind == "source_ok":
            self.status.setStringValue_("Kaboo 已准备好")
            self.detail.setStringValue_("今日累计数据检查通过。选择胸牌配对后即可开始送饭。")
        elif kind == "paired":
            self.prefs.save(config=str(data))
            self.show_preferences()
            self.status.setStringValue_("胸牌配对完成")
            self.detail.setStringValue_("密钥只保存在你的电脑上；现在可以开始自动送饭。")
        elif kind == "installed":
            self.status.setStringValue_("设备操作完成")
            self.detail.setStringValue_(data)
        elif kind == "finished":
            if self.worker.stop_requested.is_set():
                self.status.setStringValue_("已停止送饭")
                self.detail.setStringValue_("现有食物和成长保留。下次点击开始即可继续。")
            self.phase = "paused"

    def refresh_(self, _):
        busy = self.worker.busy
        self.start_button.setEnabled_(not busy)
        self.stop_button.setEnabled_(busy and self.worker.kind in ("feed", "source") and not self.worker.stop_requested.is_set())
        self.kaboo.setEnabled_(not busy)
        for control in self.controls + self.maintenance:
            control.setEnabled_(not busy)
        self.tray.button().setTitle_("饭盒 · " + ("待重试" if busy and self.problem else "运行中" if busy else "暂停"))
        if self.quit_pending and not busy:
            self.quit_pending = False
            A.NSApp.replyToApplicationShouldTerminate_(True)

    def show_(self, _):
        self.window.makeKeyAndOrderFront_(None)
        A.NSApp.activateIgnoringOtherApps_(True)

    def applicationShouldHandleReopen_hasVisibleWindows_(self, app, visible):
        self.show_(None)
        return True

    def applicationShouldTerminateAfterLastWindowClosed_(self, _):
        return False

    def applicationShouldTerminate_(self, _):
        if self.worker.busy:
            if self.worker.kind == "usb":
                self.alert("设备操作还在进行", "请等待检查、备份或刷写结束后退出，避免中断设备操作。")
                return A.NSTerminateCancel
            self.worker.stop()
            self.quit_pending = True
            return A.NSTerminateLater
        return A.NSTerminateNow

    def quit_(self, _):
        A.NSApp.terminate_(None)

    @objc.python_method
    def executable(self):
        value = os.path.expanduser(str(self.kaboo.stringValue()).strip())
        if not value or not Path(value).is_file() or not os.access(value, os.X_OK):
            raise core.UserProblem("请选择已安装的 Kaboo CLI 可执行文件。")
        value = os.path.abspath(value)
        self.prefs.save(kaboo=value)
        return value

    def start_(self, _):
        if self.worker.busy:
            return
        try:
            executable = self.executable()
            config = self.prefs.data["config"]
            if not config:
                raise core.UserProblem("请先载入已有配对，或通过 USB 配对你的胸牌。")
            import pet_wireless as w
            w.load_key(config)
            self.problem = False
            self.worker.start("feed", lambda: self.worker.run_async(lambda: self.worker.feed(executable, config)))
        except Exception as exc:
            self.event("error", core.user_error(exc))

    def stop_(self, _):
        if self.worker.stop():
            self.status.setStringValue_("正在停止送饭")
            self.detail.setStringValue_("正在结束本轮读取或蓝牙连接，不会删除食物。")

    def checkSource_(self, _):
        if self.worker.busy:
            return
        try:
            executable = self.executable()
            async def check():
                self.worker.emit("phase", "source")
                await core.load_kaboo(executable)
                self.worker.emit("source_ok", None)
            self.worker.start("source", lambda: self.worker.run_async(check))
        except Exception as exc:
            self.event("error", core.user_error(exc))

    @objc.python_method
    def pick_file(self, title, directory=None):
        dialog = A.NSOpenPanel.openPanel()
        dialog.setTitle_(title)
        dialog.setCanChooseDirectories_(False)
        dialog.setAllowsMultipleSelection_(False)
        dialog.setResolvesAliases_(False)
        dialog.setShowsHiddenFiles_(True)
        if directory:
            dialog.setDirectoryURL_(F.NSURL.fileURLWithPath_(str(directory)))
        return str(dialog.URL().path()) if dialog.runModal() == A.NSModalResponseOK else None

    def chooseKaboo_(self, _):
        selected = self.pick_file("选择你自己的 Kaboo CLI 程序")
        if selected:
            self.kaboo.setStringValue_(selected)

    @objc.python_method
    def select_key(self, selected):
        if not selected:
            return
        try:
            import pet_wireless as w
            w.load_key(selected)
            self.prefs.save(config=selected)
            self.show_preferences()
            self.status.setStringValue_("已载入私有配对")
            self.detail.setStringValue_("开始送饭后，会验证是否能连接对应胸牌。没有复制或更换密钥。")
        except Exception as exc:
            self.event("error", core.user_error(exc))

    def chooseKey_(self, _):
        self.select_key(self.pick_file("选择这台胸牌原有的 link.json 配对文件", self.home))

    def legacyKey_(self, _):
        path = Path.home() / ".config/ai-passport/link.json"
        if not path.is_file():
            self.alert("没有找到旧版配对", "首次使用请选择 USB 配对；有其他位置的密钥请选择载入配对。")
        elif self.alert("使用这台电脑的旧版配对？", "只引用已有密钥文件，不重置胸牌、不复制密钥。", "使用", "取消"):
            self.select_key(str(path))

    def pairUSB_(self, _):
        if self.worker.busy or not self.alert("通过数据线配对胸牌", "请停止旧版同步脚本，只连接一台胸牌。不会复用其他胸牌的密钥。", "开始检查", "取消"):
            return
        def work():
            with contextlib.redirect_stdout(ProgressWriter(self.worker.emit)):
                port = installer.choose_port()
                config = installer.device_config(self.home, port)
                if installer.pair(port, config, lambda message: self.confirm_from_worker(message, "PAIR")):
                    self.worker.emit("paired", config)
        self.status.setStringValue_("正在检查 USB 配对")
        self.worker.start("usb", work)

    @objc.python_method
    def device_work(self, write):
        if self.worker.busy:
            return
        message = "请只连接你的一台 Passport，并停止旧版同步脚本。操作会暂时重启胸牌，不全盘擦除。"
        if write:
            message += "安装会替换原有应用；保留宠物成长存档，不保证其他应用存档兼容。备份和检查通过后还需再次确认。"
        if not self.alert("安装固件" if write else "检查并备份胸牌", message, "开始检查", "取消"):
            return
        def work():
            with contextlib.redirect_stdout(ProgressWriter(self.worker.emit)):
                _, table, app = installer.load_bundle(self.bundle)
                device = installer.EspDevice(installer.choose_port())
                approved = [False]
                def confirm(message):
                    answer = self.confirm_from_worker(message, "INSTALL")
                    approved[0] = answer == "INSTALL"
                    return answer
                try:
                    installer.install(device, table, app, self.home, confirm=confirm, write=write)
                    self.worker.emit("installed", "应用写入及保护区校验通过，等胸牌启动后进行 USB 配对。" if approved[0] else "备份与检查通过，没有刷写。备份只留在本机。")
                except BaseException:
                    if not approved[0]:
                        with contextlib.suppress(Exception):
                            device.reset()  # Pre-write failures must not leave the old app in ROM mode.
                    raise
                finally:
                    device.close()
        self.status.setStringValue_("正在检查并备份")
        self.detail.setStringValue_("请勿拔线，完整备份可能需要几分钟。")
        self.worker.start("usb", work)

    def backup_(self, _):
        self.device_work(False)

    def installFirmware_(self, _):
        self.device_work(True)

    def showBackups_(self, _):
        path = installer.private_dir(self.home / "backups")
        A.NSWorkspace.sharedWorkspace().openURL_(F.NSURL.fileURLWithPath_(str(path)))

    def loginChanged_(self, _):
        enabled = bool(self.login_check.state())
        old = self.prefs.data["login"]
        try:
            if enabled:
                if not self.prefs.data["config"] or not self.prefs.data["kaboo"]:
                    raise core.UserProblem("请先完成配对和 Kaboo 程序配置。")
                if not self.alert("登录后自动送饭？", "从下次登录开始启动本应用并读取 Kaboo 送饭。可随时取消；不会安装系统级服务。", "开启", "取消"):
                    self.login_check.setState_(int(old))
                    return
            core.configure_login(self.prefs, enabled, sys.executable)
            self.alert("已开启登录自启" if enabled else "已关闭登录自启", "从下次登录生效。macOS 可能要求在系统设置中允许此登录项。" if enabled else "不再在下次登录时启动；当前送饭不受影响。")
        except Exception as exc:
            self.login_check.setState_(int(old))
            self.alert("登录项未完成修改", core.user_error(exc))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--login", action="store_true")
    parser.add_argument("--state-dir", type=Path, default=installer.DATA_HOME)
    parser.add_argument("--smoke-test", action="store_true")
    args = parser.parse_args()
    os.umask(0o077)
    base = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent.parent))
    bundle = base / "firmware"
    if args.smoke_test:
        import CoreBluetooth
        import pet_wireless
        manifest, _, _ = installer.load_bundle(bundle)
        print(json.dumps({"desktop": VERSION, "firmware": manifest["firmware_version"],
                          "python": sys.version.split()[0], "frozen": bool(getattr(sys, "frozen", False))}))
        return
    app = A.NSApplication.sharedApplication()
    app.setActivationPolicy_(A.NSApplicationActivationPolicyRegular)
    try:
        with installer.exclusive(args.state_dir):
            delegate = DesktopDelegate.alloc().init()
            delegate.configure(args.state_dir, bundle, args.login)
            app.setDelegate_(delegate)
            AppHelper.runEventLoop()
    except Exception as exc:
        alert = A.NSAlert.alloc().init()
        alert.setMessageText_("补给站未能启动")
        alert.setInformativeText_("请检查是否已在运行，并保留本机配置。" if "正在运行" in str(exc) else core.user_error(exc))
        alert.runModal()


if __name__ == "__main__":
    main()
