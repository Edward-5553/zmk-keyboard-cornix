// SPDX-License-Identifier: MIT
#include "orientation.h"
#include <math.h>
#include <string.h>
static float delta(float a,float b) {return remainderf(a-b,360.0f);}
bool orientation_update(struct orientation *s,float x,float y,float z,uint32_t now) {
    float norm=x*x+y*y+z*z;
    if(!isfinite(norm) || norm<0.72f || norm>1.32f || x*x+y*y<0.09f) {
        s->filtered=false;s->pending=false;return false;
    }
    if(!s->filtered){s->x=x;s->y=y;s->z=z;s->filtered=true;}
    else {s->x+=(x-s->x)*0.35f;s->y+=(y-s->y)*0.35f;s->z+=(z-s->z)*0.35f;}
    if(s->x*s->x+s->y*s->y<0.09f){s->pending=false;return false;}
    float raw=atan2f(s->x,-s->y)*57.2957795f;
    float target=orientation_cardinal(raw);
    // Ten-degree hysteresis beyond a quadrant boundary; settle for 300 ms.
    if(s->valid && fabsf(delta(raw,s->angle))<55.0f)target=s->angle;
    if(!s->pending || target!=s->candidate) {
        s->candidate=target;s->since=now;s->pending=true;return false;
    }
    if((uint32_t)(now-s->since)<300)return false;
    if(s->valid && target==s->angle)return false;
    s->angle=target;s->valid=true;return true;
}
float orientation_cardinal(float angle) {
    int quarter=(int)lroundf(angle/90.0f);
    return ((quarter%4+4)%4)*90;
}

void orientation_rows(const uint16_t *src,uint16_t *dst,int w,int h,int row,int rows,float angle) {
    int turn=(int)orientation_cardinal(angle)/90;
    if(turn==0){memcpy(dst,src+row*w,(size_t)rows*w*sizeof(*dst));return;}
    // StopWatch is square: quarter turns need only exact integer indexing.
    for(int y=row;y<row+rows;y++)for(int x=0;x<w;x++) {
        int ix,iy;
        if(turn==1){ix=y;iy=h-1-x;}
        else if(turn==2){ix=w-1-x;iy=h-1-y;}
        else {ix=w-1-y;iy=x;}
        *dst++=(ix>=0&&ix<w&&iy>=0&&iy<h)?src[iy*w+ix]:0;
    }
}
