#ifndef ATRI_TILES_H
#define ATRI_TILES_H
/* Bounded tile cache; local coordinates use original source pixels. */
int tiles_init(const char *root);
void tiles_close(void);
void tiles_begin(void);
/* Notify completion of queued GU work before reusing texture storage. */
void tiles_synced(void);
void tiles_set_flush(void (*flush)(void));
unsigned tiles_flushes(void);
int tiles_flush(void);
int tiles_draw(unsigned image,float x,float y,float scale_x,float scale_y,float rotation,
               float clip_x,float clip_y,float clip_w,float clip_h,unsigned color);
unsigned tiles_error(void);
int tiles_io_error(void);
unsigned tiles_loaded(void);
int tiles_opaque(unsigned image);
int tiles_rule(unsigned image,unsigned char *pixels);
void tiles_style(unsigned style);
unsigned tiles_bytes(void);
unsigned tiles_trim(unsigned bytes);
unsigned tiles_peak_frame(void);
unsigned tiles_peak_load_us(void);
#endif
