#ifndef ATRI_TRACE_H
#define ATRI_TRACE_H
#define TRACE_ROWS 12
#define TRACE_COLUMNS 64
#if defined(__psp__) || defined(TRACE_TEST)
void trace_reset(void);
void trace_add(const char *format,...);
void trace_freeze(void);
unsigned trace_count(void);
const char *trace_line(unsigned index);
#else
static inline void trace_reset(void){}
static inline void trace_add(const char *format,...){(void)format;}
static inline void trace_freeze(void){}
static inline unsigned trace_count(void){return 0;}
static inline const char *trace_line(unsigned index){(void)index;return "";}
#endif
#endif
