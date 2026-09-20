// SPDX-License-Identifier: MIT
#include "orientation.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void settle(struct orientation *s,float degrees,uint32_t start) {
    float r=degrees*0.01745329252f;
    for(unsigned i=0;i<30;i++)orientation_update(s,sinf(r),-cosf(r),0,start+i*100);
}
int main(void) {
    struct orientation s={0};
    assert(!orientation_update(&s,0,-1,0,0));
    assert(!orientation_update(&s,0,-1,0,299));
    assert(orientation_update(&s,0,-1,0,300));
    assert(s.valid && s.angle==0);
    settle(&s,30,1000);assert(s.angle==0);
    settle(&s,90,5000);assert(fabsf(s.angle-90)<=1);
    settle(&s,180,9000);assert(fabsf(remainderf(s.angle-180,360))<=1);
    settle(&s,-90,13000);assert(s.angle==270);
    settle(&s,-44,16000);assert(s.angle==270); // boundary hysteresis
    float saved=s.angle;
    assert(!orientation_update(&s,0,0,1,17000) && s.angle==saved); // flat
    assert(!orientation_update(&s,3,0,0,18000) && s.angle==saved); // moving
    assert(!orientation_update(&s,NAN,0,0,18100) && s.angle==saved);
    settle(&s,179,19000);settle(&s,-179,23000);
    assert(s.angle==180); // short path through wrap
    s=(struct orientation){0};
    orientation_update(&s,0,-1,0,UINT32_MAX-100);
    assert(orientation_update(&s,0,-1,0,199));
    // Exact pixel mapping for all four directions and wrap equivalents.
    uint16_t src[49],out[51];for(int i=0;i<49;i++)src[i]=i+1;
    const float angles[]={0,90,180,270,-90,360};
    for(unsigned a=0;a<sizeof(angles)/sizeof(*angles);a++) {
        out[0]=out[50]=0xbeef;
        orientation_rows(src,out+1,7,7,0,3,angles[a]);
        orientation_rows(src,out+22,7,7,3,4,angles[a]);
        double r=angles[a]*0.0174532925199433,c=cos(r),sn=sin(r);
        for(int y=0;y<7;y++)for(int x=0;x<7;x++) {
            int ix=(int)floor(3+c*(x-3)+sn*(y-3)+0.5);
            int iy=(int)floor(3-sn*(x-3)+c*(y-3)+0.5);
            assert(out[1+y*7+x]==((ix>=0&&ix<7&&iy>=0&&iy<7)?src[iy*7+ix]:0));
        }
        assert(out[0]==0xbeef && out[50]==0xbeef);
    }
    orientation_rows(src,out+1,7,7,0,7,90);
    assert(out[1+0*7+3]==src[3*7+0]); // clockwise rotation: left centre -> top centre
    puts("Orientation: gravity, settling, flat/motion hold, wrap and pixel rotation passed");
}
