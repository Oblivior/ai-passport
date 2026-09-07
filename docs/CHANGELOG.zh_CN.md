<p align="right">
  <strong>简体中文</strong> · <a href="CHANGELOG.md">English</a>
</p>

# Changelog

## Unreleased

- 启动时通过 CRC 存档事务移除旧试玩机器人家族记录，保留当前伙伴和真正的数码宝贝归档；原版本存档不改写，清理失败可重启重试。家族页为空或仅一条记录时，确定返回首页；多条时才循环翻阅，页脚提示与操作一致。测试混合归档、迁移、写入失败、CRC 回退、幂等性及真实 LVGL 按键导航。

- 统一宠物各页标题和草地页脚操作提示；进化详情分别显示食物、陪伴进度条，图鉴标明形态位置，伙伴选择标明当前位置，补充空家族翻页及无线页返回提示。大 Token 数以明确标注“约”的单位缩写显示，同步数据仍保留精确值。不改按键和存档；使用真实 LVGL 检查数值边界、进度条数值、页脚清理、缺字及文字与宠物、进度条的避让。

- 首页以居中宠物为主视觉，将 Lv.0–Lv.6 紧凑等级牌与成长阶段并列，饭盒和成长信息收拢到底部，操作提示移至草地页脚；累计陪伴天数保留在进度详情页。装饰饭盘替换为下一形态条件进度（食物、天数达标比例各自封顶后取较低值），展示剩余条件与究极体完成状态；不改成长规则、存档和动画时序。

- 新增亚古兽、加布兽、巴达兽完整主线、确认领养数码蛋与伙伴之家。各自成长、共享饭盒，旧库存不计入领养前的陪伴天数；保留跨月图鉴解锁，逐只归档当月伙伴。新增固定物种 ID 的 v3 存档、只读 v1/v2 迁移、CRC 双槽、降级保护与过期按钮防误喂。测试提交边界、失败和重启恢复、三主线像素及真实选择、取消、翻页操作。

- 接入亚古兽七阶段主线，新增独立像素同人外观、进食和睡眠图及只读进化图鉴。保留 v2 成长、协议和旧试玩回忆；用量路线数据不再冒充数码宝贝物种选择。说明角色权利边界，并测试 LVGL 放大后的实际像素，而非只检查文字边界。

- 新增五格「今日饭盒」，显示来源日 Token 快照、下一份进度、每天封顶、旧食物及同步过期状态。新食物只提醒一次，不改存档、协议或奖励；补充阈值算法与真实 LVGL 页面回归。

- 汉化胸牌界面，覆盖宠物形态、饭盒、进化、路线、家族图鉴、连接状态及演示菜单。
  内置 OFL 授权的 14/20 像素精简字库，并检查缺字和页面边界；
  不改变已有存档、同步协议标识和成长规则。

- 有线和无线伴侣均可显式选择缓存加速的累计导出，保留完整导出兼容、隐私隔离及幂等食物额度。

- 新增三条本地食物强度进化路线、只读最终形态预览页、路线外观与图鉴身份；到 RANGER 时锁定，不改 v2 存档布局、成长门槛或既有 STATUS/SYNC。补充认证后的只读 ROUTE 查询；Bits 认证路线仍是后续独立工作。

- 冷启动蓝牙发现/连接超时最多重试三次并递增等待，兼容 Python 3.9 异步超时，避免持续同步进程退出；身份或认证失败仍停止本轮，不降级。

- 将常驻无线服务放到板级初始化之后，并在 macOS 扫描结果中同时核对设备标识和服务，兼容首次扫描时名称信息的可用性。

- 新增 USB 首次配对、AES-256-GCM 无线饭盒、防重放、桌面私有配对文件、Pet Link 状态页及同步来源提示；保留原宠物存档格式和有线备用方式。

- 初始化 ESP-IDF USB 接收驱动，使饭盒同步可以接收命令；伴侣持续同步期间保持串口连接，避免反复开关串口干扰设备，并重试启动握手。

- 将无限测试喂食替换为 Kaboo 本地用量驱动的 USB 饭盒，新增食物和活跃天数双重进化门槛、状态动作、下阶段条件及可翻阅家族记录。补充累计同步、本地跨月与非破坏性旧存档导入。Bits 和 BLE 接入仍待实现。

- 修复喂食动画读取尚未完成布局的 LVGL 坐标，导致宠物跳到标题上的问题；
  改为相对位移动画，连续中断跳跃也不会累积位置偏移，并增加真实 LVGL 9.5.0 回归测试。
- 将宠物阶段标题与动画角色拆分到固定像素标题牌，并精简电量文案，避免在 240x320
  屏幕上发生遮挡。
- 新增 Passport 离线 AI 宠物 MVP：包含 7 个原创成长阶段、按键喂食与月度结算、
  12 个月宠物家族图鉴、只睡觉不掉状态的低负担机制、电量显示，以及带 CRC 校验的
  NVS 双槽存档；补充阶段阈值、幂等、结算、跨月、损坏状态与计数饱和的主机测试。
