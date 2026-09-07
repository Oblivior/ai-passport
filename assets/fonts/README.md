<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Fonts

## Passport UI subset

- Files: `passport_zh_14.c` and `passport_zh_20.c`; 14/20 px, 4 bpp,
  uncompressed LVGL fonts. Their constant glyph data lives in Flash.
- Coverage: printable ASCII and the non-ASCII literals used by the built-in
  UI. This is not a complete Chinese font for arbitrary SSIDs or user input.
- Source: [Adobe Source Han Sans SC Normal](https://github.com/adobe-fonts/source-han-sans/tree/release/OTF/SimplifiedChinese),
  SHA-256 `7e5b7e586262f5e187c3397935b3a0dfbe28e08a916be0f60ec7fee2b50abd6d`.
- License: SIL Open Font License 1.1, copyright Adobe; see [LICENSE.txt](LICENSE.txt).
  The derived font is named Passport UI to avoid the reserved source font name.
- Integration: compiled by `main/CMakeLists.txt`; declared in `main/ui_pixel.h`.
  UI translations do not change saved enums or machine-readable protocol names.

## Regeneration

Obtain the source OTF from the linked upstream repository and install
`lv_font_conv@1.5.3` in a local tools directory. Run from the repository root:

```bash
node tools/generate_ui_fonts.mjs /path/to/SourceHanSansSC-Normal.otf /path/to/lv_font_conv/lv_font_conv.js
python3 tests/test_ui_fonts.py
```

The generator verifies the source hash and extracts UI literals automatically.
Commit both generated C files when adding copy. Normal firmware builds need
neither Node nor the source OTF; they compile the checked-in subsets.
The static gate checks coverage; headless LVGL tests check real glyph lookup,
all pet stages, label bounds and sibling-label overlap.
