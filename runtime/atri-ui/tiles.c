#include "tiles.h"
#include "trace.h"
#include <pspgu.h>
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include "render_alloc.h"
#include <string.h>
#include <math.h>
#include <zlib.h>
#include <errno.h>
#define CACHE 64
#define HANDLES 24
#define OPEN_IMAGES 2
typedef struct {unsigned w,h,cols,rows,first;} Level;
typedef struct {unsigned offset,length;} Tile;
typedef struct {FILE *file;unsigned id,w,h,levels,count,format,used;Level level[3];Tile *tiles;} ImageFile;
typedef struct {unsigned image,tile,used,style;unsigned char *pixels;int valid,pending;} Cached;
static ImageFile files[HANDLES];
static Cached cache[CACHE];
static char root_path[192];
static unsigned frame,error_code,loaded;
static int io_error;
static unsigned current_style;
static unsigned allocated,frame_tiles,peak_tiles,peak_load_us;
static void (*flush_commands)(void);
static unsigned flush_count;
static unsigned char compressed[66000];
typedef struct {float u,v;unsigned color;float x,y,z;} Vertex;
int tiles_init(const char *root){if(strlen(root)>=sizeof(root_path))return 0;strcpy(root_path,root);frame=1;error_code=loaded=0;io_error=0;return 1;}
void tiles_close(void){for(unsigned i=0;i<HANDLES;++i){if(files[i].file)fclose(files[i].file);free(files[i].tiles);}for(unsigned i=0;i<CACHE;++i)free(cache[i].pixels);memset(files,0,sizeof(files));memset(cache,0,sizeof(cache));allocated=frame_tiles=peak_tiles=peak_load_us=flush_count=0;current_style=0;}
void tiles_begin(void){++frame;frame_tiles=0;}
void tiles_synced(void){for(unsigned i=0;i<CACHE;++i)cache[i].pending=0;}
unsigned tiles_trim(unsigned bytes){
    unsigned freed=0;
    while(freed<bytes){
        Cached *oldest=0;
        for(unsigned i=0;i<CACHE;++i)if(cache[i].pixels&&!cache[i].pending&&(!oldest||cache[i].used<oldest->used))oldest=&cache[i];
        if(!oldest)break;
        free(oldest->pixels);memset(oldest,0,sizeof(*oldest));allocated-=65536;freed+=65536;
    }
    return freed;
}
void tiles_set_flush(void (*flush)(void)){flush_commands=flush;}
unsigned tiles_flushes(void){return flush_count;}
int tiles_flush(void){if(!flush_commands)return 0;flush_commands();tiles_synced();++flush_count;return 1;}
void tiles_style(unsigned style){current_style=style;}
/* Metadata and decoded pixels survive file eviction. Cached tiles therefore
   need no disc open at all, while compressed misses share only two streams. */
