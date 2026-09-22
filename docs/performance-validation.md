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
