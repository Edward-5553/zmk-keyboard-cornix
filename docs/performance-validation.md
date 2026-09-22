# Performance tuning and hardware validation

These changes target the qualified Zephyr 4.1 / ZMK `main` builds in `build.yaml`.
The settings backend and no-SoftDevice flash layout are unchanged. Do not reset
pairing just to apply these optimizations. Host checks do not measure firmware
latency, RAM usage, current draw or display bus reliability.

## 1. Update battery and modifier widgets only when state changes

Battery and modifier events compare their semantic state before submitting work
to the dedicated display queue. Battery snapshots still retain all sources when
events coalesce. Rendering skips battery rows that have not changed. Key events
never render LVGL objects directly.

Run `python scripts/test-dongle-battery.py --cc gcc` (or a Zig executable).
The test compiles the real battery and modifier widgets, covers both peripheral
orders, optional dongle battery, Windows/Mac modifiers, repeated events and
coalesced changes that return to their previous state.

On hardware, press a key on each half to learn L/R, type rapidly, hold/release
modifiers and verify both battery rows continue updating. Compare display flush
counts and input latency during rapid typing with the previous firmware.

## 2. Enable WPM only with its numeric widget

`CONFIG_ZMK_DONGLE_DISPLAY_WPM=y` selects `CONFIG_ZMK_WPM`. The default display
does not need WPM counting or its periodic work; Salary Cat uses physical key
timestamps independently. Other modules may still select WPM if they need it.

In the default dongle build, check that `.config` leaves `CONFIG_ZMK_WPM`
disabled unless another enabled feature requires it. With the numeric widget
enabled, check that both symbols are enabled and the value changes while typing.

## 3. Transfer OLED pages with TWIM at 400 kHz

The `cornix_dongle_eyelash` shield uses `nordic,nrf-twim` (EasyDMA) and
`I2C_BITRATE_FAST` (400 kHz). Both the concatenation and flash-copy limits are
129 bytes: the Zephyr 4.1 SH1106 driver sends at most one 128-byte page plus one
control byte per data transaction. TWIM shares the same RAM buffer for both
purposes. Changing the display controller/width requires revisiting this size.

References: [Zephyr 4.1 TWIM binding](https://github.com/zephyrproject-rtos/zephyr/blob/v4.1.0/dts/bindings/i2c/nordic%2Cnrf-twim.yaml)
and [SH1106 write implementation](https://github.com/zephyrproject-rtos/zephyr/blob/v4.1.0/drivers/display/ssd1306.c).

Check the generated `zephyr.dts` for the TWIM compatible, 400000 Hz clock and both
129-byte properties. Check `.config` for `CONFIG_I2C_NRFX_TWIM=y`. Test cold boot,
screen wake, long typing sessions and simultaneous modifier/layer animations.
Look for display corruption, missing frames or I2C errors. If the physical bus
is unreliable at 400 kHz, try `I2C_BITRATE_STANDARD` (100 kHz) while retaining DMA.
Measure actual flush time before attributing a latency improvement to this change.
