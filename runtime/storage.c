#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
uint32_t storage_hash(const void *data,size_t size){const unsigned char *p=data;uint32_t h=2166136261u;while(size--)h=(h^*p++)*16777619u;return h;}
static int read_generation(const char *base,unsigned generation,uint32_t identity,void *data,size_t size,uint32_t *sequence){
    char path[256];if(snprintf(path,sizeof(path),"%s.%u.dat",base,generation)>=(int)sizeof(path))return 0;
    FILE *f=fopen(path,"rb");if(!f)return 0;uint32_t h[6];
    int ok=fread(h,1,sizeof(h),f)==sizeof(h)&&!memcmp(h,"ASV1",4)&&h[1]==1&&h[2]==identity&&h[3]==size&&h[4]>0;
    if(ok)ok=fread(data,1,size,f)==size&&fgetc(f)==EOF&&storage_hash(data,size)==h[5];
    fclose(f);if(ok)*sequence=h[4];return ok;
}
int storage_load(const char *base,uint32_t identity,void *data,size_t size){
    void *temp=malloc(size);if(!temp)return 0;uint32_t best=0,sequence=0;
    for(unsigned i=0;i<2;++i)if(read_generation(base,i,identity,temp,size,&sequence)&&sequence>best){memcpy(data,temp,size);best=sequence;}
    free(temp);return best!=0;
}
int storage_save(const char *base,uint32_t identity,const void *data,size_t size){
    if(size>0xffffffffu)return 0;
    void *temp=malloc(size);if(!temp)return 0;uint32_t sequences[2]={0,0};
    for(unsigned i=0;i<2;++i)read_generation(base,i,identity,temp,size,&sequences[i]);
    unsigned generation=sequences[0]<=sequences[1]?0:1;uint32_t sequence=(sequences[0]>sequences[1]?sequences[0]:sequences[1])+1;
    if(!sequence){free(temp);return 0;}
    char path[256];if(snprintf(path,sizeof(path),"%s.%u.dat",base,generation)>=(int)sizeof(path)){free(temp);return 0;}
    FILE *f=fopen(path,"wb");if(!f){free(temp);return 0;}
    uint32_t h[6]={0,1,identity,(uint32_t)size,sequence,storage_hash(data,size)};memcpy(h,"ASV1",4);
    int ok=fwrite(h,1,sizeof(h),f)==sizeof(h)&&fwrite(data,1,size,f)==size;
    if(fflush(f))ok=0;
    if(fclose(f))ok=0;
    uint32_t checked=0;
    if(ok)ok=read_generation(base,generation,identity,temp,size,&checked)&&checked==sequence&&!memcmp(temp,data,size);
    free(temp);return ok;
}
