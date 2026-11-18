#include "gallery.h"
#include "tiles.h"
#include "sound.h"
#include "cnfont.h"
#include <pspctrl.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

int gallery_load(Gallery *g,const char *root){
    memset(g,0,sizeof(*g));char path[192];snprintf(path,sizeof(path),"%sGALLERY.BIN",root);
    FILE *f=fopen(path,"rb");if(!f)return 0;
    uint32_t h[4];int ok=fread(h,1,16,f)==16&&!memcmp(h,"GAL1",4)&&h[1]==1&&h[2]>0&&h[2]<=128&&h[3]==sizeof(GalleryEntry);
    if(ok){g->count=h[2];ok=fread(g->entries,sizeof(GalleryEntry),g->count,f)==g->count&&fgetc(f)==EOF;}
    fclose(f);
    for(unsigned i=0;ok&&i<g->count;++i){GalleryEntry *e=&g->entries[i];
        ok=e->group>=0&&e->group<64&&e->image>=-1&&e->background>=-1&&memchr(e->name,0,sizeof(e->name))&&
            (e->image<0||(e->width>0&&e->height>0))&&(e->background<0||(e->bw>0&&e->bh>0));
        if(i&&e->group<g->entries[i-1].group)ok=0;
        if((unsigned)e->group>=g->groups)g->groups=(unsigned)e->group+1;
    }
    return ok;
}
void gallery_close(Gallery *g){memset(g,0,sizeof(*g));}
static void unlock_command(Gallery *g,const Story *s,unsigned index){
    const StoryCommand *c=&s->commands[index];
    if((c->op!=ST_BG&&c->op!=ST_CG&&c->op!=ST_EVSET)||story_number(s,c,"lock",0)==1||!*story_get(s,c,"set"))return;
    const char *name=story_get(s,c,"file");if(c->op!=ST_EVSET&&name[0]=='z')++name;
    for(unsigned i=0;i<g->count;++i)if(g->entries[i].image>=0&&!strcmp(g->entries[i].name,name))g->unlocked[i]=1;
}
void gallery_progress(Gallery *g,const Story *s,const unsigned char *read){
    memset(g->unlocked,0,sizeof(g->unlocked));
    for(unsigned i=0;i<s->command_count;++i)if(read[i/8]&(1u<<(i%8))){
        unlock_command(g,s,i);
        /* Older profiles only saved text bits: migrate commands preceding a
           read paragraph in that same block, never future or conditional scenes. */
        const StoryCommand *c=&s->commands[i];
        if(c->op==ST_TEXT)for(unsigned j=i;j>0;){const StoryCommand *p=&s->commands[--j];
            if(p->file!=c->file||p->block!=c->block||p->op==ST_TEXT)break;
            if(!story_has(s,p,"cond"))unlock_command(g,s,j);
        }
    }
}
void gallery_open(Gallery *g){
    g->opened=1;g->viewer=0;g->zoom=0;g->pan=g->view_pan=g->pan_x=g->view_pan_x=0;g->view_zoom=(UIAxis){0};g->status="";g->transition=(UIFade){0};
    g->counter_selection=g->selection;g->counter_alpha=(UIAxis){1,0};
    g->hint_alpha=(UIAxis){1,0};g->hint_idle=0;
}
void gallery_leave(Gallery *g){g->opened=0;g->viewer=0;g->status="";g->transition=(UIFade){0};}
static int first_variant(const Gallery *g,unsigned group){
    for(unsigned i=0;i<g->count;++i)if((unsigned)g->entries[i].group==group&&g->entries[i].image>=0&&g->unlocked[i])return (int)i;
    return -1;
}
static unsigned available_count(const Gallery *g,unsigned group){
    unsigned count=0;for(unsigned i=0;i<g->count;++i)if((unsigned)g->entries[i].group==group&&g->entries[i].image>=0&&g->unlocked[i])++count;return count;
}
static void change_view(Gallery *g,int viewer,unsigned variant){
    g->pending_view=viewer;g->pending_variant=variant;ui_fade_begin(&g->transition,1);
}
void gallery_input(Gallery *g,unsigned pressed){
    if(g->opened&&pressed)g->hint_idle=0;
    if(!g->opened||g->transition.stage)return;
    unsigned previous=g->selection;
    if(g->viewer){
        if(pressed&(PSP_CTRL_CROSS|PSP_CTRL_START)){change_view(g,0,g->variant);return;}
        int direction=(pressed&PSP_CTRL_RIGHT)?1:(pressed&PSP_CTRL_LEFT)?-1:0;
        if(direction){
            int next=(int)g->variant+direction;
            while(next>=0&&next<(int)g->count&&(unsigned)g->entries[next].group==g->selection){
                if(g->entries[next].image>=0&&g->unlocked[next]){change_view(g,1,(unsigned)next);sound_ui_tick();break;}next+=direction;
            }
        }
        if(pressed&PSP_CTRL_CIRCLE){g->zoom=!g->zoom;g->pan=g->pan_x=0;sound_ui_tick();}
    }else{
        if(pressed&(PSP_CTRL_CROSS|PSP_CTRL_START)){gallery_leave(g);return;}
        if((pressed&PSP_CTRL_UP)&&g->selection)--g->selection;
        if((pressed&PSP_CTRL_DOWN)&&g->selection+1<g->groups)++g->selection;
        if((pressed&PSP_CTRL_LEFT)&&g->selection)g->selection=g->selection>5?g->selection-5:0;
        if(pressed&PSP_CTRL_RIGHT)g->selection=g->selection+5<g->groups?g->selection+5:g->groups-1;
        if(previous!=g->selection){g->status="";sound_ui_tick();}
        if(pressed&PSP_CTRL_CIRCLE){int variant=first_variant(g,g->selection);
            if(variant>=0){change_view(g,1,(unsigned)variant);sound_ui_tick();}
            else g->status="随剧情推进解锁";
        }
    }
}
void gallery_tick(Gallery *g,float seconds){
    if(g->viewer&&g->zoom&&!g->transition.stage)g->hint_idle+=fminf(.05f,fmaxf(0,seconds));else g->hint_idle=0;
    ui_focus_step(&g->hint_alpha,seconds,g->hint_idle<3||(g->status&&*g->status));
    ui_focus_step(&g->counter_alpha,seconds,g->counter_selection==g->selection);
    if(g->counter_selection!=g->selection&&g->counter_alpha.value<.02f){g->counter_selection=g->selection;g->counter_alpha=(UIAxis){0};}
    if(ui_fade_step(&g->transition,seconds)){g->viewer=g->pending_view;g->variant=g->pending_variant;g->zoom=0;g->pan=g->view_pan=g->pan_x=g->view_pan_x=0;g->view_zoom=(UIAxis){0};g->status="";}
    if(g->viewer){
        const GalleryEntry *e=&g->entries[g->variant];
        float w=e->background>=0?1280.f:e->width,h=e->background>=0?720.f:e->height;
        float fit=fminf(480/w,270/h),zoom=fmaxf(480/w,fit*1.5f);
        g->pan=fminf(g->pan,g->zoom?fmaxf(0,h*zoom-270):0);
        float limit=g->zoom?fmaxf(0,w*zoom-480)*.5f:0;
        g->pan_x=fminf(limit,fmaxf(-limit,g->pan_x));
        g->view_pan+=(g->pan-g->view_pan)*(1-expf(-18*fminf(.05f,seconds)));
        g->view_pan_x+=(g->pan_x-g->view_pan_x)*(1-expf(-18*fminf(.05f,seconds)));
        ui_focus_step(&g->view_zoom,seconds,g->zoom);
    }
}
void gallery_stick(Gallery *g,unsigned x,unsigned y,float seconds){
    if(!g->opened||!g->viewer||!g->zoom||g->transition.stage)return;
    float dx=(float)x-128,dy=(float)y-128,length=sqrtf(dx*dx+dy*dy);
    if(length<=24)return; /* Radial dead zone prevents drift; speed follows deflection. */
    g->hint_idle=0;
    float distance=fminf(1,(length-24)/103)*180*fminf(.05f,fmaxf(0,seconds));
    const GalleryEntry *e=&g->entries[g->variant];
    float w=e->background>=0?1280.f:e->width,h=e->background>=0?720.f:e->height;
    float scale=fmaxf(480/w,fminf(480/w,270/h)*1.5f),limit=fmaxf(0,w*scale-480)*.5f;
    g->pan_x=fminf(limit,fmaxf(-limit,g->pan_x+dx/length*distance));
    g->pan=fminf(fmaxf(0,h*scale-270),fmaxf(0,g->pan+dy/length*distance));
}
static int draw_image(const GalleryEntry *e,float x,float y,float w,float h,unsigned color,float zoom,float pan_x,float pan){
    float width=e->background>=0?1280.f:(float)e->width,height=e->background>=0?720.f:(float)e->height;
    float fit=fminf(w/width,h/height),scale=fit+(fmaxf(w/width,fit*1.5f)-fit)*zoom,overflow=fmaxf(0,height*scale-h);
    float limit=fmaxf(0,width*scale-w)*.5f;
    x+=(w-width*scale)*.5f-fminf(limit,fmaxf(-limit,pan_x));y+=fmaxf(0,(h-height*scale)*.5f)-fminf(overflow,pan);
    if(e->background>=0){
        if(!tiles_draw((unsigned)e->background,x,y,scale,scale,0,0,0,0,0,color))return 0;
    }
    return tiles_draw((unsigned)e->image,x,y,scale,scale,0,0,0,0,0,color);
}
void gallery_draw(Gallery *g){
    tiles_begin();tiles_style(0);
    if(g->viewer){
        ui_fade_draw(1);const GalleryEntry *e=&g->entries[g->variant];
        if(!draw_image(e,0,1,480,270,0xffffffff,g->view_zoom.value,g->view_pan_x,g->view_pan))g->status="图片读取失败";
        ui_gallery_caption(g->status&&*g->status?g->status:g->zoom?"左右 差分　○ 缩小　摇杆 移动　× 返回":"左右 差分　○ 放大　× 返回",g->hint_alpha.value);
    }else{
        ui_gallery_sheet();
        for(unsigned i=0;i<g->groups;++i)ui_gallery_slot(i);
        char text[48];snprintf(text,sizeof(text),"%02u / %02u",g->selection+1,g->groups);ui_sheet_label(376,20,text,13,.65f);
        ui_detail_begin(0x70000000u+g->selection);
        int variant=first_variant(g,g->selection);
        ui_gallery_preview_begin();
        if(variant>=0){if(!draw_image(&g->entries[variant],96,48,368,207,ui_gallery_color(),0,0,0))g->status="图片读取失败";}
        ui_gallery_preview_end();
        if(variant<0)ui_sheet_label(240,143,"尚未解锁",16,.6f);
        if(g->status&&*g->status)ui_gallery_status(g->status);
        ui_detail_end();
        unsigned total=0;for(unsigned i=0;i<g->count;++i)if((unsigned)g->entries[i].group==g->counter_selection&&g->entries[i].image>=0)++total;
        snprintf(text,sizeof(text),"差分 %u / %u",available_count(g,g->counter_selection),total);ui_sheet_label(238,20,text,13,.7f*g->counter_alpha.value);
    }
    ui_fade_draw(g->transition.alpha);
}
