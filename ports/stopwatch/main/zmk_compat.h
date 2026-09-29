// SPDX-License-Identifier: MIT
#pragma once

// ZMK main: app/src/split/bluetooth/Kconfig (see ZMK_COMPATIBILITY.md).
// Interval: 1.25 ms units; latency: skipped idle events; timeout: 10 ms units.
#define SPLIT_INTERVAL 6
#define SPLIT_LATENCY 30
#define SPLIT_TIMEOUT 400

// config/cornix.keymap and ZMK sensor-rotate common binding defaults.
#define HOLD_TAP_TERM_MS 200
#define QUICK_TAP_MS 150
#define MACRO_TAP_MS 30
#define MACRO_WAIT_MS 30
#define SENSOR_TAP_MS 5
