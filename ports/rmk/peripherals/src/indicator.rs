// SPDX-License-Identifier: MIT
use embassy_nrf::{gpio::Output, pwm::{SequencePwm, SequenceConfig, SingleSequencer, SingleSequenceMode}};
use embassy_time::{Instant, Timer};
use rmk::{event::CentralConnectedEvent, macros::processor};
use crate::indicator_state::{State, OFF, encode};

#[processor(subscribe = [CentralConnectedEvent], poll_interval = 100)]
pub struct ConnectionIndicator {
    pwm: Option<SequencePwm<'static>>,
    power: Output<'static>,
    state: State,
    shown: [u8; 3],
    powered: bool,
}

impl ConnectionIndicator {
    pub fn new(pwm: Option<SequencePwm<'static>>, power: Output<'static>) -> Self {
        if pwm.is_none() { defmt::warn!("Connection indicator PWM initialization failed"); }
        Self { pwm, power, state: State::new(Instant::now().as_millis()), shown: OFF, powered: false }
    }

    async fn on_central_connected_event(&mut self, event: CentralConnectedEvent) {
        self.state.connection(event.connected, Instant::now().as_millis());
        self.poll().await;
    }

    async fn poll(&mut self) {
        let Some(pwm) = self.pwm.as_mut() else { return; };
        let color = self.state.color(Instant::now().as_millis());
        if color == self.shown { return; }
        if !self.powered && color != OFF {
            self.power.set_high();
            self.powered = true;
            Timer::after_millis(50).await;
        }
        let frame = encode(color);
        let mut config = SequenceConfig::default();
        // 256 zero-duty periods = 320 us low after the 48 data bits.
        config.end_delay = 256;
        let sequencer = SingleSequencer::new(pwm, &frame, config);
        if sequencer.start(SingleSequenceMode::Times(1)).is_err() {
            defmt::warn!("Connection indicator PWM transfer failed");
            self.power.set_low();
            self.powered = false;
            self.shown = OFF;
            return;
        }
        // DMA frame + reset is under 400 us. Keep buffer and sequencer alive
        // for 1 ms; dropping the sequencer then stops PWM at a low level.
        Timer::after_millis(1).await;
        drop(sequencer);
        self.shown = color;
        if color == OFF {
            self.power.set_low();
            self.powered = false;
        }
    }
}
