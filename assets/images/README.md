<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Images

Store reusable source images and generated display assets here.

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Three-line Digimon fan art

- Editable code-native source: [`tools/generate_digimon_sprites.mjs`](../../tools/generate_digimon_sprites.mjs).
- Generated firmware assets: `digimon_sprites.c` / `digimon_sprites.h`, consumed by `main/pet_view.c`.
- Agumon, Gabumon and Patamon lines, seven forms per line, three image variants
  (idle, eating, sleeping), plus SkullGreymon and BlackWarGreymon branch forms;
  32 x 28 RGB565A8, 185,472 bytes of constant pixel data.
  Render at exact 3x without antialiasing.
  Happy and evolution use bounded overlays/motion. No mutable full-screen canvas.
- Regenerate with `node tools/generate_digimon_sprites.mjs`; use `--check` to verify
  reproducibility. Ordinary firmware builds use committed assets without Node.
- Hand-authored fan depictions, not copied or extracted official sprite files.
  Character references: [Agumon](https://digimon.net/reference_en/detail.php?directory_name=agumon),
  [Botamon](https://digimon.net/reference_en/detail.php?directory_name=botamon),
  [Koromon](https://digimon.net/reference_en/detail.php?directory_name=koromon),
  [WarGreymon](https://digimon.net/reference_en/detail.php?directory_name=wargreymon),
  [SkullGreymon](https://digimon.net/reference_en/detail.php?directory_name=skullgreymon),
  [BlackWarGreymon](https://digimon.net/reference_en/detail.php?directory_name=blackwargreymon).
- Complete-line references: [Bandai evolution guide](https://toy.bandai.co.jp/assets/vb-digitalmonster/pdf/digimon_startguide.pdf)
  and [Toei Patamon evolution](https://www.toei-anim.co.jp/movie/digimon-adventure/tri/evolution/patamon.php).
  Artwork indices are catalog mappings, not persisted species IDs. Extend the
  source and array dimensions together when adding a lineage.
- Digimon character rights remain with the respective rights holders; the code
  license does not grant those rights. This is an unofficial fan implementation,
  not endorsed by the franchise. No public redistribution or commercial-use
  authorization is claimed for the character depictions. Review before publishing.

Host checks validate source hash, alpha/bounds and actual opaque pixels after
LVGL's 3x transform, as well as all poses in the 48 KB allocator configuration.
