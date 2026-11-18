#include "ui.h"
#include "cnfont.h"
#include <pspgu.h>
#include <pspkernel.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "ui_motion.h"
#include "ui_effects.h"
typedef struct {float u,v;unsigned color;float x,y,z;} Vertex;
typedef struct {unsigned color;float x,y,z;} ColorVertex;
static void rounded_panel(float x,float y,float w,float h,float r,unsigned top,unsigned bottom);
int ui_texture_load(UITexture *t,const char *path) {
    memset(t,0,sizeof(*t));FILE *f=fopen(path,"rb");if(!f)return 0;
    uint32_t h[6];errno=0;int ok=fread(h,1,24,f)==24&&!memcmp(h,"TEX1",4)&&h[1]==1&&h[2]>0&&h[3]>0&&h[4]<=512&&h[5]<=512&&h[2]<=h[4]&&h[3]<=h[5]&&!(h[4]&(h[4]-1))&&!(h[5]&(h[5]-1));
    int failure=errno?errno:EINVAL;
    if(ok) {
        t->w=h[2];t->h=h[3];t->tw=h[4];t->th=h[5];unsigned bytes=t->tw*t->th*2;
        t->pixels=memalign(16,bytes);
        if(!t->pixels){ok=0;failure=ENOMEM;}
        else {errno=0;ok=fread(t->pixels,1,bytes,f)==bytes&&fgetc(f)==EOF&&!ferror(f);failure=errno?errno:EINVAL;}
    }
    fclose(f);if(!ok) {ui_texture_close(t);errno=failure;return 0;}
    sceKernelDcacheWritebackAll();return 1;
}
void ui_texture_close(UITexture *t) {free(t->pixels);memset(t,0,sizeof(*t));}
void ui_texture_draw(const UITexture *t,int x,int y,int u,int v,int w,int h,unsigned color) {
    if(!t->pixels)return;
    sceGuEnable(GU_TEXTURE_2D);sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
    sceGuTexMode(GU_PSM_4444,0,0,0);sceGuTexImage(0,t->tw,t->th,t->tw,t->pixels);
    sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);sceGuTexFilter(GU_LINEAR,GU_LINEAR);
    sceGuTexWrap(GU_CLAMP,GU_CLAMP);
    Vertex *p=sceGuGetMemory(2*sizeof(*p));
    p[0]=(Vertex){u,v,color,x,y,0};p[1]=(Vertex){u+w,v+h,color,x+w,y+h,0};
    sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,p);
    sceGuDisable(GU_TEXTURE_2D);
}
void ui_title(const UITexture *atlas,unsigned selected,unsigned enabled) {
    ui_texture_draw(atlas,39,23,0,108,162,72,0xffffffff);
    for(unsigned i=0;i<7;++i) {
        int active=(enabled&(1u<<i))!=0;
        /* Original disabled TRUE END is blank; preserve that appearance. */
        unsigned col=!active?144:i==selected?72:0;
        ui_texture_draw(atlas,45,1+(298+41*i)*3/8,col,i*15,72,15,0xffffffff);
    }
}
static void panel(int x,int y,int w,int h,unsigned color) {
    sceGuDisable(GU_TEXTURE_2D);sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
    ColorVertex *p=sceGuGetMemory(2*sizeof(*p));
    p[0]=(ColorVertex){color,x,y,0};p[1]=(ColorVertex){color,x+w,y+h,0};
    sceGuDrawArray(GU_SPRITES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,p);
}
static UIMotion motion;
void ui_wave_nudge(void){ui_motion_nudge(&motion);}
static unsigned detail_key=~0u;
static float detail_fade=1;
static float frame_seconds,meter_fraction;
static int drawing_detail;
void ui_tick(uint64_t delta,int open,unsigned category,unsigned row){frame_seconds=fminf(.05f,(float)delta/1000000);ui_motion_step(&motion,frame_seconds,open,category,row);detail_fade=fminf(1,detail_fade+frame_seconds*5);}
int ui_visible(void){return motion.visibility.value>.005f;}
void ui_page_tick(uint64_t delta,unsigned page,unsigned selection){ui_page_step(&motion,(float)delta/1000000,page,selection);}
unsigned ui_page(void){return motion.page;}
int ui_page_ready(unsigned page){return motion.page==page&&motion.page_alpha.value>.9f;}
static unsigned white(float alpha){return ((unsigned)(fminf(1,fmaxf(0,alpha))*255)<<24)|0xffffff;}
void ui_fade_draw(float alpha){if(alpha>0)panel(0,0,480,272,white(alpha)&0xff000000);}
static void geometry(void){sceGuDisable(GU_TEXTURE_2D);sceGuEnable(GU_BLEND);sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);sceGuShadeModel(GU_SMOOTH);}
static void quad(float x,float y,float w,float h,unsigned top,unsigned bottom){
    ColorVertex *v=sceGuGetMemory(4*sizeof(*v));v[0]=(ColorVertex){top,x,y,0};v[1]=(ColorVertex){top,x+w,y,0};v[2]=(ColorVertex){bottom,x,y+h,0};v[3]=(ColorVertex){bottom,x+w,y+h,0};
    sceGuDrawArray(GU_TRIANGLE_STRIP,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,4,0,v);
}
static void stroke(float x,float y,float xx,float yy,float width,unsigned color){
    float dx=xx-x,dy=yy-y,length=sqrtf(dx*dx+dy*dy);if(length<.01f)return;
    dx*=width*.5f/length;dy*=width*.5f/length;
    ColorVertex *v=sceGuGetMemory(4*sizeof(*v));v[0]=(ColorVertex){color,x-dy,y+dx,0};v[1]=(ColorVertex){color,x+dy,y-dx,0};v[2]=(ColorVertex){color,xx-dy,yy+dx,0};v[3]=(ColorVertex){color,xx+dy,yy-dx,0};
    sceGuDrawArray(GU_TRIANGLE_STRIP,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,4,0,v);
}
static void ring(float x,float y,float radius,float width,unsigned color){
    ColorVertex *v=sceGuGetMemory(50*sizeof(*v));
    for(unsigned i=0;i<=24;++i){float angle=i*6.2831853f/24,c=cosf(angle),s=sinf(angle);v[2*i]=(ColorVertex){color,x+c*(radius-width*.5f),y+s*(radius-width*.5f),0};v[2*i+1]=(ColorVertex){color,x+c*(radius+width*.5f),y+s*(radius+width*.5f),0};}
    sceGuDrawArray(GU_TRIANGLE_STRIP,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,50,0,v);
}
static void glow(float x,float y,float radius,float opacity){
    ColorVertex *v=sceGuGetMemory(26*sizeof(*v));v[0]=(ColorVertex){white(opacity),x,y,0};
    for(unsigned i=0;i<=24;++i){float a=i*6.2831853f/24;v[i+1]=(ColorVertex){0x00ffffff,x+cosf(a)*radius,y+sinf(a)*radius*.65f,0};}
    sceGuDrawArray(GU_TRIANGLE_FAN,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,26,0,v);
}
/* Small geometric silhouettes use no texture assets or additional VRAM. */
static void icon(unsigned kind,float x,float y,float scale,float opacity){
    unsigned color=white(opacity);float s=scale,w=1.65f*s;geometry();
    if(kind==0){
        const float points[][2]={{-13,6},{-11,-5},{-7,-8},{7,-8},{11,-5},{13,6},{10,9},{5,3},{-5,3},{-10,9},{-13,6}};
        for(unsigned i=1;i<11;++i)stroke(x+points[i-1][0]*s,y+points[i-1][1]*s,x+points[i][0]*s,y+points[i][1]*s,w,color);
        stroke(x-8*s,y-2*s,x-2*s,y-2*s,w,color);stroke(x-5*s,y-5*s,x-5*s,y+s,w,color);
        ring(x+7*s,y-4*s,s,.9f*s,color);ring(x+9*s,y,s,.9f*s,color);
    }else if(kind==1){
        stroke(x-12*s,y-8*s,x+12*s,y-8*s,w,color);stroke(x-12*s,y-8*s,x-12*s,y+7*s,w,color);stroke(x+12*s,y-8*s,x+12*s,y+7*s,w,color);stroke(x-12*s,y+7*s,x+12*s,y+7*s,w,color);
        stroke(x,y+7*s,x,y+11*s,w,color);stroke(x-6*s,y+11*s,x+6*s,y+11*s,w,color);
    }else if(kind==2){
        stroke(x-5*s,y+5*s,x-5*s,y-9*s,w,color);stroke(x+8*s,y+2*s,x+8*s,y-12*s,w,color);stroke(x-5*s,y-9*s,x+8*s,y-12*s,w*2,color);
        ring(x-8*s,y+6*s,3*s,2.7f*s,color);ring(x+5*s,y+3*s,3*s,2.7f*s,color);
    }else{
        ring(x,y,7*s,2.8f*s,color);ring(x,y,2.3f*s,1.1f*s,color);
        for(unsigned i=0;i<8;++i){float a=i*.785398f;stroke(x+cosf(a)*8*s,y+sinf(a)*8*s,x+cosf(a)*11*s,y+sinf(a)*11*s,3*s,color);}
    }
}
static void backdrop(float opacity){
    geometry();
    unsigned a=(unsigned)(opacity*148);quad(0,0,480,272,(a<<24)|0x4e270b,((unsigned)(opacity*183)<<24)|0x422008);
    /* Three slowly travelling translucent ribbons, each with a soft lower edge. */
    for(unsigned layer=0;layer<3;++layer){
        ColorVertex *v=sceGuGetMemory(130*sizeof(*v));
        for(unsigned i=0;i<=64;++i){float x=i*7.5f;
            float y=ui_wave_y(&motion,x,layer);
            unsigned alpha=(unsigned)(opacity*(layer==0?27:15));
            v[2*i]=(ColorVertex){(alpha<<24)|0xffe1b2,x,y,0};v[2*i+1]=(ColorVertex){0x00ffe1b2,x,y+38+layer*8,0};}
        sceGuDrawArray(GU_TRIANGLE_STRIP,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,130,0,v);
    }
}
static void label(float x,float y,const char *text,float alpha,float size){
    cnfont_smooth(x+1,y+1,text,((unsigned)(fminf(1,alpha)*95)<<24)|0x130a04,size);
    cnfont_smooth(x,y,text,white(alpha),size);
}
static unsigned hint_length(const char *s){unsigned n=0;for(;*s;++s)if(((unsigned char)*s&0xc0)!=0x80)++n;return n;}
/* Keep every button visually attached to its action. No font atlas or texture
   is needed for the button frames; all screens share the same spacing. */
