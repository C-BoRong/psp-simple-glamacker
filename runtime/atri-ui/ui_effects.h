#ifndef ATRI_UI_EFFECTS_H
#define ATRI_UI_EFFECTS_H
#include <stdint.h>
#define UI_CUE_FRAMES 1323
/* Fixed 480x272 RGBA source with 512-pixel stride; 128x128 output. */
void ui_blur_pixels(const uint32_t *source,uint32_t *output,uint32_t *scratch);
void ui_cue_generate(int16_t samples[UI_CUE_FRAMES]);
void ui_cue_mix(int32_t *stereo,unsigned frames,const int16_t *cue,unsigned *position,unsigned volume);
#endif
