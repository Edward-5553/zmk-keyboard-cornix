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

// Two GRB pixels, 5 SPI bits per WS2812 bit at 4 MHz (1.25 us).
// 0 = 10000 (0.25 us high); 1 = 11100 (0.75 us high).
// Trailing zero bytes hold DIN low for 320 us, including newer WS2812 resets.
pub fn encode(rgb: [u8; 3]) -> [u8; 190] {
    let mut frame = [0; 190];
    let mut index = 0;
    for value in [rgb[1], rgb[0], rgb[2], rgb[1], rgb[0], rgb[2]] {
        for bit in (0..8).rev() {
            let symbol = if value & (1 << bit) != 0 { 0b11100 } else { 0b10000 };
            for shift in (0..5).rev() {
                frame[index / 8] |= ((symbol >> shift) & 1) << (7 - index % 8);
                index += 1;
            }
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
            let mut symbol = 0;
            for i in bit*5..bit*5+5 {
                symbol = (symbol << 1) | ((frame[i/8] >> (7-i%8)) & 1);
            }
            assert!(symbol == 0b10000 || symbol == 0b11100);
            decoded[bit/8] = (decoded[bit/8] << 1) | u8::from(symbol == 0b11100);
        }
        assert_eq!(decoded, [0x34, 0x12, 0x56, 0x34, 0x12, 0x56]);
        assert!(frame[30..].iter().all(|&v| v == 0));
    }
}
