// CI-only host tests: no hardware/toolchain dependencies, no local compilation needed.
mod assets;
mod render;
mod model;
use model::Model;
use render::{Canvas, View};

#[test]
fn status_power_and_disconnect() {
    let mut s=Model::new();
    assert_eq!(s.brightness(0),5);
    s.view.icon=1;s.has_activity=true;s.last_activity=1000;
    assert!(s.working(3999));assert!(!s.working(4000));
    assert_eq!(s.brightness(60999),35);assert_eq!(s.brightness(61000),20);
    assert_eq!(s.brightness(121000),0);
    s.suspended=true;assert_eq!(s.brightness(1001),0);
    s.connected(0,true);s.battery(0,None);assert_eq!(s.view.battery[0],-1);
    s.battery(0,Some(0));assert_eq!(s.view.battery[0],0);assert!(s.view.low[0]);
    s.battery(0,Some(22));assert!(s.view.low[0]);
    s.battery(0,Some(25));assert!(!s.view.low[0]);
    s.connected(0,false);assert_eq!(s.view.battery[0],-1);assert!(!s.view.low[0]);
    s.battery(0,Some(80));assert_eq!(s.view.battery[0],-1);
    s.connected(99,true);s.battery(99,Some(99));
}

#[test]
fn partial_redraw_matches_full_scene_and_generates_previews() {
    std::fs::create_dir_all("ui-preview").unwrap();
    let mut v=View::new();
    let states=[
        ("waiting",v,false),
        ("typing",{v.connected=[true,true];v.battery=[82,64];v.wpm=72;v.icon=1;v},true),
        ("layer",{v.layer=2;v.battery=[0,-1];v.low=[true,false];v},false),
        ("hint",{v.layer=0;v.battery=[82,64];v.low=[false,false];v.hint=1;v},true),
    ];
    for (name,v,working) in states {
        let mut full=vec![0;466*466*2];
        Canvas {pixels:&mut full,x:0,y:0,w:466,h:466}.scene(&v,working,3);
        for (x,y,w,h) in [(0,40,466,96),(42,374,382,62),(142,166,182,182)] {
            for top in (y..y+h).step_by(8) {
                let rows=8.min(y+h-top);
                let mut stripe=vec![0;w*rows*2];
                Canvas {pixels:&mut stripe,x:x as i32,y:top as i32,w:w as i32,h:rows as i32}.scene(&v,working,3);
                for row in 0..rows {
                    assert_eq!(&stripe[row*w*2..(row+1)*w*2],&full[((top+row)*466+x)*2..((top+row)*466+x+w)*2]);
                }
            }
        }
        let mut ppm=b"P6\n466 466\n255\n".to_vec();
        for pixel in full.chunks_exact(2) {
            let c=u16::from_be_bytes([pixel[0],pixel[1]]) as u32;
            ppm.extend_from_slice(&[((c>>11)*255/31) as u8,(((c>>5)&63)*255/63) as u8,((c&31)*255/31) as u8]);
        }
        std::fs::write(format!("ui-preview/{name}.ppm"),ppm).unwrap();
    }
}

#[test]
fn all_animation_rows_and_timing_are_valid() {
    for (data,ends,working) in [(assets::WORK,assets::WORK_ENDS,true),(assets::SLEEP,assets::SLEEP_ENDS,false)] {
        let base=(ends.len()*180+1)*4;
        for row in 0..ends.len()*180 {
            let start=u32::from_le_bytes(data[row*4..row*4+4].try_into().unwrap()) as usize;
            let end=u32::from_le_bytes(data[row*4+4..row*4+8].try_into().unwrap()) as usize;
            assert_eq!((end-start)%3,0);
            assert_eq!(data[base+start..base+end].chunks_exact(3).map(|p|p[0] as usize).sum::<usize>(),180);
        }
        for (i,&end) in ends.iter().enumerate() {assert_eq!(render::frame(working,end as u64-1),i);}
        assert_eq!(render::frame(working,*ends.last().unwrap() as u64),0);
    }
}
