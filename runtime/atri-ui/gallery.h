#ifndef ATRI_GALLERY_H
#define ATRI_GALLERY_H
#include "ui.h"
#include "ui_motion.h"
#include "story.h"
typedef struct {int32_t group,image,width,height,background,bw,bh;char name[32];} GalleryEntry;
typedef struct {
    GalleryEntry entries[128];unsigned count,selection,variant,groups;
    int opened,viewer,zoom;float pan,view_pan,pan_x,view_pan_x;UIAxis view_zoom;
    UIFade transition;unsigned pending_variant;int pending_view;
    const char *status;
    unsigned char unlocked[128];
    UIAxis counter_alpha;unsigned counter_selection;
    UIAxis hint_alpha;float hint_idle;
} Gallery;
int gallery_load(Gallery *g,const char *root);
void gallery_close(Gallery *g);
void gallery_open(Gallery *g);
void gallery_progress(Gallery *g,const Story *story,const unsigned char *read);
void gallery_leave(Gallery *g);
void gallery_input(Gallery *g,unsigned pressed);
void gallery_tick(Gallery *g,float seconds);
void gallery_stick(Gallery *g,unsigned x,unsigned y,float seconds);
void gallery_draw(Gallery *g);
#endif
