#ifndef ATRI_RENDER_ALLOC_H
#define ATRI_RENDER_ALLOC_H
#include <malloc.h>
#include "tiles.h"
/* Main/render thread only. Never flush or reclaim GPU-in-flight textures. */
static inline void *render_alloc(unsigned alignment,unsigned bytes){
    (void)alignment;
    void *p=memalign(alignment,bytes);
#ifdef PORT_BUILD
    while(!p&&tiles_trim(65536))p=memalign(alignment,bytes);
#endif
    return p;
}
#endif
