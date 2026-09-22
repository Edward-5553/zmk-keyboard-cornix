# Cornix dongle display

Display widgets vendored from [englmaxi/zmk-dongle-display](https://github.com/englmaxi/zmk-dongle-display)
at commit `2bb333f87136d33e94a49d86236ed9ec254a8060` (MIT; see LICENSE).
This distinct `cornix_dongle_display` shield avoids relying on external shield
search order and keeps the customized animation in this repository.

Local changes:

- Rename the shield; retain upstream widget configuration symbols for compatibility.
- Replace Bongo Cat images with 32×32 Salary Cat indexed monochrome frames.
- Keep animation state per widget and initialize the image immediately.
- Default: loop `cat-idle.gif` at 10 fps (3 frames).
- After physical key-down events span at least 3000 ms, with every gap below
  1000 ms, loop `cat-error.gif` at 10 fps (4 frames).
- A gap of 1000 ms returns to idle and resets the streak, even if display work
  is delayed. Releases, host key repeat and encoder rotation do not extend it.
  Key presses from both halves, including modifier/layer keys, count together.
- A shared LVGL timer checks activity every 50 ms while the display is running.
  Event callbacks only record timestamps under a spinlock; all LVGL calls stay
  on the display thread. Repeated states do not restart the current animation.
  WPM is selected only by the optional numeric widget; it does not select animations.
- Image bounds on a 128×64 screen: x=96–127, y=22–53. Leave the top 20 pixels
  for the two battery rows and the bottom 8 pixels for the layer label.
- Preserve output, battery, modifier, HID indicator, optional WPM and layer widgets.
- Prefix each peripheral battery percentage with L/R, learned from that source's
  physical key position on the selected Cornix layout (left x <= 600, right x >= 750).
  Unknown sources show `?` until a normal key is pressed on that half after dongle
  startup. Rows retain their paired source order; L is not necessarily the top row.
  Key remapping does not affect detection. Turning an encoder alone does not identify
  a half. No learned mapping is written to flash. Optional dongle battery uses `D`.
- Keep all battery sources in each display snapshot so coalesced updates retain
  both halves' readings and side labels. Labels keep the original six-character width.
- Compare battery and modifier state before queuing display work. Ordinary key
  presses/releases no longer queue updates when the displayed state is unchanged.
  Battery rendering compares each row with its last rendered state; an update to
  one half does not redraw the other. All LVGL calls remain on the display thread.

Only rebuild/flash `cornix_dongle_nosd.uf2` for this display-only change.
No settings reset or half firmware update is needed for an already paired setup.

Rebuild images with Pillow:

```sh
python scripts/encode-dongle-cat.py --source-dir /path/to/original/gifs
```

Generated C images are committed; firmware builds do not need Pillow or a GIF decoder.
See ASSETS.md for the exact source files and artwork attribution.

Hardware checks: confirm both battery rows remain readable, the layer name does
not overlap the image, continuous typing for 3 seconds changes to error and a
1-second pause resumes idle. Short bursts must keep playing idle.
Recheck screen wake after idle, and USB input while the animation is playing.

Host validation: `python scripts/test-dongle-cat.py --cc gcc` (or a Zig executable).
It compiles the real widget and image arrays with host stubs, checking initial
rendering, 3-second/1-second boundaries, interrupted bursts, held keys, both halves,
64-bit uptime, stable looping in both states and image metadata.
It does not replace a full Zephyr build or hardware display verification.

Battery host validation: `python scripts/test-dongle-battery.py --cc gcc`.
This compiles the real widget with host stubs and checks both source orders,
unknown-to-L/R identification, unchanged-event filtering, per-row redraws,
coalesced A/B/A updates, invalid events and the optional dongle battery.
On hardware, press a normal key on each half after dongle
startup and verify its row changes from `?` to the correct letter without overlap.
