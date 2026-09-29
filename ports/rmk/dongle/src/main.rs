#![no_std]
#![no_main]

use rmk::macros::rmk_central;

mod assets;
mod model;
mod panel;
mod render;
mod screen;

#[rmk_central]
mod keyboard_central {
    #[register_processor(event)]
    fn screen_events() -> crate::screen::ScreenEvents {
        use esp_hal::{i2c::master::{I2c, Config as I2cConfig}, spi::master::{Spi, Config as SpiConfig}, time::Rate};
        // The IO expander requires standard-mode timing. Do not use 400 kHz here.
        let i2c=I2c::new(p.I2C0,I2cConfig::default().with_frequency(Rate::from_hz(crate::panel::I2C_HZ)));
        let spi=Spi::new(p.SPI2,SpiConfig::default().with_frequency(Rate::from_hz(crate::panel::QSPI_HZ)));
        match (i2c,spi) {
            (Ok(i2c),Ok(spi))=>{
                let i2c=i2c.with_sda(p.GPIO47).with_scl(p.GPIO48).into_async();
                let spi=spi.with_sck(p.GPIO40).with_sio0(p.GPIO41).with_sio1(p.GPIO42)
                    .with_sio2(p.GPIO46).with_sio3(p.GPIO45).with_cs(p.GPIO39)
                    .with_dma(p.DMA_CH0).into_async();
                _s.spawn(crate::screen::display_task(crate::panel::Panel::new(i2c,spi)).unwrap());
            }
            _=>esp_println::println!("LCD bus configuration failed; keyboard input continues"),
        }
        crate::screen::ScreenEvents
    }

    #[register_processor(poll)]
    fn wpm() -> rmk::processor::builtin::wpm::WpmProcessor {
        rmk::processor::builtin::wpm::WpmProcessor::new()
    }
}
