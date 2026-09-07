<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 图片资源（Images）

本目录存放项目可复用的图片资源，如 UI 图标、背景、RGB565 资源等。

## 如何使用

- 图片文件复制到本目录，并在本项目 `README.md` 记录分辨率、格式、用途与来源。
- 与固件集成时，参考 [`components/bsp/include/bsp_display.h`](../../components/bsp/include/bsp_display.h) 与相关示例分支的图片资源管线，转换为固件所需格式（如 RGB565 数组）。
- 图片资源占用 Flash 与内存，集成前请评估 ESP32-C3 无 PSRAM 的限制。

## 三条数码宝贝主线的同人像素素材

- 可编辑的代码原生素材源：[`tools/generate_digimon_sprites.mjs`](../../tools/generate_digimon_sprites.mjs)。
- 生成的固件素材：`digimon_sprites.c` / `digimon_sprites.h`，由 `main/pet_view.c` 使用。
- 亚古兽、加布兽、巴达兽三条主线，每条七种形态，另有丧尸暴龙兽和黑暗战斗暴龙兽两种分支；每种待机、进食、睡眠三个图片版本。32 × 28 RGB565A8，共 185,472 字节常量像素数据。关闭抗锯齿、精确放大三倍。开心和进化使用边界受控的叠加图形与位移，不申请可写全屏画布。
- 分支角色参考：[丧尸暴龙兽](https://digimon.net/reference_en/detail.php?directory_name=skullgreymon)、[黑暗战斗暴龙兽](https://digimon.net/reference_en/detail.php?directory_name=blackwargreymon)。成长和分支选择规则为本项目同人设定。
- 执行 `node tools/generate_digimon_sprites.mjs` 重新生成，加 `--check` 检查可复现性。正常固件构建直接使用已生成素材，不需要 Node。
- 手工编写的同人表现，未复制或提取官方精灵文件。角色参考：[亚古兽](https://digimon.net/reference_en/detail.php?directory_name=agumon)、[黑球兽](https://digimon.net/reference_en/detail.php?directory_name=botamon)、[滚球兽](https://digimon.net/reference_en/detail.php?directory_name=koromon)、[战斗暴龙兽](https://digimon.net/reference_en/detail.php?directory_name=wargreymon)。
- 完整主线参考：[万代进化指南](https://toy.bandai.co.jp/assets/vb-digitalmonster/pdf/digimon_startguide.pdf)、[东映巴达兽进化](https://www.toei-anim.co.jp/movie/digimon-adventure/tri/evolution/patamon.php)。素材序号通过目录映射，不是持久物种 ID；新增主线时需同步扩展源文件与素材数组维度。
- 数码宝贝角色权利仍属于相应权利人；代码许可证不授予角色权利。本项目是非官方同人实现，未获品牌背书，不声明角色素材已获公开再分发或商业使用授权。公开发布前需核查。

主机测试检查素材源哈希、透明度与边界，并核对真实 LVGL 三倍放大后的非透明像素；在 48 KB 图形内存配置下覆盖全部动作。
