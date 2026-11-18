#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspaudio.h>
#include <pspdebug.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "catalog.h"
#include "storage.h"
#include <pspiofilemgr.h>
#include <pspgu.h>
#include <pspge.h>
#include "ui.h"
#include "cnfont.h"
#include "gallery.h"
#include "gpu_budget.h"
#include "tiles.h"
#include "ui_effects.h"
PSP_MODULE_INFO("PocketReader",0,1,0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(6144);
static volatile int running=1;
static uint16_t base[480*272];
typedef struct {uint32_t node,row;uint16_t thumb[120*68];} Snapshot;
static Snapshot saved;
static uint16_t capture[120*68];
static char device[96];
static void slotpath(char *buf,int slot){snprintf(buf,128,"%s/S%02d",device,slot);}
static int folder(void){
    if(*device)return 1;
    const char *roots[]={"ms0:","ef0:"};
    for(int i=0;i<2;i++){char dir[96];snprintf(dir,sizeof(dir),"%s/PSP",roots[i]);sceIoMkdir(dir,0777);strcat(dir,"/SAVEDATA");sceIoMkdir(dir,0777);strcat(dir,"/" DISC_ID);sceIoMkdir(dir,0777);int fd=sceIoDopen(dir);if(fd>=0){sceIoDclose(fd);strcpy(device,dir);return 1;}}return 0;
}
static int readslot(int slot){char path[128];if(!folder())return 0;slotpath(path,slot);return storage_load(path,STORY_SIG,&saved,sizeof(saved)) && saved.node<NODE_COUNT && saved.row<4;}
static int writeslot(int slot,int node,int row){char path[128];if(!folder())return 0;slotpath(path,slot);saved.node=node;saved.row=row;memcpy(saved.thumb,capture,sizeof(capture));return storage_save(path,STORY_SIG,&saved,sizeof(saved));}
static Gallery album;
static unsigned __attribute__((aligned(64))) gu_list[GPU_LIST_BYTES/4];
static uint16_t __attribute__((aligned(64))) scene_buffer[512*272],preview_pixels[128*68];
static volatile int cue_request;
void sound_ui_tick(void){cue_request=1;}
static int16_t cue[UI_CUE_FRAMES];
static int goodbye(int a,int b,void *c){(void)a;(void)b;(void)c;running=0;return 0;}
static int callbacks(SceSize n,void *p){(void)n;(void)p;int cb=sceKernelCreateCallback("exit",goodbye,0);sceKernelRegisterExitCallback(cb);sceKernelSleepThreadCB();return 0;}
static short samples[2048];
static int music(SceSize n,void *p){(void)n;(void)p;FILE *f=HAS_MUSIC?fopen("disc0:/PSP_GAME/USRDIR/MUSIC.PCM","rb"):NULL;
    int ch=sceAudioChReserve(-1,1024,PSP_AUDIO_FORMAT_STEREO);if(ch<0){if(f)fclose(f);return -1;}unsigned pos=UI_CUE_FRAMES;
    while(running){memset(samples,0,sizeof(samples));if(f){size_t got=0;while(got<sizeof(samples)){size_t k=fread((char*)samples+got,1,sizeof(samples)-got,f);got+=k;if(got<sizeof(samples)){if(ferror(f)||fseek(f,0,SEEK_SET)||(!k&&!got&&fgetc(f)==EOF))break;if(!k&&!got)rewind(f);}}}
        if(cue_request){cue_request=0;pos=0;}for(unsigned i=0;i<1024&&pos<UI_CUE_FRAMES;i++,pos++)for(int c=0;c<2;c++){int v=samples[i*2+c]+cue[pos];samples[i*2+c]=v>32767?32767:v< -32768?-32768:v;}if(sceAudioOutputBlocking(ch,0x6000,samples)<0)break;}
    sceAudioChRelease(ch);if(f)fclose(f);return 0;}
enum {NODE, BACK, INDEX, LIST, FONT};
static const char *names[]={"NODES","BACK","RUNS","SCENES","GLYPHS"};
static FILE *files[5];
static struct {uint32_t bg,first,count,edge[4];} current;
typedef struct {uint16_t op,x,y,w,h,color;uint32_t value;} Cmd;
static Cmd commands[256];
static struct {uint32_t tag; unsigned char pixels[400];} cache[128];
static int fetch(int file,uint64_t offset,void *buf,size_t bytes){
    return files[file] && offset<=1500000000 && !fseek(files[file],(long)offset,SEEK_SET) && fread(buf,1,bytes,files[file])==bytes;
}
static uint16_t mix(unsigned b,unsigned f,unsigned a){
    unsigned r=((f&31)*a+(b&31)*(255-a))/255,g=(((f>>5)&63)*a+((b>>5)&63)*(255-a))/255,v=((f>>11)*a+(b>>11)*(255-a))/255;
    return r|(g<<5)|(v<<11);
}
static int draw(uint16_t *fb,unsigned id){
    uint32_t run[2];if(!fetch(INDEX,(uint64_t)id*8,run,8)||run[1]>256||!fetch(LIST,run[0],commands,run[1]*sizeof(Cmd)))return 0;
    for(unsigned i=0;i<run[1];i++){
        Cmd c=commands[i];unsigned slot=c.value%128;
        if(c.op>1 || (c.op==0 && c.value>255) || (c.op==1 && (c.w!=20 || c.h!=20 || c.value>=0xffffffffu)))return 0;
        if(c.op==1 && cache[slot].tag!=c.value+1){if(!fetch(FONT,(uint64_t)c.value*400,cache[slot].pixels,400))return 0;cache[slot].tag=c.value+1;}
        for(unsigned y=0;y<c.h && c.y+y<272;y++)for(unsigned x=0;x<c.w && c.x+x<480;x++){
            uint16_t *pixel=&fb[(y+c.y)*512+x+c.x];unsigned a=c.op?cache[slot].pixels[y*20+x]:c.value;
            if(a==255)*pixel=c.color;else if(a)*pixel=mix(*pixel,c.color,a);
        }
    }return 1;
}
int main(void){
    int t=sceKernelCreateThread("callbacks",callbacks,0x11,4096,0,0);if(t>=0)sceKernelStartThread(t,0,0);
    ui_cue_generate(cue);int audio=sceKernelCreateThread("music",music,0x15,16384,0,0);if(audio>=0)sceKernelStartThread(audio,0,0);
    sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);sceDisplaySetMode(0,480,272);
    const char *root="disc0:/PSP_GAME/USRDIR/";
    if(!cnfont_load("disc0:/PSP_GAME/USRDIR/CHINESE.FNT")||!tiles_init("disc0:/PSP_GAME/USRDIR/IMAGES/")||(ART_COUNT&&!gallery_load(&album,root)))goto error;
    memset(album.unlocked,1,sizeof(album.unlocked));tiles_set_flush(gpu_budget_flush);
    sceGuInit();gpu_budget_begin(gu_list);sceGuDrawBuffer(GU_PSM_8888,0,512);sceGuDispBuffer(480,272,(void*)(512*272*4),512);sceGuOffset(2048-240,2048-136);sceGuViewport(2048,2048,480,272);sceGuScissor(0,0,480,272);sceGuEnable(GU_SCISSOR_TEST);sceGuDisable(GU_DEPTH_TEST);sceGuFinish();sceGuSync(0,0);sceDisplayWaitVblankStart();sceGuDisplay(1);
    for(int i=0;i<5;i++){char path[64];snprintf(path,sizeof(path),"disc0:/PSP_GAME/USRDIR/%s.BIN",names[i]);files[i]=fopen(path,"rb");if(!files[i])goto error;}
    int node=START_NODE,row=0,cat=0,item=0,menu=0,oldbase=-1,page=0,oldnode=-1,oldrun=-1;unsigned last=0;
    int view=0,sel=0,valid=0,confirm=0,yes=0,occupied[10]={0};uint64_t before=sceKernelGetSystemTimeWide();
    while(running){SceCtrlData pad;sceCtrlPeekBufferPositive(&pad,1);unsigned hit=pad.Buttons&~last;last=pad.Buttons;
        uint64_t now=sceKernelGetSystemTimeWide(),dt=now-before;before=now;int was_menu=menu;
        if(hit && menu)ui_wave_nudge();
        if(view==-4){gallery_tick(&album,(float)dt/1000000);if(ui_page_ready(7)){gallery_input(&album,hit);gallery_stick(&album,pad.Lx,pad.Ly,(float)dt/1000000);}if(!album.opened)view=0;}
        else if(confirm){if(hit&(PSP_CTRL_LEFT|PSP_CTRL_RIGHT))yes=!yes;if(hit&PSP_CTRL_CROSS)confirm=0;else if(hit&PSP_CTRL_CIRCLE){if(yes){ui_notice(writeslot(sel,node,row)?"保存成功":"存档失败");occupied[sel]=valid=readslot(sel);}confirm=0;}}
        else if(hit&PSP_CTRL_START){menu=MENU_COUNT && !menu;cat=item=0;view=0;}
        else if(view){
            if(hit&PSP_CTRL_CROSS)view=0;
            else if(hit&(PSP_CTRL_UP|PSP_CTRL_DOWN)){sel=(sel+10+((hit&PSP_CTRL_UP)?-1:1))%10;valid=readslot(sel);sound_ui_tick();}
            else if(hit&PSP_CTRL_CIRCLE){if(view==-2){if(valid){confirm=1;yes=0;}else{ui_notice(writeslot(sel,node,row)?"保存成功":"存档失败");occupied[sel]=valid=readslot(sel);}}
                else if(valid){node=saved.node;row=saved.row;view=menu=0;}else{ui_notice("尚无存档");}}
        }
        else if(menu){if(hit&PSP_CTRL_CROSS)menu=0;
            else if(hit&(PSP_CTRL_LEFT|PSP_CTRL_RIGHT)){cat=(cat+MENU_COUNT+((hit&PSP_CTRL_LEFT)?-1:1))%MENU_DIVISOR;item=0;}
            else if(hit&(PSP_CTRL_UP|PSP_CTRL_DOWN)){int n=groups[cat].count;item=(item+n+((hit&PSP_CTRL_UP)?-1:1))%n;}
            else if(hit&PSP_CTRL_CIRCLE){int target=actions[groups[cat].first+item];if(target<=-2){view=target;sel=0;if(view==-4)gallery_open(&album);else{for(int i=0;i<10;i++)occupied[i]=readslot(i);valid=readslot(0);}}else{if(target>=0){node=target;row=0;}menu=0;}}}
        else{int n=current.count;if(n>1 && (hit&(PSP_CTRL_UP|PSP_CTRL_DOWN)))row=(row+n+((hit&PSP_CTRL_UP)?-1:1))%n;
            else if(n && (hit&PSP_CTRL_CIRCLE)){node=current.edge[row];row=0;}}
        if(node!=oldnode){if(!fetch(NODE,(uint64_t)node*sizeof(current),&current,sizeof(current))||current.count>4)goto error;oldnode=node;}
        int bg=current.bg,run=current.first+row;
        if(bg!=oldbase||run!=oldrun){if(bg!=oldbase&&!fetch(BACK,(uint64_t)bg*sizeof(base),base,sizeof(base)))goto error;
            for(int y=0;y<272;y++)memcpy(scene_buffer+y*512,base+y*480,960);
            if(!draw(scene_buffer,run))goto error;
            for(int y=0;y<68;y++)for(int x=0;x<120;x++)capture[y*120+x]=scene_buffer[y*4*512+x*4];
            oldbase=bg;oldrun=run;sceKernelDcacheWritebackRange(scene_buffer,sizeof(scene_buffer));}
        ui_page_tick(dt,view==-4?7:view==-2?1:view==-3?2:0,view==-4?album.selection:(unsigned)sel);ui_tick(dt,menu,cat,item);
        gpu_budget_begin(gu_list);sceGuDisable(GU_BLEND);sceGuEnable(GU_TEXTURE_2D);sceGuTexMode(GU_PSM_5650,0,0,0);sceGuTexImage(0,512,512,512,scene_buffer);sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGB);sceGuTexFilter(GU_NEAREST,GU_NEAREST);sceGuTexScale(1,1);sceGuTexOffset(0,0);
        typedef struct {float u,v,x,y,z;} Blit;Blit *v=sceGuGetMemory(2*sizeof(Blit));v[0]=(Blit){0,0,0,0,0};v[1]=(Blit){480,272,480,272,0};sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);sceGuDisable(GU_TEXTURE_2D);
        if(menu&&!was_menu){sceGuFinish();sceGuSync(0,0);ui_blur_capture((char*)sceGeEdramGetAddr()+page);gpu_budget_begin(gu_list);}ui_blur_draw();
        if(menu||ui_visible()){unsigned sub=ui_page();if(sub==7)gallery_draw(&album);else if(sub==1||sub==2){ui_sheet(sub==1?"保存进度":"读取进度","上下 选择　○ 确认　× 返回");for(int i=0;i<10;i++)ui_slot(i,i==sel,occupied[i]?"已保存":"尚无存档");char number[16];snprintf(number,sizeof(number),"%02d / 10",sel+1);ui_sheet_label(362,27,number,14,.65f);ui_detail_begin(sel);if(valid)for(int y=0;y<68;y++)memcpy(preview_pixels+y*128,saved.thumb+y*120,240);ui_save_preview(preview_pixels,valid);ui_detail_end();}else ui_menu(cat,item,"",0);}
        ui_confirm_dialog("覆盖当前存档？",yes,confirm);ui_notice_draw();sceGuFinish();sceGuSync(0,0);tiles_synced();sceDisplayWaitVblankStart();page=(int)(uintptr_t)sceGuSwapBuffers();}
    gallery_close(&album);tiles_close();cnfont_close();sceGuTerm();
    goto done;
error:pspDebugScreenInit();pspDebugScreenPrintf("Invalid or unreadable scene resource");while(running)sceDisplayWaitVblankStart();
done:for(int i=0;i<5;i++)if(files[i])fclose(files[i]);
    if(audio>=0)sceKernelWaitThreadEnd(audio,NULL);
    sceKernelExitGame();return 0;}
