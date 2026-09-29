// SPDX-License-Identifier: MIT
// Same 466x466 scene, coordinates, palette and assets as StopWatch display.c.
// A stripe renderer avoids an LVGL heap or a full framebuffer in internal RAM.
use crate::assets;

pub struct Glyph(pub u32, pub usize, pub i32, pub i32, pub i32, pub i32, pub i32, pub usize, pub usize);
pub struct Font {
    pub bitmap: &'static [u8], pub glyphs: &'static [Glyph], pub height: i32,
    pub baseline: i32, pub kern: &'static [u8], pub right_classes: usize,
}
impl Font {
    fn glyph(&self, c: char) -> Option<&Glyph> { self.glyphs.iter().find(|g| g.0 == c as u32) }
    fn advance(&self, g: &Glyph, next: Option<char>) -> i32 {
        let right = next.and_then(|c| self.glyph(c)).map_or(0, |n| n.8);
        let kern = if g.7 > 0 && right > 0 {
            self.kern[(g.7 - 1) * self.right_classes + right - 1] as i8 as i32
        } else { 0 };
        (g.2 + kern + 8) >> 4
    }
    fn width(&self, text: &str) -> i32 {
        let mut chars = text.chars().peekable();
        let mut width = 0;
        while let Some(c) = chars.next() {
            if let Some(g) = self.glyph(c) { width += self.advance(g, chars.peek().copied()); }
        }
        width
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct View {
    pub connected: [bool; 2], pub battery: [i16; 2], pub low: [bool; 2],
    pub layer: u8, pub wpm: u16, pub icon: u8, pub hint: u8,
}
impl View {
    pub const fn new() -> Self {
        Self { connected: [false; 2], battery: [-1; 2], low: [false; 2], layer: 0, wpm: 0, icon: 3, hint: 0 }
    }
}

pub const GREEN: u32 = 0x96d8aa;
const MUTED: u32 = 0x9daaa3;
const AMBER: u32 = 0xeda575;
pub const fn rgb565(c: u32) -> u16 {
    (((c >> 19) & 31) << 11 | ((c >> 10) & 63) << 5 | ((c >> 3) & 31)) as u16
}

pub struct Canvas<'a> { pub pixels: &'a mut [u8], pub x: i32, pub y: i32, pub w: i32, pub h: i32 }
impl Canvas<'_> {
    fn pixel(&mut self, x: i32, y: i32, c: u16) {
        if x < self.x || y < self.y || x >= self.x + self.w || y >= self.y + self.h { return; }
        let i = ((y - self.y) * self.w + x - self.x) as usize * 2;
        self.pixels[i..i+2].copy_from_slice(&c.to_be_bytes());
    }
    fn rect(&mut self, x: i32, y: i32, w: i32, h: i32, color: u32) {
        for py in y.max(self.y)..(y + h).min(self.y + self.h) {
            for px in x.max(self.x)..(x + w).min(self.x + self.w) { self.pixel(px, py, rgb565(color)); }
        }
    }
    fn rounded(&mut self, x: i32, y: i32, w: i32, h: i32, radius: i32, color: u32) {
        if w <= 0 { return; }
        let r = radius.min(w / 2).min(h / 2);
        for py in y.max(self.y)..(y+h).min(self.y+self.h) {
            for px in x.max(self.x)..(x+w).min(self.x+self.w) {
                let dx = if px < x+r { x+r-px } else { (px-(x+w-r-1)).max(0) };
                let dy = if py < y+r { y+r-py } else { (py-(y+h-r-1)).max(0) };
                if dx*dx + dy*dy <= r*r { self.pixel(px, py, rgb565(color)); }
            }
        }
    }
    fn line(&mut self, points: &[(i32,i32)], c: u32) {
        for pair in points.windows(2) {
            let (mut x, mut y) = pair[0]; let (x1,y1) = pair[1];
            let dx = (x1-x).abs(); let dy = -(y1-y).abs();
            let sx = if x<x1 {1} else {-1}; let sy = if y<y1 {1} else {-1}; let mut err=dx+dy;
            loop {
                self.rect(x+217, y+41, 2, 2, c);
                if x==x1 && y==y1 {break;}
                let e=2*err; if e>=dy {err+=dy;x+=sx;} if e<=dx {err+=dx;y+=sy;}
            }
        }
    }
    // align: 0 left, 1 center, 2 right. Glyph baseline/kerning match LVGL fonts.
    fn text(&mut self, text: &str, x: i32, y: i32, width: i32, font: &Font, align: u8, color: u32) {
        if y >= self.y+self.h || y+font.height <= self.y {return;}
        let mut pen=x+match align {1=>(width-font.width(text))/2, 2=>width-font.width(text), _=>0};
        let mut chars=text.chars().peekable();
        while let Some(c)=chars.next() {
            if let Some(g)=font.glyph(c) {
                let top=y+font.height-font.baseline-g.4-g.6;
                for gy in 0..g.4 {
                    if top+gy < self.y || top+gy >= self.y+self.h {continue;}
                    for gx in 0..g.3 {
                        let px=pen+g.5+gx;
                        if px<x || px>=x+width {continue;}
                        let bit=(gy*g.3+gx) as usize;
                        let b=font.bitmap[g.1+bit/2];
                        let a=if bit%2==0 {b>>4} else {b&15};
                        if a>0 {
                            let a=a as u32;
                            let mixed=(((color>>16)&255)*a/15)<<16 | (((color>>8)&255)*a/15)<<8 | (color&255)*a/15;
                            self.pixel(px,top+gy,rgb565(mixed));
                        }
                    }
                }
                pen+=font.advance(g,chars.peek().copied());
            }
        }
    }
    fn cat(&mut self, working: bool, frame: usize) {
        let (data, count) = if working {(assets::WORK,assets::WORK_ENDS.len())} else {(assets::SLEEP,assets::SLEEP_ENDS.len())};
        let base=(count*180+1)*4;
        for y in self.y.max(167)..(self.y+self.h).min(347) {
            let row=(frame%count)*180+(y-167) as usize;
            let offset=u32::from_le_bytes(data[row*4..row*4+4].try_into().unwrap()) as usize;
            let mut p=base+offset; let mut x=143;
            while x<323 {
                let n=data[p] as i32;
                let c=u16::from_le_bytes([data[p+1],data[p+2]]);p+=3;
                for px in x..x+n {self.pixel(px,y,c);}
                x+=n;
            }
        }
    }
    pub fn scene(&mut self, v: &View, working: bool, frame: usize) {
        self.pixels.fill(0);
        // Outer 434px ring centered on the circular panel.
        for y in self.y..self.y+self.h {
            for x in self.x..self.x+self.w {
                let d=(2*x-465)*(2*x-465)+(2*y-465)*(2*y-465);
                if (432*432..434*434).contains(&d) {self.pixel(x,y,rgb565(0x26372e));}
            }
        }
        match v.icon {
            1 => {
                for p in [&[(16,28),(16,5)][..], &[(12,9),(16,4),(20,9)], &[(16,21),(9,16),(9,12)], &[(16,17),(23,13),(23,9)]] {self.line(p,GREEN);}
                self.rounded(231,67,4,4,2,GREEN);
                self.rounded(224,49,4,4,2,GREEN);
                self.rect(238,46,4,4,GREEN);
            }
            2 => self.line(&[(9,9),(23,23),(16,29),(16,3),(23,9),(9,23)],0x83bfff),
            3 => {for i in 0..3 {self.rounded(222+i*9,55,4,4,2,MUTED);}}
            _ => {self.line(&[(10,10),(22,22)],MUTED);self.line(&[(22,10),(10,22)],MUTED);}
        }
        let mut digits=[0u8;8];
        self.text(number(v.wpm.min(999),false,&mut digits),173,104,68,&assets::LARGE,2,0xe7ece8);
        self.text("WPM",247,115,46,&assets::SMALL,0,MUTED);
        for side in 0..2 {
            let x=if side==0 {84} else {317};
            let color=if !v.connected[side] || v.low[side] {AMBER} else {GREEN};
            let mut digits=[0u8;8];
            let text=if !v.connected[side] {"未连接"} else if v.battery[side]<0 {"--%"} else {number(v.battery[side] as u16,true,&mut digits)};
            self.text(text,x,86,65,&assets::NOTO,1,color);
            self.rounded(x+5,116,55,12,6,0x395044);
            self.rounded(x+6,117,53,10,5,0x142219);
            let level=if v.connected[side] {v.battery[side].clamp(0,100) as i32} else {0};
            let w=53*level/100;
            self.rounded(if side==0 {x+6} else {x+59-w},117,w,10,5,color);
        }
        self.cat(working,frame);
        let names=["BASE","NUM / NAV","FN / SYMBOL","SCROLL","SNIPE"];
        self.text(names.get(v.layer as usize).copied().unwrap_or("CUSTOM"),43,374,380,&assets::LARGE,1,0xf1f4ed);
        let hints=["","音量 +","音量 -","向上滚动","向下滚动"];
        self.text(hints.get(v.hint as usize).copied().unwrap_or(""),63,412,340,&assets::NOTO,1,MUTED);
        for i in 0..5 {self.rounded(206+i*12,428,5,5,2,if i==v.layer as i32 {GREEN} else {0x334138});}
    }
}

fn number(mut n: u16, percent: bool, buf: &mut [u8;8]) -> &str {
    let mut i=buf.len();
    if percent {i-=1;buf[i]=b'%';}
    loop {i-=1;buf[i]=b'0'+(n%10) as u8;n/=10;if n==0 {break;}}
    core::str::from_utf8(&buf[i..]).unwrap()
}

pub fn frame(working: bool, elapsed: u64) -> usize {
    let ends=if working {assets::WORK_ENDS} else {assets::SLEEP_ENDS};
    let elapsed=(elapsed % *ends.last().unwrap() as u64) as u16;
    ends.iter().position(|&e| elapsed<e).unwrap_or(0)
}
