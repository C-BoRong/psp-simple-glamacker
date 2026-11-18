#include "story.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

typedef char command_abi[(sizeof(StoryCommand)==24)?1:-1];
typedef char param_abi[(sizeof(StoryParam)==8)?1:-1];
uint32_t story_hash(const char *key) {
    uint32_t h=2166136261u;while(*key) h=(h^(unsigned char)*key++)*16777619u;return h;
}
static int fail(Story *s,const char *message) {
    free(s->allocation);s->allocation=0;
    snprintf(s->error,sizeof(s->error),"%s",message);return 0;
}
void story_close(Story *s) {free(s->allocation);memset(s,0,sizeof(*s));}
int story_load(Story *s,const char *path) {
    memset(s,0,sizeof(*s));
    FILE *f=fopen(path,"rb");if(!f) return fail(s,"Cannot open story package");
    uint32_t h[16];
    int ok=fread(h,1,64,f)==64&&!memcmp(h,"ATS2",4)&&h[1]==2;
    if(!ok||!h[2]||h[2]>65535||h[3]>1000000||!h[4]||h[4]>8*1024*1024||!h[5]||h[5]>256||h[6]>=h[2]||h[7]>=h[2]) {
        fclose(f);return fail(s,"Invalid story header");
    }
    size_t bytes=(size_t)h[2]*24+(size_t)h[3]*8+(size_t)h[5]*4+h[4];
    s->allocation=malloc(bytes);
    if(!s->allocation) {fclose(f);return fail(s,"Story memory budget exceeded");}
    ok=fread(s->allocation,1,bytes,f)==bytes&&fgetc(f)==EOF;fclose(f);
    if(!ok) return fail(s,"Truncated or trailing story data");
    s->command_count=h[2];s->param_count=h[3];s->string_bytes=h[4];s->file_count=h[5];s->entry=h[6];s->true_entry=h[7];
    s->commands=s->allocation;s->params=(const StoryParam*)(s->commands+h[2]);
    s->files=(const uint32_t*)(s->params+h[3]);s->strings=(const char*)(s->files+h[5]);
    if(s->strings[h[4]-1]||s->strings[0]) return fail(s,"Invalid string pool");
    for(unsigned i=0;i<s->param_count;++i) {
        uint32_t offset=s->params[i].value;
        if(offset>=h[4]||(offset&&s->strings[offset-1])) return fail(s,"Invalid parameter string offset");
    }
    for(unsigned i=0;i<s->file_count;++i)
        if(s->files[i]>=h[4]||(s->files[i]&&s->strings[s->files[i]-1])) return fail(s,"Invalid filename offset");
    for(unsigned i=0;i<s->command_count;++i) {
        const StoryCommand *c=&s->commands[i];
        if(!c->op||c->op>ST_LINK||c->count>128||c->first>s->param_count||c->count>s->param_count-c->first||c->file>=h[5])
            return fail(s,"Invalid command record");
        for(unsigned j=0;j<c->count;++j) for(unsigned k=0;k<j;++k)
            if(s->params[c->first+j].key==s->params[c->first+k].key) return fail(s,"Duplicate command parameter");
        if(story_has(s,c,"target")) {
            double n=story_number(s,c,"target",-1);
            if(n<0||floor(n)!=n||(n>=h[2]&&!(c->op==ST_LINK&&n==4294967295.0))) return fail(s,"Invalid control-flow target");
        }
    }
    return 1;
}
const char *story_get(const Story *s,const StoryCommand *c,const char *key) {
    uint32_t h=story_hash(key);
    for(unsigned i=0;i<c->count;++i) {const StoryParam *p=&s->params[c->first+i];if(p->key==h) return s->strings+p->value;}
    return "";
}
int story_has(const Story *s,const StoryCommand *c,const char *key) {
    uint32_t h=story_hash(key);for(unsigned i=0;i<c->count;++i) if(s->params[c->first+i].key==h) return 1;return 0;
}
double story_number(const Story *s,const StoryCommand *c,const char *key,double fallback) {
    const char *p=story_get(s,c,key);if(!*p) return fallback;
    char *end;errno=0;double n=strtod(p,&end);return errno||*end||!isfinite(n)?fallback:n;
}
const char *story_file(const Story *s,const StoryCommand *c) {return s->strings+s->files[c->file];}
