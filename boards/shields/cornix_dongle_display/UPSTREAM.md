# Cornix dongle display

Display widgets vendored from [englmaxi/zmk-dongle-display](https://github.com/englmaxi/zmk-dongle-display)
at commit `2bb333f87136d33e94a49d86236ed9ec254a8060` (MIT; see LICENSE).
This distinct `cornix_dongle_display` shield avoids relying on external shield
search order and keeps the customized animation in this repository.

Local changes:

- Rename the shield; retain upstream widget configuration symbols for compatibility.
- Replace Bongo Cat images with 32×32 Salary Cat indexed monochrome frames.
- Keep animation state per widget and initialize the image immediately.
- WPM < 5: sleeping still frame (one playback, no repeating animation).
- WPM 5–29: typing at 5 fps; 30–69: typing at 10 fps; 70+: snacking at 10 fps.
- Identical WPM ranges do not restart animation. Response follows ZMK's WPM
  sampling/decay, not individual key presses; holding a key need not increase WPM.
- Image bounds on a 128×64 screen: x=96–127, y=22–53. Leave the top 20 pixels
  for the two battery rows and the bottom 8 pixels for the layer label.
- Preserve output, battery, modifier, HID indicator, optional WPM and layer widgets.

Only rebuild/flash `cornix_dongle_nosd.uf2` for this display-only change.
No settings reset or half firmware update is needed for an already paired setup.

Rebuild images with Pillow:

```sh
python scripts/encode-dongle-cat.py --source-dir /path/to/original/gifs
```

Generated C images are committed; firmware builds do not need Pillow or a GIF decoder.
See ASSETS.md for the exact source files and artwork attribution.

Hardware checks: confirm both battery rows remain readable, the layer name does
not overlap the image, typing changes the animation and idle returns to sleep.
Recheck screen wake after idle, and USB input while the animation is playing.

Host validation: `python scripts/test-dongle-cat.py --cc gcc` (or a Zig executable).
It compiles the real widget and image arrays with host stubs, checking initial
rendering, WPM boundaries, stable animation, idle non-repetition and image metadata.
It does not replace a full Zephyr build or hardware display verification.
