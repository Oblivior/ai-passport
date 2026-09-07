<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 字库资源

## Passport UI 精简字库

- 文件：`passport_zh_14.c` 和 `passport_zh_20.c`；14/20 像素、4 bpp、
  未压缩的 LVGL 字库，字形常量存放于 Flash。
- 字符范围：可打印 ASCII 和内置界面使用的非 ASCII 字符；
  不是用于任意中文网络名称或用户输入的完整中文字库。
- 来源：[Adobe 思源黑体 SC Normal](https://github.com/adobe-fonts/source-han-sans/tree/release/OTF/SimplifiedChinese)，
  SHA-256：`7e5b7e586262f5e187c3397935b3a0dfbe28e08a916be0f60ec7fee2b50abd6d`。
- 授权：SIL Open Font License 1.1，Adobe 版权所有，见 [LICENSE.txt](LICENSE.txt)。
  派生字库命名为 Passport UI，避免使用原字库的保留名称。
- 集成：由 `main/CMakeLists.txt` 编译，在 `main/ui_pixel.h` 声明。
  汉化不改变存档枚举或供程序读取的协议名称。

## 重新生成

从上述官方仓库获取源 OTF，在本地工具目录安装 `lv_font_conv@1.5.3`。
从仓库根目录运行：

```bash
node tools/generate_ui_fonts.mjs /path/to/SourceHanSansSC-Normal.otf /path/to/lv_font_conv/lv_font_conv.js
python3 tests/test_ui_fonts.py
```

生成器校验源字体哈希并自动提取界面字面量。新增文案后应提交两份重新生成的 C 文件。
常规固件构建直接编译已提交的精简字库，不需要 Node 或源 OTF。
静态门禁检查字形覆盖；无界面 LVGL 测试检查实际字形查找、全部宠物阶段、
文字边界和同层标签重叠。
