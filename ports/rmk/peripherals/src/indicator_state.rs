// SPDX-License-Identifier: MIT
pub const OFF: [u8; 3] = [0, 0, 0];
pub const BLUE: [u8; 3] = [0, 0, 24];
pub const GREEN: [u8; 3] = [0, 24, 0];

pub struct State {
    connected: bool,
    changed_at: u64,
}

impl State {
    pub fn new(now: u64) -> Self { Self { connected: false, changed_at: now } }

    pub fn connection(&mut self, connected: bool, now: u64) {
        if self.connected != connected {
            self.connected = connected;
            self.changed_at = now;
        }
    }

    pub fn color(&self, now: u64) -> [u8; 3] {
        let elapsed = now.saturating_sub(self.changed_at);
        if self.connected {
            if elapsed < 2000 { GREEN } else { OFF }
        } else if elapsed % 3000 < 300 { BLUE } else { OFF }
    }
}

// PWM runs at 16 MHz with a 20-tick period (1.25 us).
// Falling-edge polarity starts high; 6/13 ticks give 375/812.5 ns pulses.
// The final zero-duty word is held low by the driver's reset end delay.
pub fn encode(rgb: [u8; 3]) -> [u16; 49] {
    let mut frame = [0x8000; 49];
    let mut index = 0;
    for value in [rgb[1], rgb[0], rgb[2], rgb[1], rgb[0], rgb[2]] {
        for bit in (0..8).rev() {
            frame[index] = 0x8000 | if value & (1 << bit) != 0 { 13 } else { 6 };
            index += 1;
        }
    }
    frame
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn connection_transitions_and_duplicate_events() {
        let mut state = State::new(100);
        assert_eq!(state.color(100), BLUE);
        assert_eq!(state.color(400), OFF);
        assert_eq!(state.color(3100), BLUE);
        state.connection(true, 3200);
        assert_eq!(state.color(3200), GREEN);
        state.connection(true, 5100);
        assert_eq!(state.color(5200), OFF);
        state.connection(false, 6000);
        assert_eq!(state.color(6000), BLUE);
        assert_eq!(state.color(6300), OFF);
        assert_eq!(state.color(9000), BLUE);
    }
    #[test]
    fn wire_format_is_two_grb_pixels_with_reset() {
        let frame = encode([0x12, 0x34, 0x56]);
        let mut decoded = [0u8; 6];
        for bit in 0..48 {
            let word = frame[bit];
            assert!(word == 0x8006 || word == 0x800d);
            decoded[bit/8] = (decoded[bit/8] << 1) | u8::from(word == 0x800d);
        }
        assert_eq!(decoded, [0x34, 0x12, 0x56, 0x34, 0x12, 0x56]);
        assert_eq!(frame[48], 0x8000);
    }
}
