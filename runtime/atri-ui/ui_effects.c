#include "ui_effects.h"
#include "mixer.h"
#include <math.h>
void ui_blur_pixels(const uint32_t *src,uint32_t *out,uint32_t *scratch){
    for(unsigned y=0;y<68;++y)for(unsigned x=0;x<120;++x){
        unsigned r=0,g=0,b=0;
        for(unsigned dy=0;dy<4;++dy)for(unsigned dx=0;dx<4;++dx){uint32_t p=src[(y*4+dy)*512+x*4+dx];r+=p&255;g+=(p>>8)&255;b+=(p>>16)&255;}
        scratch[y*128+x]=0xff000000u|(r/16)|((g/16)<<8)|((b/16)<<16);
    }
    for(unsigned y=0;y<128;++y)for(unsigned x=0;x<128;++x){
        int cx=x<120?(int)x:119,cy=y<68?(int)y:67;unsigned r=0,g=0,b=0;
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){int xx=cx+dx,yy=cy+dy;xx=xx<0?0:xx>119?119:xx;yy=yy<0?0:yy>67?67:yy;
            uint32_t p=scratch[yy*128+xx];r+=p&255;g+=(p>>8)&255;b+=(p>>16)&255;}
        out[y*128+x]=0xff000000u|(r/9)|((g/9)<<8)|((b/9)<<16);
    }
}
void ui_cue_generate(int16_t samples[UI_CUE_FRAMES]){
    for(unsigned i=0;i<UI_CUE_FRAMES;++i){float t=(float)i/(UI_CUE_FRAMES-1),envelope=sinf(3.141592654f*t);envelope*=envelope;
        samples[i]=(int16_t)(1350*envelope*sinf(6.283185307f*880*i/44100.f));}
}
void ui_cue_mix(int32_t *stereo,unsigned frames,const int16_t *cue,unsigned *position,unsigned volume){
    if(volume>100)volume=100;
      for(unsigned i=0;i<frames&&*position<UI_CUE_FRAMES;++i,++*position){int sample=cue[*position]*(int)volume/MIX_DEFAULT_VOLUME;
        for(unsigned c=0;c<2;++c)stereo[i*2+c]+=sample;}
}
