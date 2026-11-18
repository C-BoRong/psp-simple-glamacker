#ifndef ATRI_SOUND_H
#define ATRI_SOUND_H
#include "mixer.h"
int sound_init(void);
void sound_shutdown(void);
int sound_play(unsigned slot,const char *path,unsigned loop,unsigned group,unsigned volume,int pan,unsigned fade_ms);
void sound_stop(unsigned slot,unsigned fade_ms);
int sound_continue(unsigned slot,const char *path,unsigned loop,unsigned volume,int pan,unsigned fade_ms);
void sound_pause(int paused);
int sound_menu(int enabled,const char *music);
void sound_volume(unsigned group,unsigned percent);
int sound_active(unsigned slot);
unsigned sound_error(void);
unsigned sound_peak_us(void);
unsigned sound_lock_peak_us(void);
unsigned sound_late_blocks(void);
unsigned sound_output_gap_us(void);
unsigned sound_underruns(void);
unsigned sound_limited_blocks(void);
typedef struct {
    char path[192];
    unsigned position,loop,group,volume;
    int pan;
    int32_t gain,target,step,remainder,fade_error;
    unsigned fade_left,fade_total,stop_on_silence;
} SoundState;
int sound_snapshot(SoundState states[MIX_TRACKS]);
int sound_restore(const SoundState states[MIX_TRACKS]);
void sound_preview(int enabled);
void sound_ui_tick(void);
#endif
