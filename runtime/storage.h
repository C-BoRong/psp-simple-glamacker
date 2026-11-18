#ifndef ATRI_STORAGE_H
#define ATRI_STORAGE_H
#include <stdint.h>
#include <stddef.h>
/* Two complete generations: a failed write never truncates the newest valid save. */
uint32_t storage_hash(const void *data,size_t size);
int storage_load(const char *base,uint32_t identity,void *data,size_t size);
int storage_save(const char *base,uint32_t identity,const void *data,size_t size);
#endif
