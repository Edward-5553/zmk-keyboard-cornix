# Screen resources

`cat_work.bin` / `cat_sleep.bin` are RGB565 little-endian, 242 × 214 pixels,
sampled at 10 fps from `../../design/assets/*.gif`. They are mapped from Flash;
the device uses a 16-line DMA buffer, not a full animation in RAM.
Frame counts and SHA-256 hashes are recorded in `manifest.json`.

Artwork: **@月薪喵**, with source and usage restrictions in
[the existing project asset credits](../../../../boards/shields/cornix_dongle_display/ASSETS.md).
The artwork is not covered by the source-code MIT license.

`display_font.c` contains only the UI glyphs from **Noto Sans SC**, rasterized at
16 px / 4 bpp. Font source: [Google Fonts Noto Sans SC](https://github.com/google/fonts/tree/main/ofl/notosanssc).
The font and derived subset are distributed under the **SIL Open Font License 1.1**;
see the included `OFL.txt`. `font_codepoints.json` supports coverage checks.

To regenerate (Python + Pillow, from the repository root):

```sh
python ports/stopwatch/tools/encode-display-assets.py
python ports/stopwatch/tools/encode-display-font.py path/to/NotoSansSC.ttf
```

Normal firmware builds use committed resources and do not require Pillow or a font download.

The work source (`design/assets/work.gif`) is now `cat-sad.gif` from
[LLMPET](https://github.com/myunwang/LLMPET/blob/main/assets/cat/cat-sad.gif),
downloaded 2026-09-27. The original sleep GIF is unchanged. Both animations are
resampled to 10 fps and compiled into Flash (no network access on the device).
