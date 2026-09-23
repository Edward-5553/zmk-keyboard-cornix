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

## 6. Deep sleep only after two hours of inactivity

The earlier `3a866c2` optimization enabled deep sleep after 15 minutes on Cornix
left, left-for-dongle and right. This caused a potential responsiveness regression:
each battery-powered half could power off independently, and its next key first
had to wake the controller and reconnect BLE. The wake press and presses during
reconnection are not guaranteed to reach the host. A USB-powered dongle cannot
keep a battery-powered peripheral awake.

The current board defaults enable `CONFIG_ZMK_SLEEP=y` with
`CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=7200000` (two hours), as requested after reviewing
the wake behavior. Ordinary idle state, dongle screen blanking and RGB idle power
control remain enabled as before. Each half stays connected through normal
breaks, then may sleep independently after two hours without activity on that
half. Its first key after deep sleep still needs wake/reconnection; this change
postpones that transition rather than preserving wake presses. Battery use
between 15 minutes and two hours idle will be higher than with the old timeout;
no current draw or battery-life difference has been measured.

For dongle use, flash both `cornix_left_for_dongle_nosd.uf2` and
`cornix_right_nosd.uf2`. A dongle-only update cannot change the halves' sleep
configuration. For standard split use, update the standard left and right halves.
No settings reset, re-pairing or dongle update is required for this sleep change.

Check each normal half's resolved `.config` has `CONFIG_ZMK_SLEEP=y`,
`CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=7200000`, `CONFIG_NVS=y` and `CONFIG_SETTINGS_NVS=y`.
On battery power, test the first ordinary letter after 30 seconds, 5 minutes,
more than 15 minutes, and just under two hours idle. Then leave a half inactive
for over two hours and verify deep sleep and wake/reconnection. Test each half
independently, including leaving one idle while typing on the other. Check RGB
indications and repeat while the halves are USB-powered.

The matrix is a wakeup source, but encoder rotation is not a configured deep-sleep
wake source. ZMK prevents automatic deep sleep while USB power is detected. See
[ZMK power management configuration](https://zmk.dev/docs/config/power).

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
It retains the normal system/key-event work queue's 3072-byte stack; disabling
display would otherwise reduce that default to 2048 bytes.
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
   tied to those keys need a separate behavior check. Older half firmware enabled
   deep sleep after 15 minutes; the current default extends it to two hours (see section 6).
   A wake/reconnect pause differs from stutters during continuous typing. Apply
   any half firmware update before starting a new display comparison, then keep
   the half firmware fixed while comparing the two dongle builds.

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
