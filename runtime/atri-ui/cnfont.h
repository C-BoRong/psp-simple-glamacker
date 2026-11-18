#ifndef ATRI_CNFONT_H
#define ATRI_CNFONT_H
#include "layout.h"
int cnfont_load(const char *path);
int cnfont_ready(void);
const char *cnfont_error(void);
void cnfont_close(void);
int cnfont_draw(int x,int y,const GlyphPosition *glyphs,unsigned count,unsigned color);
int cnfont_scaled(int x,int y,const GlyphPosition *glyphs,unsigned count,unsigned color,unsigned size);
int cnfont_line(int x,int y,const char *text,unsigned color);
int cnfont_story(int x,int y,const GlyphPosition *glyphs,unsigned count,unsigned size);
int cnfont_speaker(int x,int y,const char *text);
int cnfont_smooth(float x,float y,const char *text,unsigned color,float size);
int cnfont_glyphs_smooth(float x,float y,const GlyphPosition *glyphs,unsigned count,unsigned color,float size);
#endif
