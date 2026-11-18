#include "trace.h"
#if defined(__psp__) || defined(TRACE_TEST)
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
/* Main thread only: no allocation, file I/O, locks or audio-worker logging. */
static char rows[TRACE_ROWS][TRACE_COLUMNS+1];
static unsigned count,next,sequence;
static int frozen;
void trace_reset(void){count=next=sequence=0;frozen=0;}
void trace_add(const char *format,...){
    if(frozen)return;
    int saved_errno=errno;
    char *line=rows[next];
    int prefix=snprintf(line,TRACE_COLUMNS+1,"%04u ",++sequence%10000);
    va_list args;va_start(args,format);
    vsnprintf(line+prefix,TRACE_COLUMNS+1-(unsigned)prefix,format,args);va_end(args);
    for(unsigned i=0;line[i];++i)if((unsigned char)line[i]<32||(unsigned char)line[i]>126)line[i]='?';
    next=(next+1)%TRACE_ROWS;if(count<TRACE_ROWS)++count;
    errno=saved_errno;
}
void trace_freeze(void){frozen=1;}
unsigned trace_count(void){return count;}
const char *trace_line(unsigned index){return index<count?rows[(next+TRACE_ROWS-count+index)%TRACE_ROWS]:"";}
#endif
