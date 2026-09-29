# StopWatch UI resources

The RMK renderer preserves the scene in `ports/stopwatch/main/display.c`: 466x466
black background, green ring, transport icon, two battery bars, WPM, 180x180 cat,
layer label, encoder hints and five layer dots. It uses the same RGB565 animation
pixels, frame durations, Noto Sans SC glyphs and LVGL Montserrat 14/28 glyphs.
Native Rust primitives replace LVGL; primitive edge antialiasing may differ.

`work.rle` / `sleep.rle` are lossless per-row runs (count:u8, RGB565:u16 LE),
prefixed with `(frame_count * 180 + 1)` little-endian u32 row offsets. Only an
8-line internal-RAM stripe is used for rendering and asynchronous QSPI DMA.
All resource conversion is offline and checked against the original files:

```
python ports/rmk/tools/display_assets.py
python ports/rmk/tools/display_assets.py --check
```

Source fonts in `source/` are LVGL **v9.2.2** (matching the ESP-IDF port), from
https://github.com/lvgl/lvgl/tree/v9.2.2/src/font. Retain `source/LVGL-LICENSE.txt`.
The Noto subset retains the SIL OFL in `NotoSansSC-OFL.txt`. Generated font data
includes kerning from the Montserrat sources.

The cat artwork is **not MIT-licensed code**. Attribution and usage restrictions
remain those in [the original artwork notice](../../../../../boards/shields/cornix_dongle_display/ASSETS.md)
and [the StopWatch assets](../../../../stopwatch/main/assets/README.md).
The M5Stack panel initialization attribution is retained in
`M5Stack-NOTICES.md`. These notices accompany the firmware artifact.
