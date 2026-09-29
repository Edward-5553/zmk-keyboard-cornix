// SPDX-License-Identifier: MIT
use embassy_nrf::{gpio::Output, spim::Spim};
use embassy_time::{Instant, Timer};
use rmk::{event::CentralConnectedEvent, macros::processor};
use crate::indicator_state::{State, OFF, encode};

embassy_nrf::bind_interrupts!(pub struct LedIrqs {
    SPIM3 => embassy_nrf::spim::InterruptHandler<embassy_nrf::peripherals::SPI3>;
});

#[processor(subscribe = [CentralConnectedEvent], poll_interval = 100)]
pub struct ConnectionIndicator {
    spi: Spim<'static>,
    power: Output<'static>,
    state: State,
    shown: [u8; 3],
    powered: bool,
}

impl ConnectionIndicator {
    pub fn new(spi: Spim<'static>, power: Output<'static>) -> Self {
        Self { spi, power, state: State::new(Instant::now().as_millis()), shown: OFF, powered: false }
    }

    async fn on_central_connected_event(&mut self, event: CentralConnectedEvent) {
        self.state.connection(event.connected, Instant::now().as_millis());
        self.poll().await;
    }

    async fn poll(&mut self) {
        let color = self.state.color(Instant::now().as_millis());
        if color == self.shown { return; }
        if !self.powered && color != OFF {
            self.power.set_high();
            self.powered = true;
            Timer::after_millis(50).await;
        }
        if self.spi.write(&encode(color)).await.is_err() {
            defmt::warn!("Connection indicator SPI write failed");
            self.power.set_low();
            self.powered = false;
            self.shown = OFF;
            return;
        }
        self.shown = color;
        if color == OFF {
            self.power.set_low();
            self.powered = false;
        }
    }
}