static int image_stream(ImageFile *im,unsigned id){
    if(im->file)return 1;
    unsigned count=0;ImageFile *oldest=0;
    for(unsigned i=0;i<HANDLES;++i)if(files[i].file){++count;if(!oldest||files[i].used<oldest->used)oldest=&files[i];}
    if(count>=OPEN_IMAGES&&oldest){fclose(oldest->file);oldest->file=0;}
    char path[224];snprintf(path,sizeof(path),"%sI%05u.IMG",root_path,id);
    errno=0;im->file=fopen(path,"rb");
    trace_add("image %u open=%s errno=%d",id,im->file?"ok":"FAIL",errno);
    if(!im->file){io_error=errno;error_code=1;return 0;}
    return 1;
}
static ImageFile *open_image(unsigned id){
    ImageFile *out=&files[0];
    for(unsigned i=0;i<HANDLES;++i){if(files[i].levels&&files[i].id==id){files[i].used=frame;return &files[i];}if(!files[i].levels||files[i].used<out->used)out=&files[i];}
    if(out->file)fclose(out->file);
    free(out->tiles);memset(out,0,sizeof(*out));
    if(!image_stream(out,id))return 0;
    uint32_t h[9];int ok=fread(h,1,36,out->file)==36&&!memcmp(h,"IMG2",4)&&h[1]==2&&h[2]&&h[3]&&h[4]&&h[5]&&h[6]>0&&h[6]<=3&&h[7]>0&&h[7]<65536&&(h[8]==0||h[8]==3);
    if(ok){out->id=id;out->w=h[2];out->h=h[3];out->levels=h[6];out->count=h[7];out->format=h[8];out->used=frame;
        out->tiles=render_alloc(16,h[7]*sizeof(Tile));ok=out->tiles&&fread(out->level,20,h[6],out->file)==h[6]&&fread(out->tiles,8,h[7],out->file)==h[7];}
    long length=0;if(ok){ok=!fseek(out->file,0,SEEK_END);length=ftell(out->file);}
    for(unsigned i=0;ok&&i<out->levels;++i){Level *l=&out->level[i];if(!l->w||!l->h||l->cols!=(l->w+127)/128||l->rows!=(l->h+127)/128||l->first>out->count||l->cols*l->rows>out->count-l->first)ok=0;}
    for(unsigned i=0;ok&&i<out->count;++i){Tile *t=&out->tiles[i];if(!t->length||t->length>sizeof(compressed)||t->offset>(unsigned long)length||t->length>(unsigned long)length-t->offset)ok=0;}
    if(!ok){io_error=errno;fclose(out->file);free(out->tiles);memset(out,0,sizeof(*out));error_code=2;return 0;}return out;
}
int tiles_opaque(unsigned image){ImageFile *im=open_image(image);return im&&im->format==0;}
static void *fetch(ImageFile *im,unsigned id,int queued){
    Cached *slot=0;
    for(unsigned i=0;i<CACHE;++i){Cached *c=&cache[i];
        if(c->valid&&c->image==im->id&&c->tile==id&&c->style==current_style){if(c->used!=frame){++frame_tiles;if(frame_tiles>peak_tiles)peak_tiles=frame_tiles;}c->used=frame;c->pending|=queued;return c->pixels;}
        if(!c->valid){slot=c;break;}
        if(!c->pending&&(!slot||c->used<slot->used))slot=c;
    }
    if(!slot&&queued&&tiles_flush()){slot=&cache[0];for(unsigned i=1;i<CACHE;++i)if(cache[i].used<slot->used)slot=&cache[i];}
    if(!slot){error_code=3;return 0;} /* Never overwrite a texture still used by GU. */
    uint64_t started=sceKernelGetSystemTimeWide();
    if(!slot->pixels){slot->pixels=memalign(16,65536);if(slot->pixels)allocated+=65536;}
    if(!slot->pixels){
        /* Reuse an existing idle allocation before failing on a fragmented heap. */
        for(unsigned i=0;i<CACHE;++i)if(cache[i].pixels&&!cache[i].pending){slot=&cache[i];break;}
    }
    if(!slot->pixels&&queued&&tiles_flush()){
        for(unsigned i=0;i<CACHE;++i)if(cache[i].pixels){slot=&cache[i];break;}
    }
    if(!slot->pixels){error_code=4;return 0;}
    if(!image_stream(im,im->id))return 0;
    /* A failed read must never leave overwritten pixels tagged as the old tile. */
    slot->valid=0;
    Tile *t=&im->tiles[id];uLongf size=im->format==3?65536:32768;
    if(fseek(im->file,t->offset,SEEK_SET)||fread(compressed,1,t->length,im->file)!=t->length||uncompress(slot->pixels,&size,compressed,t->length)!=Z_OK||size!=(im->format==3?65536u:32768u)) {io_error=errno;error_code=5;return 0;}
    if(current_style){
        /* Backward traversal permits in-place RGB565 -> RGBA mask expansion. */
        for(unsigned n=128*128;n;--n){unsigned i=n-1,r,g,b,a=255;
            if(im->format==0){unsigned value=((unsigned short*)slot->pixels)[i];r=(value&31)*255/31;g=((value>>5)&63)*255/63;b=(value>>11)*255/31;}
            else {unsigned value=((unsigned*)slot->pixels)[i];r=value&255;g=(value>>8)&255;b=(value>>16)&255;a=value>>24;}
            unsigned gray=(298912*r+586611*g+114478*b+500000)/1000000;
            if(current_style==3){((unsigned*)slot->pixels)[i]=0xffffff|(gray<<24);continue;}
            if(current_style==4){((unsigned*)slot->pixels)[i]=0xffffff|(a<<24);continue;}
            if(current_style==1)r=g=b=gray+51;
            else {r=gray+9;g=gray+3;b=gray;}
            if(r>255)r=255;
            if(g>255)g=255;
            if(b>255)b=255;
            if(im->format==0)((unsigned short*)slot->pixels)[i]=(r*31/255)|((g*63/255)<<5)|((b*31/255)<<11);
            else ((unsigned*)slot->pixels)[i]=r|(g<<8)|(b<<16)|(a<<24);
        }
    }
    slot->valid=1;slot->image=im->id;slot->tile=id;slot->used=frame;slot->style=current_style;slot->pending=queued;loaded++;
    ++frame_tiles;if(frame_tiles>peak_tiles)peak_tiles=frame_tiles;
    unsigned elapsed=(unsigned)(sceKernelGetSystemTimeWide()-started);if(elapsed>peak_load_us)peak_load_us=elapsed;
    sceKernelDcacheWritebackRange(slot->pixels,65536);return slot->pixels;
}
int tiles_rule(unsigned image,unsigned char *pixels){
    ImageFile *im=open_image(image);if(!im)return 0;
    Level *l=&im->level[0];unsigned saved=current_style;current_style=0;
    for(unsigned ty=0;ty<l->rows;++ty)for(unsigned tx=0;tx<l->cols;++tx){
        void *data=fetch(im,l->first+ty*l->cols+tx,0);if(!data){current_style=saved;return 0;}
        for(unsigned y=0;y<272;++y){unsigned iy=(y?y-1:0)*l->h/270;if(iy>=l->h)iy=l->h-1;if(iy/128!=ty)continue;
            for(unsigned x=0;x<480;++x){unsigned ix=x*l->w/480;if(ix/128!=tx)continue;
                unsigned k=(iy%128)*128+ix%128,r,g,b;
                if(im->format==0){unsigned v=((unsigned short*)data)[k];r=(v&31)*255/31;g=((v>>5)&63)*255/63;b=(v>>11)*255/31;}
                else{unsigned v=((unsigned*)data)[k];r=v&255;g=(v>>8)&255;b=(v>>16)&255;}
                pixels[y*480+x]=(unsigned char)((r*77+g*150+b*29+128)>>8);
            }
        }
    }current_style=saved;return 1;
}
int tiles_draw(unsigned image,float x,float y,float sx,float sy,float rotation,float cx,float cy,float cw,float ch,unsigned color){
    ImageFile *im=open_image(image);if(!im)return 0;
    unsigned mip=0;float scale=fmaxf(fabsf(sx)*im->w/im->level[0].w,fabsf(sy)*im->h/im->level[0].h);
    while(mip+1<im->levels&&scale<.5f){scale*=2;mip++;}
    Level *l=&im->level[mip];float rx=(float)im->w/l->w,ry=(float)im->h/l->h;
    if(cw<=0)cw=im->w;
    if(ch<=0)ch=im->h;
    float c=cosf(rotation),s=sinf(rotation);
    sceGuEnable(GU_TEXTURE_2D);sceGuEnable(GU_BLEND);sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
    sceGuTexMode(current_style>=3?GU_PSM_8888:im->format,0,0,0);sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);sceGuTexFilter(GU_LINEAR,GU_LINEAR);sceGuTexWrap(GU_CLAMP,GU_CLAMP);
    for(unsigned ty=0;ty<l->rows;++ty)for(unsigned tx=0;tx<l->cols;++tx){
        float ox=tx*128*rx,oy=ty*128*ry;
        float a=fmaxf(ox,cx),b=fmaxf(oy,cy),d=fminf(fminf((tx+1)*128*rx,im->w),cx+cw),e=fminf(fminf((ty+1)*128*ry,im->h),cy+ch);
        if(a>=d||b>=e)continue;
        float vx[4]={a,d,a,d},vy[4]={b,b,e,e},px[4],py[4];
        float minx=1e9f,maxx=-1e9f,miny=1e9f,maxy=-1e9f;
        for(unsigned j=0;j<4;++j){float lx=(vx[j]-cx)*sx,ly=(vy[j]-cy)*sy;px[j]=x+c*lx-s*ly;py[j]=y+s*lx+c*ly;minx=fminf(minx,px[j]);maxx=fmaxf(maxx,px[j]);miny=fminf(miny,py[j]);maxy=fmaxf(maxy,py[j]);}
        if(maxx<0||minx>480||maxy<0||miny>272)continue;
        void *pixels=fetch(im,l->first+ty*l->cols+tx,1);if(!pixels)return 0;
        sceGuTexImage(0,128,128,128,pixels);
        Vertex *v=sceGuGetMemory(4*sizeof(*v));
        for(unsigned j=0;j<4;++j)v[j]=(Vertex){(vx[j]-ox)/rx,(vy[j]-oy)/ry,color,px[j],py[j],0};
        sceGuDrawArray(GU_TRIANGLE_STRIP,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,4,0,v);
    }
    sceGuDisable(GU_TEXTURE_2D);return 1;
}
unsigned tiles_error(void){return error_code;}
int tiles_io_error(void){return io_error;}
unsigned tiles_loaded(void){return loaded;}
unsigned tiles_bytes(void){return allocated;}
unsigned tiles_peak_frame(void){return peak_tiles;}
unsigned tiles_peak_load_us(void){return peak_load_us;}
