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

## 4. Schedule display ticks every 20 ms

`CONFIG_ZMK_DISPLAY_TICK_PERIOD_MS=20` reduces nominal periodic work submissions
from 100 to 50 per second while the display is active. This is not a claim of
halving CPU usage. The dedicated display queue and its 4096-byte stack are kept.
Salary Cat retains 10 fps playback, the 50 ms activity timer and the existing
3-second typing / 1-second inactivity thresholds. State changes become visible
at the next eligible display tick.

Run `python scripts/test-dongle-cat.py --cc gcc` (or a Zig executable) for the
animation state machine. On hardware, check modifiers, Num/Nav and Fn/Symbol
label scrolling, cat playback, idle blanking and screen wake while typing.
The host test does not simulate LVGL's timing or the OLED bus.

## 5. Reduce behavior delays and align debounce across roles

Space/Enter layer-taps retain the balanced flavor and 150 ms quick-tap window;
their tapping term changes from 200 to 180 ms. The three bracket-pair macros
use 20 ms tap and wait times instead of 30 ms. Encoder scroll pulse timing is
unchanged. These are behavior timings, not a measured end-to-end latency result.

The standard central left half now uses the same 3 ms press/release debounce
as the left-for-dongle and right peripherals. This shared board default applies
to qualified and legacy targets and remains overridable by user configuration.

After compilation, check both debounce symbols are 3 in each half's `.config`.
After flashing the central device, test quick Space/Enter taps, double taps,
held-repeat, rolling between thumbs and letters, and deliberate layer holds.
Test bracket macros over USB and BLE in the editors/input methods you use.
Watch for missing characters or unintended layer activation. On both halves,
test fast repeated presses and long holds for chatter; restore a larger debounce
value if your switches need it. Firmware keymap defaults may be superseded by
key bindings saved through Studio; do not clear Bluetooth bonds to change them.
