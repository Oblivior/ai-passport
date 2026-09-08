#!/usr/bin/env python3
"""Native AppKit companion. The bundle embeds Python; users need no terminal."""
from __future__ import annotations

import argparse
import contextlib
from functools import lru_cache
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

VERSION = "0.2.1"
WIDTH, HEIGHT = 864, 644
COLORS = {"sky": "F2F5F9", "blue": "275DDA", "ink": "1C3048",
          "meal": "FFF2C6", "grass": "267354", "paper": "FFFFFF", "muted": "596A7E",
          "line": "E3E9F1", "softblue": "EDF2FC", "warning": "936216"}


@lru_cache(maxsize=None)
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


def rule(parent, x, y, width):
    view = panel(parent, x, y, width, 1, "line")
    view.layer().setCornerRadius_(0)


class FlatButton(A.NSButton):
    """Native keyboard/AX button with one consistent, non-gradient appearance."""
    @objc.python_method
    def paint(self):
        primary = getattr(self, "primary", False)
        self.layer().setBackgroundColor_(color("blue" if primary and self.isEnabled() else "softblue" if self.isEnabled() else "sky").CGColor())
        tone = "paper" if primary and self.isEnabled() else "blue" if self.isEnabled() else "muted"
        title = F.NSAttributedString.alloc().initWithString_attributes_(self.title(), {
            A.NSForegroundColorAttributeName: color(tone),
            A.NSFontAttributeName: A.NSFont.systemFontOfSize_weight_(13, A.NSFontWeightMedium),
        })
        self.setAttributedTitle_(title)

    def setEnabled_(self, enabled):
        if self.isEnabled() == enabled:
            return
        objc.super(FlatButton, self).setEnabled_(enabled)
        if self.layer() is not None:
            self.paint()

    def setTitle_(self, title):
        if self.title() == title:
            return
        objc.super(FlatButton, self).setTitle_(title)
        if self.layer() is not None:
            self.paint()


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
        view = FlatButton.buttonWithTitle_target_action_(text, self, action)
        view.setFrame_(rect(x, y, width, height))
        view.setBezelStyle_(A.NSBezelStyleRegularSquare)
        view.setBordered_(False)
        view.setFocusRingType_(A.NSFocusRingTypeExterior)
        view.setWantsLayer_(True)
        view.layer().setCornerRadius_(7)
        view.primary = primary
        view.paint()
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
        app_icon = A.NSImageView.alloc().initWithFrame_(rect(26, 26, 46, 46))
        app_icon.setImage_(A.NSImage.imageNamed_(A.NSImageNameApplicationIcon))
        app_icon.setImageScaling_(A.NSImageScaleProportionallyUpOrDown)
        self.content.addSubview_(app_icon)
        label(self.content, "伙伴补给站", 84, 20, 420, 34, 23, bold=True)
        label(self.content, "Kaboo 的日常使用，变成伙伴的每一餐。", 85, 58, 490, 22, 12, "muted")
        self.mode_dot = panel(self.content, 696, 38, 7, 7, "muted")
        self.mode = label(self.content, "尚未开始", 712, 31, 126, 23, 12, "muted")
        panel(self.content, 24, 104, 510, 232, "paper")
        panel(self.content, 550, 104, 290, 232, "paper")
        self.status = label(self.content, "准备好送饭了吗？", 46, 122, 464, 30, 20, bold=True)
        self.detail = label(self.content, "选好 Kaboo 和胸牌配对，再开始自动送饭。", 46, 164, 460, 42, 13, "muted")
        panel(self.content, 42, 216, 138, 66, "meal")
        self.values = []
        for x, name in ((56, "待吃食物"), (220, "进化阶段"), (390, "陪伴天数")):
            label(self.content, name, x, 223, 120, 20, 11, "muted")
            self.values.append(label(self.content, "—", x, 245, 120, 34, 25, "ink", True))
        self.last = label(self.content, "尚未收到胸牌确认 · 以下数据来自最近一次送达", 46, 302, 474, 19, 11, "muted")
        label(self.content, "自动送饭", 572, 122, 230, 26, 16, bold=True)
        label(self.content, "每轮完成后，约一分钟再同步", 572, 157, 246, 22, 12, "muted")
        self.start_button = self.button("开始送饭", "toggleFeeding:", 572, 190, 246, 40, True)
        self.login_check = A.NSButton.checkboxWithTitle_target_action_("登录后启动并自动送饭", self, "loginChanged:")
        self.login_check.setFrame_(rect(575, 248, 244, 26))
        self.login_check.setFont_(A.NSFont.systemFontOfSize_(12))
        self.login_check.setState_(int(self.prefs.data["login"]))
        self.content.addSubview_(self.login_check)
        label(self.content, "关窗口后，仍可从菜单栏管理", 576, 298, 242, 20, 11, "muted")
        panel(self.content, 24, 354, 816, 164, "paper")
        label(self.content, "连接设置", 46, 369, 200, 26, 15, bold=True)
        self.settings_note = label(self.content, "", 574, 373, 240, 20, 11, "muted")
        label(self.content, "Kaboo 程序", 46, 415, 96, 24, 13, "muted")
        self.kaboo = A.NSTextField.alloc().initWithFrame_(rect(146, 413, 470, 22))
        self.kaboo.setPlaceholderString_("选择你已安装的 Kaboo CLI")
        self.kaboo.setStringValue_(self.prefs.data["kaboo"] or core.discover_kaboo())
        self.kaboo.setFont_(A.NSFont.monospacedSystemFontOfSize_weight_(11, A.NSFontWeightRegular))
        self.kaboo.setTextColor_(color("muted"))
        self.kaboo.setBordered_(False)
        self.kaboo.setBackgroundColor_(color("sky"))
        self.kaboo.setFocusRingType_(A.NSFocusRingTypeExterior)
        self.kaboo.cell().setUsesSingleLineMode_(True)
        self.kaboo.cell().setLineBreakMode_(A.NSLineBreakByTruncatingMiddle)
        self.content.addSubview_(self.kaboo)
        self.controls += [self.button("选择…", "chooseKaboo:", 630, 409, 82, 30),
                          self.button("检测", "checkSource:", 722, 409, 94, 30)]
        rule(self.content, 46, 450, 770)
        label(self.content, "胸牌配对", 46, 472, 96, 24, 13, "muted")
        self.config_label = label(self.content, "尚未选择", 146, 471, 272, 26, 12, "muted")
        self.controls += [self.button("载入配对…", "chooseKey:", 448, 465, 112, 30),
                          self.button("使用旧版配对", "legacyKey:", 570, 465, 132, 30),
                          self.button("USB 配对", "pairUSB:", 712, 465, 104, 30)]
        label(self.content, "安装与维护", 36, 543, 220, 24, 14, bold=True)
        label(self.content, "仅通过 USB 操作。先备份，再检查；不兼容时停止。", 36, 581, 780, 22, 12, "muted")
        self.maintenance += [self.button("检查并备份", "backup:", 402, 536, 134, 32),
                             self.button("安装固件", "installFirmware:", 550, 536, 126, 32),
                             self.button("查看备份", "showBackups:", 690, 536, 126, 32)]
        rule(self.content, 36, 610, 792)
        label(self.content, "本机 Kaboo 数据  ·  私有配对  ·  不上传原始用量", 36, 622, 660, 18, 10, "muted")
        version = label(self.content, "v" + VERSION, 748, 621, 78, 18, 11, "muted")
        version.setFont_(A.NSFont.monospacedSystemFontOfSize_weight_(11, A.NSFontWeightRegular))
        version.setAlignment_(A.NSTextAlignmentRight)
        self.menus()
        self.show_preferences()
        self.refresh_(None)
        self.timer = F.NSTimer.scheduledTimerWithTimeInterval_target_selector_userInfo_repeats_(
            0.25, self, "refresh:", None, True)
        self.window.center()
        self.window.setInitialFirstResponder_(self.start_button)
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
        self.values[1].setFont_(A.NSFont.systemFontOfSize_weight_(22, A.NSFontWeightMedium))
        self.values[2].setStringValue_(str(last.get("days", "—")))
        self.last.setStringValue_(core.delivery_caption(last.get("at")))

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
                     "sending": ("正在送出这一餐", "等待胸牌确认收到，确认前不会显示送达。"),
                     "waiting": ("饭盒已送达", "在胸牌上按确定喂给伙伴，下一轮会自动同步。")}
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
            self.problem = False
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
            if self.quit_pending:
                # NSTerminateLater can suspend the regular UI timer. The worker
                # emits this only after export/radio cleanup has completed.
                self.quit_pending = False
                A.NSApp.replyToApplicationShouldTerminate_(True)

    def refresh_(self, _):
        busy = self.worker.busy
        title, enabled, mode, tone = core.feeding_control(busy, self.worker.kind,
                                                        self.worker.stop_requested.is_set(), self.problem)
        self.start_button.setTitle_(title)
        self.start_button.setEnabled_(enabled)
        self.mode.setStringValue_(mode)
        self.mode.setTextColor_(color(tone))
        self.mode_dot.layer().setBackgroundColor_(color(tone).CGColor())
        self.kaboo.setEditable_(not busy)
        self.kaboo.setSelectable_(True)
        self.kaboo.setToolTip_(str(self.kaboo.stringValue()))
        self.settings_note.setStringValue_("修改连接前，请先暂停送饭" if busy and self.worker.kind == "feed" else "操作结束后可修改连接" if busy else "配对与用量只保存在本机")
        for control in self.controls + self.maintenance:
            control.setEnabled_(not busy)
        self.tray.button().setTitle_("饭盒 · " + ("待重试" if busy and self.problem else "运行中" if busy else "暂停"))
        if self.quit_pending and not busy:
            self.quit_pending = False
            A.NSApp.replyToApplicationShouldTerminate_(True)

    def toggleFeeding_(self, _):
        if self.worker.busy:
            if self.worker.kind in ("feed", "source"):
                self.stop_(None)
        else:
            self.start_(None)

    def show_(self, _):
        # Dock/menu activation must not steal focus from a safety confirmation.
        window = A.NSApp.modalWindow() or self.window
        window.makeKeyAndOrderFront_(None)
        A.NSApp.activateIgnoringOtherApps_(True)

    def applicationShouldHandleReopen_hasVisibleWindows_(self, app, visible):
        if not visible:
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
