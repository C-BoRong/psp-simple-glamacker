#ifndef ATRI_STORY_H
#define ATRI_STORY_H
#include <stdint.h>
#include <stddef.h>
enum {
    ST_TEXT=1,ST_BG,ST_FG,ST_TRANS,ST_HIDE,ST_DELETE,ST_CACHE,ST_SE,ST_WAIT,ST_BGM,
    ST_FGACT,ST_FRAME,ST_QUAKE,ST_JUMP,ST_USER,ST_TITLE,ST_SCENE,ST_VOSTOP,ST_NTCLEAR,
    ST_TROPHY,ST_MSG,ST_BRAND,ST_SELECT,ST_CGACT,ST_FRAMEACT,ST_EVAL,ST_GOTITLE,
    ST_MOVIE,ST_EVSET,ST_TONE,ST_CHECKPOINT,ST_CG,ST_KEYSKIP,ST_LINK
};
typedef struct {uint32_t op,first,count,line,file,block;} StoryCommand;
typedef struct {uint32_t key,value;} StoryParam;
typedef struct {
    void *allocation;
    const StoryCommand *commands;
    const StoryParam *params;
    const uint32_t *files;
    const char *strings;
    uint32_t command_count,param_count,string_bytes,file_count,entry,true_entry;
    char error[160];
} Story;
int story_load(Story *s,const char *path);
void story_close(Story *s);
const char *story_get(const Story *s,const StoryCommand *c,const char *key);
double story_number(const Story *s,const StoryCommand *c,const char *key,double fallback);
int story_has(const Story *s,const StoryCommand *c,const char *key);
uint32_t story_hash(const char *key);
const char *story_file(const Story *s,const StoryCommand *c);
#endif
