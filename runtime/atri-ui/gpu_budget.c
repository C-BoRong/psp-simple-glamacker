#include "gpu_budget.h"
#include <pspgu.h>
#include <assert.h>
/* All runtime allocation sites request at most 2080 bytes. Leave room for
   intervening state commands, Clear's internal vertices and Finish. */
#define RESERVE (32u*1024u)
static void *buffer,*target;
static int target_set,target_format,target_width;
static unsigned peak,splits;
void *__real_sceGuGetMemory(int size);
void __real_sceGuDrawBufferList(int format,void *address,int width);
static unsigned sample(void){unsigned n=(unsigned)sceGuCheckList();if(n>peak)peak=n;return n;}
void gpu_budget_begin(void *list){buffer=list;target_set=0;sceGuStart(GU_DIRECT,list);}
void gpu_budget_flush(void){
    sample();sceGuFinish();sceGuSync(0,0);sceGuStart(GU_DIRECT,buffer);
    /* Start resets the framebuffer pointer. Preserve offscreen blur/mask targets. */
    if(target_set)__real_sceGuDrawBufferList(target_format,target,target_width);
    /* Do not clear tile pins here: a texture may already have been fetched for
       the draw whose vertex allocation triggered this split. */
}
void *__wrap_sceGuGetMemory(int size){
    assert(size>=0&&(unsigned)size<RESERVE);
    if(sample()+((unsigned)size+15u)+RESERVE>=GPU_LIST_BYTES){gpu_budget_flush();++splits;}
    return __real_sceGuGetMemory(size);
}
void __wrap_sceGuDrawBufferList(int format,void *address,int width){
    target_set=1;target_format=format;target=address;target_width=width;
    __real_sceGuDrawBufferList(format,address,width);
}
unsigned gpu_budget_peak(void){return peak;}
unsigned gpu_budget_splits(void){return splits;}
