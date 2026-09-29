// SPDX-License-Identifier: MIT
// Power/reset sequence ported from ports/stopwatch/main/display_hw.c.
// Copyright (c) 2026 M5Stack Technology CO LTD (initialization sequence).
use embassy_time::{Duration, Timer, with_timeout};
use esp_hal::{Async, i2c::master::I2c, spi::master::{DataMode, Address, Command, SpiDma}};

pub const WIDTH: usize = 466;
pub const HEIGHT: usize = 466;
pub const STRIPE_ROWS: usize = 8;
pub const I2C_HZ: u32 = 100_000;
pub const QSPI_HZ: u32 = 40_000_000;
const X_GAP: u16 = 6;
pub type Result<T> = core::result::Result<T, &'static str>;

pub struct Panel {
    pub i2c: I2c<'static, Async>,
    pub spi: SpiDma<'static, Async>,
    address: u8,
}

impl Panel {
    pub fn new(i2c: I2c<'static, Async>, spi: SpiDma<'static, Async>) -> Self {
        Self { i2c, spi, address: 0 }
    }

    async fn read(&mut self, address: u8, reg: u8) -> Result<u8> {
        let mut value = [0];
        with_timeout(Duration::from_millis(100), self.i2c.write_read_async(address, &[reg], &mut value))
            .await.map_err(|_| "IOE read timeout")?.map_err(|_| "IOE read")?;
        Ok(value[0])
    }

    async fn update(&mut self, reg: u8, clear: u8, set: u8) -> Result<()> {
        let value = self.read(self.address, reg).await?;
        with_timeout(Duration::from_millis(100), self.i2c.write_async(self.address, &[reg, (value & !clear) | set]))
            .await.map_err(|_| "IOE write timeout")?.map_err(|_| "IOE write")
    }

    async fn quiet_peripherals(&mut self) -> Result<()> {
        // IOE state can survive an ESP reset. PYG9 = motor (PWM1),
        // PYG10 = AW8737A enable (PWM4), PYG3 = codec/microphone power.
        // Disable PWM overrides before driving the motor and amplifier low.
        self.update(0x1c, 0x80, 0).await?;
        self.update(0x22, 0x80, 0).await?;
        self.update(0x06, 0x03, 0).await?;
        self.update(0x0a, 0x03, 0).await?;
        self.update(0x0c, 0x03, 0).await?;
        self.update(0x14, 0x03, 0).await?;
        self.update(0x04, 0, 0x03).await?;
        self.update(0x05, 0x04, 0).await?;
        self.update(0x09, 0x04, 0).await?;
        self.update(0x0b, 0x04, 0).await?;
        self.update(0x13, 0x04, 0).await?;
        self.update(0x03, 0, 0x04).await?;
        Ok(())
    }

    pub async fn init(&mut self) -> Result<()> {
        // A sleeping M5IOE1 may NACK its first transaction. Retry both official addresses.
        'probe: for address in [0x4f, 0x6f] {
            for _ in 0..3 {
                if self.read(address, 0x05).await.is_ok() {
                    self.address = address;
                    break 'probe;
                }
                Timer::after_millis(10).await;
            }
        }
        if self.address == 0 { return Err("M5IOE1 not found"); }
        esp_println::println!("LCD: IOE at {:#x}, I2C {} Hz", self.address, I2C_HZ);
        self.update(0x23, 0x0f, 0).await?;
        if let Err(error) = self.quiet_peripherals().await {
            esp_println::println!("IOE audio/motor shutdown failed: {}", error);
        }
        // Read-modify-write PYG5 (reset) and PYG8 (power); retain USB mux/charging.
        self.update(0x09, 0x90, 0).await?;
        self.update(0x0b, 0x90, 0).await?;
        self.update(0x13, 0x90, 0).await?;
        self.update(0x05, 0x10, 0x80).await?;
        self.update(0x03, 0, 0x90).await?;
        Timer::after_millis(80).await;
        self.update(0x05, 0, 0x90).await?;
        Timer::after_millis(150).await;
        if self.read(self.address, 0x05).await? & 0x90 != 0x90 {
            return Err("LCD power/reset readback");
        }
        self.command(0x36, &[0]).await?;
        self.command(0x3a, &[0x55]).await?; // RGB565, as esp_lcd_panel_init does.
        self.command(0x11, &[]).await?;
        Timer::after_millis(150).await;
        for (cmd, data) in [
            (0xc4, &[0x80][..]), (0x35, &[0x80]), (0x44, &[0x01, 0xd2]),
            (0x53, &[0x20]), (0x20, &[]), (0x36, &[0]), (0x51, &[0]), (0x29, &[]),
        ] { self.command(cmd, data).await?; }
        Ok(())
    }

    async fn transfer(&mut self, opcode: u16, cmd: u8, mode: DataMode, data: &[u8]) -> Result<()> {
        // CO5300: single-line opcode + 24-bit address (00, command, 00).
        // Parameters are single-line; pixel payload uses all four data pins.
        let result = with_timeout(Duration::from_millis(100), self.spi.half_duplex_write_async(
            mode, Command::_8Bit(opcode, DataMode::Single),
            Address::_24Bit((cmd as u32) << 8, DataMode::Single), 0, data,
        )).await;
        match result {
            Ok(Ok(())) => Ok(()),
            Ok(Err(error)) => {
                esp_println::println!("LCD command {:#04x}, {} bytes: {:?}", cmd, data.len(), error);
                Err("LCD SPI transfer")
            }
            Err(_) => {
                esp_println::println!("LCD command {:#04x}, {} bytes: timeout", cmd, data.len());
                Err("LCD DMA timeout")
            }
        }
    }

    pub async fn command(&mut self, cmd: u8, data: &[u8]) -> Result<()> {
        self.transfer(0x02, cmd, DataMode::Single, data).await
    }

    pub async fn draw(&mut self, x: u16, y: u16, w: u16, h: u16, pixels: &[u8]) -> Result<()> {
        if w == 0 || h == 0 || x as usize + w as usize > WIDTH || y as usize + h as usize > HEIGHT
            || pixels.len() != w as usize * h as usize * 2 { return Err("LCD bounds"); }
        let x0 = x + X_GAP;
        let x1 = x0 + w - 1;
        let y1 = y + h - 1;
        self.command(0x2a, &[(x0 >> 8) as u8, x0 as u8, (x1 >> 8) as u8, x1 as u8]).await?;
        self.command(0x2b, &[(y >> 8) as u8, y as u8, (y1 >> 8) as u8, y1 as u8]).await?;
        self.transfer(0x32, 0x2c, DataMode::Quad, pixels).await
    }

    pub async fn brightness(&mut self, percent: u8) -> Result<()> {
        self.command(0x51, &[(percent.min(100) as u16 * 255 / 100) as u8]).await
    }
}
