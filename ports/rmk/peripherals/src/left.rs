#![no_std]
#![no_main]

use rmk::macros::rmk_peripheral;
mod indicator;
mod indicator_state;

#[rmk_peripheral(id = 0)]
mod keyboard_peripheral {
    #[register_processor(poll)]
    fn connection_indicator() -> crate::indicator::ConnectionIndicator {
        use embassy_nrf::{gpio::{Output, Level, OutputDrive}, spim::{Spim, Config, Frequency}};
        let mut config = Config::default();
        config.frequency = Frequency::M4;
        let power = Output::new(p.P0_13, Level::Low, OutputDrive::Standard);
        let spi = Spim::new_txonly_nosck(p.SPI3, crate::indicator::LedIrqs, p.P0_24, config);
        crate::indicator::ConnectionIndicator::new(spi, power)
    }
}
