#![no_std]
#![no_main]

use rmk::macros::rmk_peripheral;
mod indicator;
mod indicator_state;

#[rmk_peripheral(id = 1)]
mod keyboard_peripheral {
    #[register_processor(poll)]
    fn connection_indicator() -> crate::indicator::ConnectionIndicator {
        use embassy_nrf::{gpio::{Output, Level, OutputDrive}, pwm::{SequencePwm, Config, Prescaler}};
        let mut config = Config::default();
        config.prescaler = Prescaler::Div1;
        config.max_duty = 20;
        config.ch0_drive = OutputDrive::HighDrive;
        let power = Output::new(p.P0_24, Level::Low, OutputDrive::Standard);
        let pwm = SequencePwm::new_1ch(p.PWM0, p.P0_13, config).ok();
        crate::indicator::ConnectionIndicator::new(pwm, power)
    }
}