static float hint_key_width(const char *key,float size){return fmaxf(size+9,hint_length(key)*size+8);}
static void hint_item(float x,float y,const char *key,const char *action,float a,float size,float key_column){
    float w=hint_key_width(key,size);geometry();
    rounded_panel(x,y,w,size+6,3,white(a*.26f),white(a*.09f));
    label(x+(w-hint_length(key)*size)*.5f,y+3,key,a,size);
    label(x+(key_column>0?key_column:w+7),y+3,action,a*.88f,size);
}
static void hint_row(const char *text,float y,float a,float size){
    char keys[6][32],actions[6][96];unsigned count=0;float width=0;
    while(text&&*text&&count<6){
        const char *end=strstr(text,"　");if(!end)end=text+strlen(text);
        const char *space=memchr(text,' ',(size_t)(end-text));
        if(!space){label(28,y+3,text,a,size);return;}
        size_t k=(size_t)(space-text),n=(size_t)(end-space-1);
        if(k>=sizeof(keys[0])||n>=sizeof(actions[0]))return;
        memcpy(keys[count],text,k);keys[count][k]=0;
        memcpy(actions[count],space+1,n);actions[count][n]=0;
        width+=hint_key_width(keys[count],size)+7+hint_length(actions[count])*size;
        ++count;text=*end?end+3:end;
    }
    width+=count>0?(count-1)*18:0;
    float x=(480-width)*.5f;
    for(unsigned i=0;i<count;++i){
        hint_item(x,y,keys[i],actions[i],a,size,0);
        x+=hint_key_width(keys[i],size)+7+hint_length(actions[i])*size+18;
    }
}
static void controls_body(float x,float y,float a){
    static const char *keys[]={"○","L","R","□","△","摇杆","START"};
    static const char *actions[]={"下一句／显示全部文字","快速保存","自动播放开关","按住已读快进，松开停止","隐藏／恢复对白","隐藏后放大移动，松开回正","打开功能菜单"};
    for(unsigned i=0;i<7;++i)hint_item(x,y+i*22,keys[i],actions[i],a,13,88);
}
void ui_menu(unsigned category,unsigned row,const char *detail,int editing) {
    static const char *cats[]={"游戏","显示","声音","系统"};
    static const char *items[4][8]={
        {"返回游戏","保存进度","读取进度","对话回看","自动播放","快速保存","快速读取","返回标题"},
        {"文字速度","自动等待","文字大小"},
        {"背景音乐","人物配音","音效"},
        {"操作说明","自动存档","画面查看","关于本作","恢复默认","退出游戏"}};
    if(category>=4)return;
    float opacity=motion.visibility.value*motion.page_alpha.value,enter=(1-opacity)*13;backdrop(motion.visibility.value);
    for(unsigned i=0;i<4;++i){float distance=fabsf((float)i-motion.horizontal.value),focus=fmaxf(0,1-distance),x=128+((float)i-motion.horizontal.value)*86;
        geometry();if(focus>.01f)glow(x,67+enter,32,opacity*focus*.13f);
        icon(i,x,67+enter,1+focus*.24f,opacity*(.42f+.58f*focus));
        label(x-16,92+enter,cats[i],opacity*(.4f+.6f*focus),16);
    }
    float eased=motion.items*motion.items*(3-2*motion.items);
    float fade=opacity*eased,slide=(1-eased)*13;
    sceGuScissor(0,114,480,237);
    for(unsigned i=0;i<8&&items[category][i];++i){
        float offset=(float)i-motion.vertical.value;
        float y=151+offset*33+enter;
        if(y<96||y>248)continue;
        float focus=fmaxf(0,1-fabsf(offset)),edge=fminf(1,fminf((y-105)/24,(245-y)/24));
        float alpha=fade*fmaxf(0,edge)*(.36f+.64f*focus),x=128+slide;
        geometry();if(focus>.01f)glow(x,y,22,alpha*focus*(.12f+.025f*sinf(motion.pulse)));
        /* A hollow diamond belongs to the vertical axis; selected items gently enlarge. */
        float radius=3.5f+focus*2;unsigned color=white(alpha);
        stroke(x-radius,y,x,y-radius,1.25f,color);stroke(x,y-radius,x+radius,y,1.25f,color);stroke(x+radius,y,x,y+radius,1.25f,color);stroke(x,y+radius,x-radius,y,1.25f,color);
        label(155+slide+focus*2,y-8,items[category][i],alpha,16+focus);
    }
    sceGuScissor(0,0,480,272);
    if(detail&&*detail)label(298,143+enter,detail,fade,14);
    hint_row(editing?"左右 调整　○ 完成　× 返回":"左右 分类　上下 选择　○ 确认　× 返回",244,opacity*.85f,12);
    (void)row;
}
void ui_value(const char *value){label(302,170,value,motion.visibility.value*motion.page_alpha.value*motion.items*.8f,14);}
void ui_overlay(void){backdrop(1);}
void ui_highlight(int x,int y,int width,int height){panel(x,y,width,height,0x50ffffff);}
static float detail_ease(void){return detail_fade*detail_fade*(3-2*detail_fade);}
void ui_detail_begin(unsigned key){if(detail_key!=key){detail_key=key;detail_fade=0;}drawing_detail=1;}
void ui_detail_end(void){drawing_detail=0;}
static float sheet_alpha(void){return motion.visibility.value*motion.page_alpha.value*(drawing_detail?detail_ease():1);}
static float sheet_slide(void){return (1-motion.page_alpha.value)*18+(1-motion.visibility.value)*12+(drawing_detail?(1-detail_ease())*8:0);}
unsigned ui_gallery_color(void){return white(sheet_alpha());}
void ui_gallery_preview_begin(void){
    geometry();float a=sheet_alpha();
    rounded_panel(94,46,372,211,4,white(a*.18f),white(a*.04f));
    panel(96,48,368,207,((unsigned)(a*180)<<24)|0x2f1b0e);
    sceGuScissor(96,48,464,255);
}
void ui_gallery_preview_end(void){sceGuScissor(0,0,480,272);}
void ui_gallery_sheet(void){
    backdrop(motion.visibility.value);geometry();float a=sheet_alpha();
    quad(24,42,432,1,white(a*.2f),white(a*.2f));
    ui_sheet_label(28,16,"图片鉴赏",19,1);
    hint_row("上下 选择　左右 翻页　○ 查看　× 返回",255,a*.85f,11);
}
void ui_gallery_slot(unsigned index){
    float y=146+((float)index-motion.list.value)*32;
    if(y<49||y>245)return;
    float edge=fmaxf(0,fminf(1,fminf((y-49)/24,(245-y)/24)));
    float focus=fmaxf(0,1-fabsf((float)index-motion.list.value)),a=sheet_alpha()*edge;
    geometry();rounded_panel(26,y-5,52,28,4,white(a*focus*.14f),white(a*focus*.035f));
    char number[8];snprintf(number,sizeof(number),"%02u",index+1);
    float size=15+focus;label(52-size,y,number,a*(.5f+.5f*focus),size);
}
void ui_gallery_status(const char *text){
    geometry();float a=sheet_alpha();rounded_panel(102,231,170,20,3,((unsigned)(a*210)<<24)|0x63432a,((unsigned)(a*230)<<24)|0x2f1b0e);
    label(110,234,text,a*.85f,12);
}
void ui_gallery_caption(const char *text,float alpha){
    float a=fminf(1,fmaxf(0,alpha));if(a<.005f)return;
    float y=242+(1-a)*5;
    geometry();rounded_panel(12,y-3,456,28,4,((unsigned)(a*192)<<24)|0x63432a,((unsigned)(a*224)<<24)|0x2f1b0e);
    hint_row(text,y,a,12);
}
static UIAxis choice_focus[8],choice_visibility;
void ui_story_choices_tick(float seconds,unsigned count,unsigned selected){
    ui_focus_step(&choice_visibility,seconds,count!=0);
    for(unsigned i=0;i<8;++i)ui_focus_step(&choice_focus[i],seconds,count&&selected==i);
}
void ui_story_choice(unsigned index,unsigned count,const char *text,int disabled,unsigned size){
    if(index>=8||!count)return;
    float focus=choice_focus[index].value,a=choice_visibility.value;
    float h=44,gap=10,y=136-(count*(h+gap)-gap)*.5f+index*(h+gap)+(1-a)*8;
    float x=70-focus*3,w=340+focus*6;
    geometry();
    rounded_panel(x,y,w,h,4,((unsigned)(a*(190+focus*35))<<24)|0x65412b,((unsigned)(a*235)<<24)|0x302015);
    rounded_panel(x+1,y+1,w-2,h-2,3,((unsigned)(a*focus*46)<<24)|0xffd9a5,((unsigned)(a*focus*8)<<24)|0xffd9a5);
    quad(x+13,y+h-5,w-26,1,white(a*focus*.6f),white(a*focus*.6f));
    GlyphPosition glyphs[48];unsigned n;size_t next;
    if(layout_page(text,0,320/size,2,0,glyphs,48,&n,&next)){
        unsigned rows=n?glyphs[n-1].row+1:1;
        for(unsigned row=0;row<rows;++row){unsigned first=0,last=0;
            while(first<n&&glyphs[first].row<row)++first;
            last=first;while(last<n&&glyphs[last].row==row)++last;
            unsigned columns=last>first?glyphs[last-1].column+1:0;
            for(unsigned i=first;i<last;++i)glyphs[i].row=0;
            cnfont_scaled((int)(240-columns*size*.5f),(int)(y+(h-rows*size)*.5f+row*size),glyphs+first,last-first,white(a*(disabled?.35f:.8f+.2f*focus)),size);
        }
    }
}
void ui_sheet_label(float x,float y,const char *text,float size,float alpha){label(x+sheet_slide(),y,text,sheet_alpha()*alpha,size);}
void ui_sheet_centered(float y,const char *text,float size,float alpha){
    GlyphPosition glyphs[30];unsigned count;size_t next;
    if(layout_page(text,0,30,1,0,glyphs,30,&count,&next)&&!text[next])
        ui_sheet_label((480-count*size)*.5f,y,text,size,alpha);
}
void ui_sheet_body(float x,float y,const char *text,size_t offset,unsigned columns,unsigned rows,float size){
    GlyphPosition glyphs[252];unsigned n;size_t next;
    if(layout_page(text,offset,columns,rows,0,glyphs,252,&n,&next))cnfont_glyphs_smooth(x+sheet_slide(),y,glyphs,n,white(sheet_alpha()*.92f),size);
}
void ui_sheet(const char *title,const char *hint){
    backdrop(motion.visibility.value);geometry();float a=sheet_alpha(),x=sheet_slide();
    quad(24+x,54,432,1,white(a*.2f),white(a*.2f));
    quad(24+x,235,432,1,white(a*.14f),white(a*.14f));
    ui_sheet_label(28,23,title,20,1);hint_row(hint,244,a*.85f,12);
}
void ui_slot(unsigned index,int selected,const char *chapter){
    float y=139+((float)index-motion.list.value)*34,x=sheet_slide();
    if(y<48||y>233)return;
    float edge=fminf(1,fminf((y-52)/28,(233-y)/28));if(edge<0)return;
    float focus=fmaxf(0,1-fabsf((float)index-motion.list.value)),a=sheet_alpha()*edge;
    geometry();sceGuScissor(24,60,214,230);
    quad(26+x,y-6,180,29,white(a*focus*.1f),white(a*focus*.035f));
    quad(26+x,y-4,2,23,white(a*focus*.9f),white(a*focus*.4f));
    char number[16];
    if(index>=10){strcpy(number,index==10?"快速":"自动");rounded_panel(31+x,y-2,31,20,3,white(a*.2f),white(a*.08f));}
    else snprintf(number,sizeof(number),"%02u",index+1);
    label(36+x,y,number,a*(.5f+.5f*focus),index>=10?12:15);
    GlyphPosition glyphs[9];unsigned n;size_t next;
    if(layout_page(chapter,0,9,1,0,glyphs,9,&n,&next))cnfont_glyphs_smooth(66+x,y,glyphs,n,white(a*(selected?1:.55f)),14);
    sceGuScissor(0,0,480,272);
}
void ui_save_preview(const uint16_t *pixels,int valid){
    static uint16_t __attribute__((aligned(16))) image[128*128];
    float x=232+sheet_slide(),y=65,a=sheet_alpha();geometry();
    rounded_panel(x-2,y-2,220,126,3,white(a*.2f),white(a*.06f));
    if(!valid){label(x+40,y+52,"暂无预览",a*.55f,16);return;}
    memcpy(image,pixels,128*68*2);sceKernelDcacheWritebackRange(image,sizeof(image));
    sceGuEnable(GU_TEXTURE_2D);sceGuTexMode(GU_PSM_5650,0,0,0);sceGuTexImage(0,128,128,128,image);
    sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);sceGuTexFilter(GU_LINEAR,GU_LINEAR);sceGuTexWrap(GU_CLAMP,GU_CLAMP);
    Vertex *v=sceGuGetMemory(2*sizeof(*v));
    v[0]=(Vertex){.5f,.5f,white(a),x,y,0};v[1]=(Vertex){119.5f,67.5f,white(a),x+216,y+122.4f,0};
    sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);
    sceGuDisable(GU_TEXTURE_2D);
}
void ui_setting_row(unsigned index,int selected,const char *name,const char *value){
    static UIAxis focus[3];if(index>=3)return;
    ui_focus_step(&focus[index],frame_seconds,selected);float f=focus[index].value;
    float x=32+sheet_slide(),y=76+index*46,a=sheet_alpha();geometry();
    rounded_panel(x-8,y-8,432,36,4,white(a*f*.13f),white(a*f*.035f));
    label(x,y,name,a*(.65f+.35f*f),16);label(x+264,y,value,a*(.65f+.35f*f),16);
}
void ui_meter(float fraction){
    geometry();float x=sheet_slide(),a=sheet_alpha();fraction=fminf(1,fmaxf(0,fraction));
    if(motion.page_alpha.value<.03f)meter_fraction=fraction;
    meter_fraction+=(fraction-meter_fraction)*(1-expf(-18*frame_seconds));fraction=meter_fraction;
    quad(100+x,159,280,2,white(a*.18f),white(a*.18f));
    quad(100+x,159,280*fraction,2,white(a*.85f),white(a*.85f));
    glow(100+x+280*fraction,160,10,a*.22f);ring(100+x+280*fraction,160,3,2,white(a));
}
void ui_start_hint(int visible,int in_game){
    static float progress;
    if(!in_game){progress=0;return;}
    progress=fminf(1,fmaxf(0,progress+frame_seconds*(visible?2.5f:-4.5f)));
    if(progress<=0)return;
    float alpha=progress*progress*(3-2*progress),slide=(1-alpha)*10;
    backdrop(alpha);geometry();
    quad(48,57+slide,384,1,white(alpha*.2f),white(alpha*.2f));
    label(48,24+slide,"操作说明",alpha,21);
    controls_body(48,66+slide,alpha);
    float cue=fmaxf(0,(progress-.25f)/.75f);cue=cue*cue*(3-2*cue);
    hint_row("○ 开始游戏",238+(1-cue)*7,cue*(.85f+.08f*sinf(motion.pulse)),14);
}
void ui_controls(void){
    ui_sheet("操作说明","× 返回");
    controls_body(32+sheet_slide(),65,sheet_alpha());
}
static const char *notice="";static float notice_time;static UIAxis notice_visibility;
void ui_notice(const char *message){notice=message;notice_time=1.8f;}
void ui_notice_draw(void){
    notice_time=fmaxf(0,notice_time-frame_seconds);
    /* Keep the current envelope on repeated toggles instead of flashing off. */
    ui_focus_step(&notice_visibility,frame_seconds,notice_time>0);
    float a=fminf(1,fmaxf(0,notice_visibility.value));if(a<.005f)return;
    GlyphPosition glyphs[30];unsigned count;size_t next;
    if(!layout_page(notice,0,30,1,0,glyphs,30,&count,&next)||notice[next])return;
    float width=fmaxf(176,count*14+32),x=(480-width)*.5f,y=18-(1-a)*7;
    geometry();
    rounded_panel(x-1,y-1,width+2,34,5,white(a*.27f),white(a*.055f));
    rounded_panel(x,y,width,32,4,((unsigned)(a*222)<<24)|0x63432a,((unsigned)(a*235)<<24)|0x2f1b0e);
    /* Restrained top sheen, matching the menu's translucent blue panels. */
    quad(x+10,y+1,width-20,1,white(a*.12f),white(0));
    cnfont_glyphs_smooth((480-count*14)*.5f,y+9,glyphs,count,white(a),14);
}
static uint32_t __attribute__((aligned(64))) blurred[128*128],blur_scratch[128*68];
static int blur_valid;
static unsigned blur_peak;
void ui_blur_capture(const void *framebuffer){
    uint64_t start=sceKernelGetSystemTimeWide();
    sceKernelDcacheInvalidateRange((void*)framebuffer,512*272*4);
    ui_blur_pixels(framebuffer,blurred,blur_scratch);sceKernelDcacheWritebackRange(blurred,sizeof(blurred));blur_valid=1;
    unsigned elapsed=(unsigned)(sceKernelGetSystemTimeWide()-start);if(elapsed>blur_peak)blur_peak=elapsed;
}
unsigned ui_blur_peak_us(void){return blur_peak;}
void ui_blur_draw(void){
    if(!ui_visible()){blur_valid=0;return;}if(!blur_valid)return;
    sceGuEnable(GU_TEXTURE_2D);sceGuEnable(GU_BLEND);sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
    sceGuTexMode(GU_PSM_8888,0,0,0);sceGuTexImage(0,128,128,128,blurred);sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR,GU_LINEAR);sceGuTexWrap(GU_CLAMP,GU_CLAMP);
    Vertex *v=sceGuGetMemory(2*sizeof(*v));unsigned color=white(motion.visibility.value);
    v[0]=(Vertex){0,0,color,0,0,0};v[1]=(Vertex){120,68,color,480,272,0};
    sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);sceGuDisable(GU_TEXTURE_2D);
}
static unsigned gradient_color(unsigned top,unsigned bottom,float fraction){
    unsigned color=0;
    for(unsigned shift=0;shift<32;shift+=8){float t=(float)((top>>shift)&255),b=(float)((bottom>>shift)&255);color|=(unsigned)(t+(b-t)*fraction+.5f)<<shift;}
    return color;
}
/* Rounded gradient strips use a fixed quarter-circle table, no textures or
   per-vertex trigonometry. Each panel costs 36 vertices. */
