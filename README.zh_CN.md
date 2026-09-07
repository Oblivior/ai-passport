<p align="right"><strong>简体中文</strong> · <a href="README.md">English</a></p>

# AI Pet Passport：每日饭盒

本 Fork 消费 Kaboo 本地 CSV 导出，通过 USB 给原创像素宠物投食。目前是可玩原型，尚非完整无线产品。

## 怎么玩

菜单打开 AI Pet。上下键切换首页、进化条件、家族。单击 OK 吃一份已获得的食物；空饭盒时摸摸或唤醒宠物，不增加成长。进食 1.2 秒，进化演出 1.8 秒。安静 30 秒后休息，不惩罚。家族页单击 OK 翻阅，长按 OK 返回硬件菜单。双击不再产生食物。

| 阶段 | 已吃食物 | 食物已吃的用量活跃天数 |
| --- | --- | --- |
| EGG | 0 | 0 |
| SPARK | 1 | 1 |
| BYTE | 3 | 2 |
| SCOUT | 6 | 3 |
| RANGER | 12 | 6 |
| TITAN | 24 | 10 |
| APEX | 40 | 16 |

两个条件同时满足才能进化。离线补吃按食物来源日计算天数，不按按钮日期。首次领养忽略之前历史。未领取食物保留至下次跨月同步，届时过期。

## 桌面伴侣

需要 Python 3.9+、`pyserial==3.5`，以及支持 `export --format csv` 的已安装 `kaboo-cli`。在本仓库运行，指定已识别的设备端口：

```bash
python3 tools/pet_companion.py --preview
python3 tools/pet_companion.py --port /dev/cu.usbmodem101
python3 tools/pet_companion.py --port /dev/cu.usbmodem101 --watch
```

默认同步一次；`--watch` 在进程运行期间每 300 秒同步，不安装自启服务，刷机前须停止。`--csv path/to/export.csv` 可指定本地导出。真实导出不得提交仓库。

首次正用量获得一份食物，此后每超过日目标的 20% 解锁下一份，每天最多五份。日目标取此前十四个活跃日的中位数，不含当天；没有历史时为 1,000,000。每月首次同步可用 `--goal` 指定，之后当月锁定。日期采用北京时间，本地覆盖范围不等于 Bits 官方月报。

伴侣关闭 CLI 自动更新、隔离扫描缓存，在内存处理 CSV，不保留原始导出、Prompt、项目名、主机名或凭据。CSV 消费端独立实现，没有复制内部 Kaboo 代码。设备只接收日期、日聚合 Token、目标和每日食物额度。缺失或无效数据停止同步，不伪造零值。

## 存储与传输

原 `ai_pet` 存档保持不动。新增 `ai_pet_v2` 使用 CRC 双槽，将旧宠物导入为 DEMO MEMORY，新玩法从蛋开始。家族可翻阅最近 12 条记录，更早的旧版记录仍在原存档。v2 双槽不可读时阻止写入，不静默重置进度。核对分区表后只更新 `0x10000` 应用区域；禁止全盘擦除，保留设备身份与 Recovery。

USB 是受信任本地线缆协议，不是远程认证 API，使用[现有非阻塞控制台](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32c3/api-guides/stdio.html)：

```text
PET2 STATUS
PET2 SYNC YYYYMMDD <tokens_today> <daily_goal> <31 digits, each 0..5>
```

每行最多 159 字符。拒绝无效或回退日期、未来日期食物、未知命令和月中目标变更。每天累计额度取最高值，重复同步或 ACK 丢失不重复发饭，上游修正不扣已获得的食物。NVS 提交成功后才 ACK；写入失败不改变生效状态。串口不提供吃饭命令，必须按实体按钮。

进入新月份后的第一次同步将旧宠物存为 LOCAL CHAPTER，并领取新蛋。这是本地跨月，不是 Bits 官方结算。离线时等待电脑时间，不自行假设新月份。

## 验证与边界

运行 `./tools/validate.sh --static` 和[构建与测试](docs/development/build-and-test.zh_CN.md)中的真实 LVGL 测试。页面测试输出 PPM 截图，覆盖布局、进食、进化、睡眠、快速按键和退出。纯逻辑测试覆盖三天成长、十六天进化门槛、重放、补发、日期校验、跨月和旧存档导入。

尚缺 BLE 同步、Bits 结算、独立 Flux 适配、可选物种、分支进化、相遇、音效和正式精灵素材。真机断电、电池续航和连续三天人工体验属于独立验收，主机模拟不能代替。
