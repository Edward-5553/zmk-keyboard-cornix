// SPDX-License-Identifier: MIT
use crate::render::View;

#[derive(Clone, Copy)]
pub struct Model {
    pub view: View, pub last_activity: u64, pub hint_at: u64,
    pub has_activity: bool, pub suspended: bool,
}
impl Model {
    pub const fn new() -> Self {
        Self {view: View::new(), last_activity: 0, hint_at: 0, has_activity: false, suspended: false}
    }
    pub fn ready(&self) -> bool {matches!(self.view.icon,1|2)}
    pub fn working(&self, now: u64) -> bool {self.ready() && self.has_activity && now.saturating_sub(self.last_activity)<3000}
    pub fn brightness(&self, now: u64) -> u8 {
        let idle=now.saturating_sub(self.last_activity);
        if self.suspended || idle>=120_000 {0} else if !self.ready() {5} else if idle>=60_000 {20} else {35}
    }
    pub fn visible(&self, now: u64) -> View {
        let mut v=self.view;
        if !self.ready() {v.wpm=0;}
        if !self.ready() || !v.connected.iter().all(|c| *c) || v.low.iter().any(|l| *l) || now.saturating_sub(self.hint_at)>=1200 {v.hint=0;}
        v
    }
    pub fn connected(&mut self, id: usize, connected: bool) {
        if id>=2 {return;}
        self.view.connected[id]=connected;
        if !connected {self.view.battery[id]=-1;self.view.low[id]=false;}
    }
    pub fn battery(&mut self, id: usize, value: Option<u8>) {
        if id>=2 || !self.view.connected[id] {return;}
        self.view.battery[id]=value.filter(|v| *v<=100).map_or(-1,|v|v as i16);
        let level=self.view.battery[id];
        if level<0 || level>=25 {self.view.low[id]=false;} else if level<20 {self.view.low[id]=true;}
    }
}
