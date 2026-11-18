#ifndef ATRI_UI_MOTION_H
#define ATRI_UI_MOTION_H
typedef struct {float value,velocity;} UIAxis;
/* 0 idle, 1 reveal, 2 cover, 3 fully covered (one presented frame). */
typedef struct {unsigned stage;float elapsed,alpha;} UIFade;
void ui_fade_begin(UIFade *fade,int cover);
int ui_fade_step(UIFade *fade,float seconds);
typedef struct {UIAxis horizontal,vertical,visibility,page_alpha,list,wave_speed;float phase,wave_b,pulse,items,wave_drive;unsigned category,page;int open;} UIMotion;
void ui_motion_step(UIMotion *m,float seconds,int open,unsigned category,unsigned row);
void ui_page_step(UIMotion *m,float seconds,unsigned page,unsigned selection);
float ui_wave_y(const UIMotion *m,float x,unsigned layer);
void ui_motion_nudge(UIMotion *m);
void ui_focus_step(UIAxis *axis,float seconds,int selected);
#endif
