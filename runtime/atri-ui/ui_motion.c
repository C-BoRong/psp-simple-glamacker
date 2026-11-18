#include "ui_motion.h"
#include <math.h>
void ui_fade_begin(UIFade *f,int cover){f->stage=cover?2:1;f->elapsed=0;f->alpha=cover?0:1;}
int ui_fade_step(UIFade *f,float seconds){
    if(!f->stage)return 0;
    /* Switch only after a fully black frame has reached the display. */
    if(f->stage==3){ui_fade_begin(f,0);return 1;}
    f->elapsed+=fminf(.05f,fmaxf(0,seconds));
    float t=fminf(1,f->elapsed/(f->stage==2?.35f:.65f));
    float eased=t*t*t*(t*(t*6-15)+10);
    f->alpha=f->stage==2?eased:1-eased;
    if(t>=1)f->stage=f->stage==2?3:0;
    return 0;
}
static void approach(UIAxis *a,float target,float dt,float rate){
    float distance=a->value-target,b=a->velocity+rate*distance,decay=expf(-rate*dt);
    a->value=target+(distance+b*dt)*decay;a->velocity=(a->velocity-rate*b*dt)*decay;
    if(fabsf(a->value-target)<.0005f&&fabsf(a->velocity)<.005f){a->value=target;a->velocity=0;}
}
void ui_motion_step(UIMotion *m,float dt,int open,unsigned category,unsigned row){
    if(dt<0)dt=0;
    if(dt>.05f)dt=.05f;
    if(category>3)category=3;
    static const unsigned counts[]={8,3,3,6};unsigned rows=counts[category];row%=rows;
    if(open&&!m->open&&m->visibility.value<.01f){m->horizontal=(UIAxis){(float)category,0};m->vertical=(UIAxis){(float)row,0};m->category=category;m->items=0;}
    if(category!=m->category){m->vertical=(UIAxis){(float)row,0};m->category=category;m->items=0;}
    /* Selection wraps at the endpoints; layout remains a finite list. */
    approach(&m->horizontal,(float)category,dt,20);approach(&m->vertical,(float)row,dt,22);approach(&m->visibility,open?1:0,dt,22);
    m->items=fminf(1,m->items+dt*5);
    /* Input refreshes a bounded drive, never phase or velocity. Smooth speed
       follows its decay; trapezoidal integration preserves the wave shape. */
    float old_speed=m->wave_speed.value,decay=expf(-3*dt);
    approach(&m->wave_speed,m->wave_drive*expf(-1.5f*dt),dt,10);
    m->wave_drive*=decay;
    float wave_dt=dt*(1+3.6f*(old_speed+m->wave_speed.value)*.5f);
    m->phase=fmodf(m->phase+wave_dt*.23f,6.283185307f);
    m->wave_b=fmodf(m->wave_b+wave_dt*.161f,6.283185307f);
    m->pulse=fmodf(m->pulse+dt*1.8f,6.283185307f);m->open=open;
}
void ui_motion_nudge(UIMotion *m){m->wave_drive=1;}
void ui_focus_step(UIAxis *axis,float seconds,int selected){approach(axis,selected?1:0,fminf(.05f,fmaxf(0,seconds)),19);}
void ui_page_step(UIMotion *m,float dt,unsigned page,unsigned selection){
    dt=fminf(.05f,fmaxf(0,dt));
    if(m->visibility.value<.01f){m->page=page;m->page_alpha=(UIAxis){1,0};m->list=(UIAxis){(float)selection,0};}
    approach(&m->page_alpha,m->page==page?1:0,dt,32);
    if(m->page!=page&&m->page_alpha.value<.025f){m->page=page;m->page_alpha=(UIAxis){0,0};m->list=(UIAxis){(float)selection,0};}
    approach(&m->list,(float)selection,dt,20);
}
float ui_wave_y(const UIMotion *m,float x,unsigned layer){
    return 176+layer*13+sinf(x*.009f+m->phase+layer*.9f)*23+sinf(x*.016f-m->wave_b-layer*.63f)*9;
}
