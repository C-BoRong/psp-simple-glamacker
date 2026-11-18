#ifndef ATRI_MIXER_H
#define ATRI_MIXER_H
#include <stdint.h>
#include <stdio.h>
#define MIX_TRACKS 24
#define MIX_MENU_TRACK MIX_TRACKS
#define MIX_CHANNELS (MIX_TRACKS+1)
#define MIX_RATE 44100
#define MIX_RAMP 128
#define MIX_DEFAULT_VOLUME 70
typedef struct {
    FILE *file;
    unsigned channels,frames,position,block_position,loop,group,stop_on_silence;
    int32_t volume,target,step,pan;
    unsigned fade_left,fade_total;
    int32_t fade_remainder,fade_error;
    int32_t last[2],tail[2];
    int32_t menu_tone[2];
    unsigned tail_left,attack;
    int16_t decoded[512];
    unsigned char *buffer;
} MixTrack;
typedef struct {
    MixTrack tracks[MIX_CHANNELS];
    unsigned volumes[3],error;
    int32_t group_gain[3],group_target[3];
    unsigned group_left[3];
    uint64_t clock;
    int paused,preview_voice;
    int menu_mode;
    unsigned menu_blend;
} Mixer;
void mixer_init(Mixer *m);
int mixer_play(Mixer *m,unsigned slot,const char *path,unsigned loop,unsigned group,unsigned volume,int pan,unsigned fade_ms);
void mixer_stop(Mixer *m,unsigned slot,unsigned fade_ms);
void mixer_gain(Mixer *m,unsigned slot,unsigned volume,int pan,unsigned fade_ms);
void mixer_close(Mixer *m);
/* Wide output: clipping/limiting must occur once, after every bus and UI cue. */
void mixer_render(Mixer *m,int32_t *stereo,unsigned frames);
int mixer_seek(Mixer *m,unsigned slot,unsigned frame);
#endif
