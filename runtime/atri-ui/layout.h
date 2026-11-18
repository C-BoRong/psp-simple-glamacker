#ifndef ATRI_LAYOUT_H
#define ATRI_LAYOUT_H
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t codepoint; unsigned column,row; } GlyphPosition;
/* Byte offset into UTF-8; no truncation or loss at a page boundary. */
int layout_page(const char *text,size_t offset,unsigned columns,unsigned rows,int novel,
                GlyphPosition *out,unsigned capacity,unsigned *count,size_t *next);
#endif
