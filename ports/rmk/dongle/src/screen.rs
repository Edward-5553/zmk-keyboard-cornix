// SPDX-License-Identifier: MIT
use core::cell::Cell;
use embassy_sync::blocking_mutex::{Mutex, raw::CriticalSectionRawMutex};
use embassy_time::{Instant, Timer};
use rmk::{event::*, macros::processor, types::{battery::BatteryStatus, connection::UsbState, action::Action, keycode::{KeyCode, HidKeyCode, ConsumerKey}}};
use crate::{model::Model, panel::{Panel, Result, WIDTH, HEIGHT, STRIPE_ROWS}, render::{self, Canvas, View}};

static STATE: Mutex<CriticalSectionRawMutex, Cell<Model>>=Mutex::new(Cell::new(Model::new()));
fn change(f: impl FnOnce(&mut Model)) {STATE.lock(|c| {let mut s=c.get();f(&mut s);c.set(s);});}
fn now() -> u64 {Instant::now().as_millis()}

// Event handlers only update a small snapshot. Never await a display transfer here.
#[processor(subscribe = [KeyboardEvent, ConnectionStatusChangeEvent, PeripheralConnectedEvent, PeripheralBatteryEvent, LayerChangeEvent, WpmUpdateEvent, ActionEvent])]
pub struct ScreenEvents;
impl ScreenEvents {
    async fn on_keyboard_event(&mut self, e: KeyboardEvent) {
        if e.pressed && matches!(e.pos, KeyboardEventPos::Key(_) | KeyboardEventPos::RotaryEncoder(_)) {
            change(|s| {s.last_activity=now();s.has_activity=true;});
        }
    }
    async fn on_connection_status_change_event(&mut self, e: ConnectionStatusChangeEvent) {
        change(|s| {
            let old=s.view.icon;
            s.suspended=e.0.usb==UsbState::Suspended && e.0.decide_active()==Some(ConnectionType::Usb);
            s.view.icon=if s.suspended {0} else {match e.0.decide_active() {Some(ConnectionType::Usb)=>1,Some(ConnectionType::Ble)=>2,None=>3}};
            if old!=s.view.icon {s.view.wpm=0;if s.ready() {s.last_activity=now();s.has_activity=false;}}
        });
    }
    async fn on_peripheral_connected_event(&mut self, e: PeripheralConnectedEvent) {change(|s| s.connected(e.id,e.connected));}
    async fn on_peripheral_battery_event(&mut self, e: PeripheralBatteryEvent) {
        let level=match e.state.0 {BatteryStatus::Available {level,..}=>level,_=>None};
        change(|s| s.battery(e.id,level));
    }
    async fn on_layer_change_event(&mut self, e: LayerChangeEvent) {change(|s| s.view.layer=e.0);}
    async fn on_wpm_update_event(&mut self, e: WpmUpdateEvent) {change(|s| s.view.wpm=e.0);}
    async fn on_action_event(&mut self, e: ActionEvent) {
        if !e.keyboard_event.pressed {return;}
        let hint=match e.action {
            Action::Key(KeyCode::Hid(HidKeyCode::AudioVolUp)) | Action::Key(KeyCode::Consumer(ConsumerKey::VolumeIncrement))=>1,
            Action::Key(KeyCode::Hid(HidKeyCode::AudioVolDown)) | Action::Key(KeyCode::Consumer(ConsumerKey::VolumeDecrement))=>2,
            Action::Key(KeyCode::Hid(HidKeyCode::MouseWheelUp))=>3,
            Action::Key(KeyCode::Hid(HidKeyCode::MouseWheelDown))=>4,
            _=>0,
        };
        if hint!=0 {change(|s| {s.view.hint=hint;s.hint_at=now();});}
    }
}

#[repr(C, align(4))]
struct Stripe([u8; WIDTH*STRIPE_ROWS*2]);

async fn region(panel: &mut Panel, buffer: &mut Stripe, rect: (usize,usize,usize,usize), v: &View, working: bool, frame: usize) -> Result<()> {
    let (x,y,w,h)=rect;
    for row in (y..y+h).step_by(STRIPE_ROWS) {
        let rows=STRIPE_ROWS.min(y+h-row);
        let pixels=&mut buffer.0[..w*rows*2];
        Canvas {pixels,x:x as i32,y:row as i32,w:w as i32,h:rows as i32}.scene(v,working,frame);
        panel.draw(x as u16,row as u16,w as u16,rows as u16,pixels).await?;
        // Bounded CPU work per poll even when a DMA completion is immediately ready.
        embassy_futures::yield_now().await;
    }
    Ok(())
}

async fn run(panel: &mut Panel, buffer: &mut Stripe) -> Result<()> {
    panel.init().await?;
    let mut previous=None; let mut last_frame=usize::MAX; let mut was_working=false;
    let mut animation_start=now(); let mut brightness=0; let mut off=false;
    loop {
        let now=now();
        let state=STATE.lock(|s|s.get());
        let target=state.brightness(now);
        if target==0 {
            if !off {panel.command(0x28,&[]).await?;off=true;}
            Timer::after_millis(100).await;
            continue;
        }
        let v=state.visible(now);
        let working=state.working(now);
        if working!=was_working || off {animation_start=now;last_frame=usize::MAX;was_working=working;}
        let frame=render::frame(working,now-animation_start);
        if previous.is_none() || off {
            panel.brightness(0).await?;
            region(panel,buffer,(0,0,WIDTH,HEIGHT),&v,working,frame).await?;
            esp_println::println!("LCD: full frame drawn");
            if off {panel.command(0x29,&[]).await?;off=false;}
            brightness=0;
        } else {
            if previous!=Some(v) {
                region(panel,buffer,(0,40,WIDTH,96),&v,working,frame).await?;
                region(panel,buffer,(42,374,382,62),&v,working,frame).await?;
            }
            if frame!=last_frame {region(panel,buffer,(142,166,182,182),&v,working,frame).await?;}
        }
        if target!=brightness {panel.brightness(target).await?;brightness=target;}
        previous=Some(v);last_frame=frame;
        Timer::after_millis(20).await;
    }
}

#[embassy_executor::task]
pub async fn display_task(mut panel: Panel) {
    esp_println::println!("LCD: display task running");
    let mut buffer=Stripe([0;WIDTH*STRIPE_ROWS*2]);
    if let Err(error)=run(&mut panel,&mut buffer).await {
        esp_println::println!("LCD disabled: {}; keyboard BLE/USB remain active",error);
        #[cfg(feature = "diagnostic-usb")]
        loop {
            Timer::after_secs(3).await;
            esp_println::println!("LCD diagnostic failure: {}",error);
        }
    }
}
