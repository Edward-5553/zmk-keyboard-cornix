# Performance tuning and hardware validation

These changes target the qualified Zephyr 4.1 / ZMK `main` builds in `build.yaml`.
The settings backend and no-SoftDevice flash layout are unchanged. Do not reset
pairing just to apply these optimizations. Host checks do not measure firmware
latency, RAM usage, current draw or display bus reliability.

## 1. Update battery and modifier widgets only when state changes

Battery and modifier events compare their semantic state before submitting work
to the dedicated display queue. Battery snapshots still retain all sources when
events coalesce. Rendering skips battery rows that have not changed. Key events
never render LVGL objects directly. The later portrait redesign keeps L/R rows
fixed, with visible `--%` placeholders and bounded labels; see
[dongle portrait validation](dongle-portrait.md).
The compact header redesign disables modifier/lock widgets by default and uses
pre-scaled 64×64 cat frames. Test the legacy modifier widget only when explicitly
enabled; its previous change filtering remains available.

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
Salary Cat retains 10 fps playback, the 20 ms activity timer. A later UI change activates on the first physical
press and returns to idle 3 seconds after the last press. State changes become visible
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

## 6. Deep-sleep battery-powered halves after 15 minutes

Cornix left, left-for-dongle and right board defaults enable `CONFIG_ZMK_SLEEP`
with `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=900000`. Builds with
`CONFIG_ZMK_SETTINGS_RESET_ON_START=y` are excluded from these defaults.
The nice!nano dongle does not use the Cornix board defaults. ZMK's activity
manager prevents deep sleep while USB power is detected; the default indicator
shield already enables USB power detection on the peripherals.

The matrix already has `wakeup-source`. Wake a sleeping half with a matrix key;
encoder rotation is not configured as a deep-sleep wake source. Expect a wake
and Bluetooth reconnect delay, and do not assume the wake press is delivered as
normal input. Each peripheral tracks its own activity, so one half can sleep
while the other remains in use.

Check each normal half's `.config` for `CONFIG_ZMK_SLEEP=y`, timeout 900000 and
`CONFIG_PM_DEVICE=y`. Confirm settings-reset builds do not enable sleep through
this board default. For a shorter hardware trial, override the timeout to 60000
in a temporary build configuration, then restore 900000 for daily use.

Measure battery current before/after timeout. Wake each half independently and
both together; check reconnection, subsequent typing, RGB indicators and battery
reporting. Repeat while USB-powered to confirm it stays awake. If reliable
immediate input is more important than long-idle battery life, override with
`CONFIG_ZMK_SLEEP=n`.

## Intermittent dongle input latency: display A/B check

The dongle path is peripheral halves -> BLE (2.4 GHz) -> dongle -> USB HID.
USB output does not bypass the wireless connection from the halves.

Review of the `03b0794` build found no display rendering or I2C writes on the key
event path. The cat listener only records an activity timestamp under a short
spinlock; battery listeners copy state under a mutex and release it before any
LVGL work. Display writes run on the dedicated display queue. The TWIM driver
waits on a semaphore for DMA completion, allowing other threads to run.

Do not compare `CONFIG_BT_RX_PRIO=8` directly with the display priority of 5:
the Bluetooth host creates its receive queue with `K_PRIO_COOP(8)`, which resolves
to -8 with this build's 16 cooperative priorities. Bluetooth RX, system key
processing (-1), and the USB queue (-1) run ahead of the preemptible display queue
(5). The BLE command queue also uses 5. The display queue therefore does not
outrank Bluetooth RX, key processing or USB; this does not measure interrupt load
or end-to-end latency.

The 64x64 cat has four times the pixels of the earlier 32x32 cat at the same
10 fps. Rotation and decoding add display work; the polarity/180-degree fix does
not change the image dimensions, refresh rate, keymap or radio configuration.
Host rendering tests verify correctness, not responsiveness on the nRF52840.

`build.yaml` also produces `cornix_dongle_no_display_nosd.uf2` for diagnosis. It
uses the same board, adapter, keymap and no-SoftDevice/Studio snippets as the
normal dongle, omits both display shields, and explicitly disables ZMK display.
This is a comparison build, not a confirmed latency fix. Check its resolved
configuration: display/LVGL/OLED must be disabled, while USB HID, both BLE
peripherals, radio parameters, NVS and Studio remain consistent with the normal
dongle from the same Actions run.

1. Keep both halves' firmware, the USB port, dongle position and typing workload
   unchanged. First reproduce with that run's normal `cornix_dongle_nosd.uf2`.
2. Flash only the dongle with `cornix_dongle_no_display_nosd.uf2`, then unplug and
   reconnect USB. The power cycle clears any image retained by the OLED controller.
   Do not reset settings or re-pair either half.
3. Compare continuous ordinary letters on each half and alternating halves;
   separately test Space/Enter rolls and the first input after idle. Switch back
   to the normal firmware and repeat to check that any difference is reproducible.
4. If the no-display build also stutters, display work is less likely to explain
   it. Next test the same firmware with the dongle away from the metal chassis,
   keeping the USB port unchanged by using an extension cable if available.
   [ZMK documents metal enclosures as a wireless connection risk](https://zmk.dev/docs/troubleshooting/connection-issues#unreliableweak-connection).
5. Space/Enter have balanced layer-tap behavior with a 180 ms tapping term; pauses
   tied to those keys need a separate behavior check. Each half can also sleep
   after 15 minutes idle; a wake/reconnect pause is different from stutters during
   continuous typing. Do not change these settings during the display comparison.

Record which half, ordinary keys versus layer-taps, time since idle, firmware and
mounting position when a stall occurs. A successful build alone cannot establish
or exclude a hardware latency regression.

## Firmware set and final checks

For dongle mode, flash the newly built dongle, left-for-dongle and right firmware.
For a standard split, flash the standard left and right firmware. Settings-reset
firmware is not needed for this update. Retain qualified `//zmk` board names and
the existing no-SoftDevice layout.

Inspect the final `.config` files for `CONFIG_NVS=y`, `CONFIG_SETTINGS_NVS=y`
and the absence of `CONFIG_SETTINGS_NONE=y`. Keep `.config`, `zephyr.dts`, size
reports and the resolved west manifest with each build so comparisons use the
same dependency revisions. Compile-time/host checks must be followed by the
hardware checks above; no latency or battery-life improvement has been measured
by the host tests.