- 将小程序 BLE 安装兼容提升为二创模板强制契约：固定保护 `cardid`/Recovery 分区，
  保留上键持续 5 秒进入 Recovery 的 bootloader hook，并在 CI 强制校验合并镜像结构、
  分区表 MD5/范围、3 MB 应用上限和保护分区数据不入包。
- 规定多应用发布的 Release 标题约定：tag 按 `v<版本>-<应用名>`（如 `v0.1.0-voice-keychain`）命名，让 Release 标题同时带版本与应用名；发布成功后核对标题，保证一眼扫 Release 列表就能区分是哪个应用。
- 新增发布后收尾流程：`issue-suggestions` skill 用于把用户反馈作为 issue 提交到上游项目；`experience-pr` skill 用于把可复用的开发经验作为文档 PR 提交；新增 `docs/experiences/` 目录保存单条经验文件；并配套 `project-completion`、`file-issues` 与经验索引文档。
- 精简仓库根目录：将 GitHub 可识别的社区治理文档迁入 `.github/`，将变更记录迁入 `docs/`，同步全部引用，并在仓库检查中加入根目录文档白名单。
- 全仓库文档语言规范：所有维护中的 Markdown 默认 `.md` 文件使用英文，简体中文使用配对的 `.zh_CN.md`，双方提供语言切换；静态检查会阻止缺失配对、缺失切换链接或英文默认页混入中文正文。
- AI 开发流程一期：精简按任务加载的上下文入口，统一本地/CI 验证脚本，新增 PR 自动构建与模板，并提交依赖锁文件以提高构建可复现性。
- PR 审查修复：GitHub Actions 固定到完整 commit SHA，构建与发布 job 按最小权限拆分，同步 checkout 关闭凭证持久化；补充 Feature Request / Usage Question issue 表单；启用并修正私密安全报告兜底说明；清理 README 路径、CI 触发条件与历史分支描述漂移。
- 语言规范变更：commit 标题、PR 标题与 body 由"默认中文"改为**使用英文**（`docs/contribution/commit-and-pr.md` 更新）；中文写作规范（全角标点）适用范围剔除 PR/MR 描述（`doc-conventions.md` 更新）。
- CI 构建改造：`build-firmware.yml` 显式传入 `SDKCONFIG_DEFAULTS=sdkconfig.defaults` 再 `idf.py build`，由 defaults 启用自定义分区表（`CONFIG_PARTITION_TABLE_CUSTOM=y`，文件名为 `partitions.csv`）；`CONFIG_ESPTOOLPY_HEADER_FLASHSIZE_UPDATE` 改为 `n`，再用 `idf.py merge-bin -o build/FoloToy-AI-Passport-full.bin` 合并可直刷完整固件；产物精简为仅 full.bin；`actions/cache` 升级到 v5 以消除 GitHub Actions Node.js 20 弃用警告；CI 文档同步更新。
- 合并上游 PR #6（wireless-low-power-demos）以解决 PR #4 冲突：引入无线/低功耗 demo（`main/demo_wifi.c`、`demo_ble.c`、`demo_radio.c`、`demo_low_power.c`）、`partitions.csv`（NVS/PHY/3 MB factory-app 分区）、`main/CMakeLists.txt`/`main.c`/`demo.h`/`sdkconfig.defaults` 更新；同步硬件指南的 Wi-Fi/BLE/低功耗章节；README 能力契约表补充 Wi-Fi/Bluetooth LE/Low power 三项（中英双语）。
- 提交规范补充：`docs/contribution/commit-and-pr.md` 明确 PR 标题与 commit 标题使用相同的 Conventional Commit 格式和英文祈使句，不用名词短语当标题。
- CI 与文档清理：`sync-main.yml` 移除 `test_mode` 残留模板注释；`docs/development/coding-conventions.md` 将「Redis TTL」条目泛化为「缓存组件」条目（当前固件无 TTL 约束需求，消除从模板带入的无关约定）。
- 补充通用规范（借鉴 Shinku）：`docs/contribution/doc-conventions.md` 新增中文全角标点规范（正文 `，`；`（`）`，代码/命令/路径保留英文原样）、凭证不入仓规范（token/密钥/私钥绝不入仓，提交前 git diff 扫描敏感前缀）、文件删除安全规范（删除走系统回收站，不用 rm -rf/git clean -fd）。
- 代码注释规范强化：`docs/development/coding-conventions.md` 补充完善注释要求——函数说明（用途/参数/返回值/副作用/线程上下文/内存所有权/初始化顺序）、变量说明（语义/取值范围/生命周期/同步要求）、逻辑注释（状态机/时序/寄存器/魔数依据），覆盖范围宁多勿少，中文注释保留英文技术术语。
- 文档去 AI 化：`docs/README.md` / `docs/README.zh_CN.md` 移除 AI 专属章节（Entry point、Source-of-truth、提需求格式、BSP 边界、Runtime invariants、验收交付格式、构建命令），README 只保留给人看的项目介绍、硬件能力契约、demo 案例与项目结构；构建命令章节删除（与 `docs/development/build-and-test.md` 重复）。
- 新增 `docs/development/agent-guide.md`：集中承载"AI 如何在本仓库工作"（上下文建立顺序、事实来源优先级、提需求格式、BSP 边界、运行时规则、交付格式），并链接 build-and-test 与硬件指南，不重复构建命令与验收矩阵。
- 同步更新索引：`AGENTS.md` 规则索引新增 agent-guide 条目；`docs/INDEX.md` 与 `docs/development/README.md` 新增 agent-guide 索引行。
- 文档补充：`docs/fork-guide.md` 说明「为什么根目录不放置 README」——根目录 README 预留给 fork 开发者自行放置（上游留空），fork 后可将自己的内容写入根目录 `README.md` 介绍 fork 后的项目；GitHub 显示优先级（根 README > docs/README.md）契合该预留意图。
- 分支合并：创建 `main-update` 分支（基于与上游一致的 main），将 `feature/repo-structure`、`ci/build-firmware`、`ci/sync-main` 三个分支合并进来，统一 docs 结构（CI 文档归入 `docs/development/`，workflow 文件随 ci 分支引入 `.github/workflows/`）；解决 development/software-design README 的 add/add 冲突。
- 合并后审查修复：`docs/INDEX.md` 补充 CI 文档索引；`docs/fork-guide.md` 修正 workflow 引用为 `.github/workflows/sync-main.yml`；`docs/README` 双语项目结构块补充 `.github/workflows/` 与 CI 文档说明。
- ci 分支 CI 文档路径调整：`ci/build-firmware` 的 `docs/software-design/CI-build-and-release.md` 与 `ci/sync-main` 的 `docs/software-design/CI-sync-main.md` 均移入各分支的 `docs/development/`（CI 属工程规范）；`docs/software-design/README.md` 保留为软件设计索引；feature 分支的 software-design 索引同步更新引用。
- fork 补充文档目录迁移：`assets/docs/` 移至 `docs/assets/`（文档素材归入 docs/ 更合理），新增 `docs/assets/.gitkeep` 空目录占位；同步更新 AGENTS.md / INDEX / doc-conventions / fork-guide 的路径引用。
- 文档结构调整：根目录不再放 README——上游英文 README 移入 `docs/README.md`、中文移入 `docs/README.zh_CN.md`（GitHub 从 docs/ 识别主 README）；原 `docs/README.md` 根总索引更名为 `docs/INDEX.md`；同步更新 AGENTS.md / CONTRIBUTING / SUPPORT / fork-guide / doc-conventions 的路径引用。
- 初始化项目文档：新增 `AGENTS.md`、`CLAUDE.md` 和 `CHANGELOG.md`。
- 仓库结构规整：上游英文 `README.md` 更名为 `README.en_US.md`，保留 `README.zh_CN.md`。
- 新增目录骨架：`docs/`（software-design / hardware-design）、`assets/`（fonts / images / music，各含 `README.md`）、`skills/`。
- 将上游硬件开发指南归位到 `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`。
- 文档规范：子目录 readme 统一为大写 `README.md`；补充 fork 用户约定（main 只动根 README）。
- 扩展 fork 用户约定：`main` 分支允许修改根目录 `README.md` 和 `assets/docs/`（README 不足以说明项目时存放补充文档与素材）。
- 新增 `assets/docs/` 目录约定：上游 main 只保留空目录 `.gitkeep`，内容文件仅存在于 fork；使用方法规范写入 AGENTS.md「给 fork 用户」约定。
- CI 文档迁移：`docs/software-design/CI.md` 从本分支移除，迁至 `ci/build-firmware` 分支并改名为 `docs/software-design/CI-build-and-release.md`。
- 补充 `main` 分支策略说明：解释 `main` 保持干净的两大原因（与上游同步无冲突 + 多小项目按分支整理）；例外——执意 main 开发需停用 CI 自动同步；提醒 fork 用户默认 action 关闭需手动启用（此条为整个 CI 的通用要求，统一写入 AGENTS.md）。
- 文档拆分：将 `AGENTS.md` 按主题拆为公共文档——新增 `docs/contribution/`（doc-conventions.md、commit-and-pr.md）与 `docs/development/`（build-and-test.md、coding-conventions.md），新增 `docs/fork-guide.md`；`AGENTS.md` 精简为简介 + 项目概述 + 必读文档索引。
- 同步更新索引：`docs/software-design/README.md`、`README.en_US.md` / `README.zh_CN.md` 的 `docs/` 目录说明。
- 参考 cindy 仓库文档组织完善索引：新增 `docs/README.md` 根总索引；AGENTS.md 规则索引按触发场景改写（附触发条件）；`docs/contribution/` 与 `docs/development/` 的 README 补充收录标准。
- 引入社区治理文档（参照 cindy 改写，放仓库根目录）：新增 `CONTRIBUTING.md` / `.zh_CN.md`（贡献指南，针对 ESP-IDF/AI agent/fork 场景改写）、`CODE_OF_CONDUCT.md` / `.zh_CN.md`（贡献者公约）、`SECURITY.md` / `.zh_CN.md`（安全报告流程）、`SUPPORT.md` / `.zh_CN.md`（支持渠道）；AGENTS.md 与 docs/README.md 同步引用。
