#include "cnfont.h"
#include <pspgu.h>
#include <pspkernel.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static uint32_t *codes;
static unsigned char *pixels;
static unsigned count,pages;
static char load_error[96];
const char *cnfont_error(void){return load_error;}
int cnfont_ready(void){return pixels!=0;}
static unsigned __attribute__((aligned(16))) palette[16];
typedef struct {float u,v;unsigned color;float x,y,z;} Vertex;
void cnfont_close(void) {free(codes);free(pixels);codes=0;pixels=0;count=pages=0;}
static int load_fail(FILE *f,const char *step,int code,unsigned actual,unsigned expected){
    snprintf(load_error,sizeof(load_error),"%s\nBYTES %u / %u",step,actual,expected);
    if(f)fclose(f);
    cnfont_close();errno=code;return 0;
}
static int read_font(FILE *f,void *out,unsigned bytes,const char *step){
    errno=0;unsigned got=(unsigned)fread(out,1,bytes,f);
    if(got!=bytes||ferror(f))return load_fail(f,step,errno?errno:EIO,got,bytes);
    return 1;
}
int cnfont_load(const char *path) {
    cnfont_close();load_error[0]=0;errno=0;
    FILE *f=fopen(path,"rb");if(!f)return load_fail(0,"FONT OPEN",errno,0,0);
    uint32_t h[4];if(!read_font(f,h,16,"FONT HEADER READ"))return 0;
    if(memcmp(h,"FNT2",4)||h[1]!=2||!h[2]||h[2]>4096||h[3]!=(h[2]+1023)/1024)
        return load_fail(f,"FONT HEADER FORMAT",EINVAL,16,16);
    count=h[2];pages=h[3];codes=malloc(count*4);
    if(!codes)return load_fail(f,"FONT CODES ALLOCATION",ENOMEM,0,count*4);
    pixels=memalign(16,pages*131072);
    if(!pixels)return load_fail(f,"FONT PIXELS ALLOCATION",ENOMEM,0,pages*131072);
    if(!read_font(f,codes,count*4,"FONT CODES READ"))return 0;
    if(!read_font(f,pixels,pages*131072,"FONT PIXELS READ"))return 0;
    errno=0;int extra=fgetc(f);
    if(ferror(f))return load_fail(f,"FONT EOF READ",errno?errno:EIO,0,0);
    if(extra!=EOF)return load_fail(f,"FONT TRAILING DATA",EINVAL,1,0);
    for(unsigned i=0;i<count;++i)if(codes[i]<32||codes[i]>0x10ffff||(i&&codes[i]<=codes[i-1]))
        return load_fail(f,"FONT GLYPH ORDER",EINVAL,i,count);
    fclose(f);
    for(unsigned i=0;i<16;++i)palette[i]=(i*17u<<24)|0xffffff;
    sceKernelDcacheWritebackAll();return 1;
}
static int lookup(uint32_t cp) {
    unsigned lo=0,hi=count;
    while(lo<hi) {unsigned mid=(lo+hi)/2;if(codes[mid]<cp)lo=mid+1;else hi=mid;}
    return lo<count&&codes[lo]==cp?(int)lo:-1;
}
static int render(float x,float y,const GlyphPosition *glyphs,unsigned n,unsigned color,float size,int smooth,int story) {
    if(!pixels)return 0;
    sceGuEnable(GU_TEXTURE_2D);sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
    sceGuClutMode(GU_PSM_8888,0,15,0);sceGuClutLoad(2,palette);
    sceGuTexMode(GU_PSM_T4,0,0,0);sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
    sceGuTexFilter(smooth?GU_LINEAR:GU_NEAREST,smooth?GU_LINEAR:GU_NEAREST);sceGuTexWrap(GU_CLAMP,GU_CLAMP);
    if(story)sceGuShadeModel(GU_SMOOTH);
    int page=-1;
    for(unsigned i=0;i<n;++i) {
        int index=lookup(glyphs[i].codepoint);if(index<0) {sceGuDisable(GU_TEXTURE_2D);return 0;}
        if(index/1024!=page) {page=index/1024;sceGuTexImage(0,512,512,512,pixels+page*131072);}
        float u=(index%32)*16,v=((index%1024)/32)*16;
        float gx=x+glyphs[i].column*size,gy=y+glyphs[i].row*(size+2);
        if(story){
            /* Reuse the alpha atlas: eight one-pixel offsets form a thin edge.
               One batched sprite draw plus a gradient quad per glyph; no new
               textures, readbacks or per-frame heap allocation. */
            static const signed char offsets[8][2]={{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}};
            Vertex *edge=sceGuGetMemory(16*sizeof(*edge));
            for(unsigned j=0;j<8;++j){float ex=gx+offsets[j][0],ey=gy+offsets[j][1];
                edge[j*2]=(Vertex){u,v,0xff503022,ex,ey,0};
                edge[j*2+1]=(Vertex){u+16,v+16,0xff503022,ex+size,ey+size,0};
            }
            sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,16,0,edge);
            Vertex *fill=sceGuGetMemory(4*sizeof(*fill));
            fill[0]=(Vertex){u,v,0xffffffff,gx,gy,0};
            fill[1]=(Vertex){u+16,v,0xffffffff,gx+size,gy,0};
            fill[2]=(Vertex){u,v+16,0xfff0c49a,gx,gy+size,0};
            fill[3]=(Vertex){u+16,v+16,0xfff0c49a,gx+size,gy+size,0};
            sceGuDrawArray(GU_TRIANGLE_STRIP,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,4,0,fill);
            continue;
        }
        Vertex *p=sceGuGetMemory(2*sizeof(*p));
        p[0]=(Vertex){u,v,color,gx,gy,0};p[1]=(Vertex){u+16,v+16,color,gx+size,gy+size,0};
        sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,p);
    }
    sceGuDisable(GU_TEXTURE_2D);return 1;
}
int cnfont_glyphs_smooth(float x,float y,const GlyphPosition *glyphs,unsigned n,unsigned color,float size){return render(x,y,glyphs,n,color,size,1,0);}
int cnfont_scaled(int x,int y,const GlyphPosition *glyphs,unsigned n,unsigned color,unsigned size){return render(x,y,glyphs,n,color,(float)size,0,0);}
int cnfont_story(int x,int y,const GlyphPosition *glyphs,unsigned n,unsigned size){return render(x,y,glyphs,n,0xffffffff,(float)size,0,1);}
int cnfont_speaker(int x,int y,const char *text){
    GlyphPosition glyphs[30];unsigned n;size_t next;
    return layout_page(text,0,30,1,0,glyphs,30,&n,&next)&&!text[next]&&cnfont_story(x,y,glyphs,n,16);
}
int cnfont_draw(int x,int y,const GlyphPosition *glyphs,unsigned n,unsigned color){return cnfont_scaled(x,y,glyphs,n,color,16);}
int cnfont_smooth(float x,float y,const char *text,unsigned color,float size){
    GlyphPosition glyphs[30];unsigned n;size_t next;
    return layout_page(text,0,30,1,0,glyphs,30,&n,&next)&&!text[next]&&render(x,y,glyphs,n,color,size,1,0);
}
int cnfont_line(int x,int y,const char *text,unsigned color) {
    GlyphPosition glyphs[30];unsigned n;size_t next;
    return layout_page(text,0,30,1,0,glyphs,30,&n,&next)&&!text[next]&&cnfont_draw(x,y,glyphs,n,color);
}
