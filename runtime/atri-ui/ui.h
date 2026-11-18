#ifndef ATRI_UI_H
#define ATRI_UI_H
#include <stdint.h>
#include <stddef.h>
typedef struct {unsigned w,h,tw,th;uint16_t *pixels;} UITexture;
int ui_texture_load(UITexture *t,const char *path);
void ui_texture_close(UITexture *t);
void ui_texture_draw(const UITexture *t,int x,int y,int u,int v,int w,int h,unsigned color);
void ui_title(const UITexture *atlas,unsigned selected,unsigned enabled);
/* Draw only: input dispatch and feature availability belong to the caller. */
void ui_menu(unsigned category,unsigned row,const char *detail,int editing);
void ui_overlay(void);
void ui_highlight(int x,int y,int width,int height);
void ui_tick(uint64_t delta,int open,unsigned category,unsigned row);
int ui_visible(void);
void ui_value(const char *value);
void ui_page_tick(uint64_t delta,unsigned page,unsigned selection);
unsigned ui_page(void);
int ui_page_ready(unsigned page);
void ui_sheet(const char *title,const char *hint);
void ui_sheet_label(float x,float y,const char *text,float size,float alpha);
void ui_sheet_centered(float y,const char *text,float size,float alpha);
void ui_sheet_body(float x,float y,const char *text,size_t offset,unsigned columns,unsigned rows,float size);
void ui_slot(unsigned index,int selected,const char *chapter);
void ui_save_preview(const uint16_t *pixels,int valid);
void ui_setting_row(unsigned index,int selected,const char *name,const char *value);
void ui_meter(float fraction);
void ui_detail_begin(unsigned key);
void ui_detail_end(void);
void ui_start_hint(int visible,int in_game);
void ui_blur_capture(const void *framebuffer);
void ui_blur_draw(void);
unsigned ui_blur_peak_us(void);
void ui_confirm_dialog(const char *message,int yes,int active);
void ui_controls(void);
void ui_notice(const char *message);
void ui_notice_draw(void);
void ui_wave_nudge(void);
void ui_fade_draw(float alpha);
void ui_gallery_preview_begin(void);
void ui_gallery_preview_end(void);
unsigned ui_gallery_color(void);
void ui_gallery_caption(const char *text,float alpha);
void ui_gallery_sheet(void);
void ui_gallery_slot(unsigned index);
void ui_gallery_status(const char *text);
void ui_story_choices_tick(float seconds,unsigned count,unsigned selected);
void ui_story_choice(unsigned index,unsigned count,const char *text,int disabled,unsigned size);
#endif