static void rounded_panel(float x,float y,float w,float h,float r,unsigned top,unsigned bottom){
    static const float sine[9]={0,.1950903f,.3826834f,.5555702f,.7071068f,.8314696f,.9238795f,.9807853f,1};
    ColorVertex *v=sceGuGetMemory(36*sizeof(*v));unsigned n=0;
    for(unsigned i=0;i<18;++i){float yy,inset;
        if(i<=8){yy=r*(1-sine[8-i]);inset=r*(1-sine[i]);}
        else{unsigned j=i-9;yy=h-r+r*sine[j];inset=r*(1-sine[8-j]);}
        unsigned color=gradient_color(top,bottom,yy/h);
        v[n++]=(ColorVertex){color,x+inset,y+yy,0};v[n++]=(ColorVertex){color,x+w-inset,y+yy,0};
    }
    sceGuDrawArray(GU_TRIANGLE_STRIP,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,n,0,v);
}
void ui_confirm_dialog(const char *message,int yes,int active){
    static float progress;static const char *previous="";static int selected,was_active;static UIAxis focus;
    if(active){previous=message;selected=yes;if(!was_active)focus=(UIAxis){yes?1:0,0};}
    was_active=active;ui_focus_step(&focus,frame_seconds,selected);
    progress=fminf(1,fmaxf(0,progress+frame_seconds*(active?6:-8)));if(progress<=0)return;
    float a=progress*progress*(3-2*progress),y=83+(1-a)*6;
    geometry();quad(0,0,480,272,((unsigned)(a*110)<<24),((unsigned)(a*110)<<24));
    rounded_panel(47,y-1,386,124,6,white(a*.25f),white(a*.06f));
    rounded_panel(48,y,384,122,5,((unsigned)(a*247)<<24)|0x63432a,((unsigned)(a*247)<<24)|0x2f1b0e);
    label(70,y+19,previous,a,16);
    geometry();
    rounded_panel(104,y+77,106,29,4,white(a*.045f),white(a*.015f));
    rounded_panel(270,y+77,106,29,4,white(a*.045f),white(a*.015f));
    float f=fminf(1,fmaxf(0,focus.value)),x=104+166*f;
    rounded_panel(x-1,y+76,108,31,5,white(a*.22f),white(a*.045f));
    rounded_panel(x,y+77,106,29,4,white(a*.16f),white(a*.045f));
    label(141,y+83,"取消",a*(1-.5f*f),16);label(307,y+83,"确认",a*(.5f+.5f*f),16);
}
