#include "layout.h"

static int decode(const unsigned char *s,uint32_t *cp) {
    if(s[0]<128) {*cp=s[0];return s[0]?1:0;}
    unsigned n;uint32_t c,min;
    if(s[0]>=0xc2&&s[0]<=0xdf) {n=2;c=s[0]&31;min=128;}
    else if(s[0]>=0xe0&&s[0]<=0xef) {n=3;c=s[0]&15;min=2048;}
    else if(s[0]>=0xf0&&s[0]<=0xf4) {n=4;c=s[0]&7;min=65536;}
    else return -1;
    for(unsigned i=1;i<n;++i) {if((s[i]&0xc0)!=0x80) return -1;c=(c<<6)|(s[i]&63);}
    if(c<min||c>0x10ffff||(c>=0xd800&&c<=0xdfff)) return -1;
    *cp=c;return (int)n;
}
int layout_page(const char *text,size_t offset,unsigned columns,unsigned rows,int novel,
                GlyphPosition *out,unsigned capacity,unsigned *count,size_t *next) {
    unsigned x=0,y=0;size_t p=offset;*count=0;*next=offset;
    if(!columns||!rows) return 0;
    while(text[p]) {
        uint32_t cp;int bytes=decode((const unsigned char*)text+p,&cp);
        if(bytes<1) return 0;
        if(cp==31&&!novel) {p+=(size_t)bytes;continue;}
        if(cp==10||cp==31) {
            p+=(size_t)bytes;
            if(x) {x=0;y++;}
            else if(novel) y++;
            if(y>=rows) break;
            continue;
        }
        if(cp<32) return 0;
        if(x==columns) {x=0;y++;}
        if(y>=rows) break;
        if(*count==capacity) return 0;
        out[(*count)++]=(GlyphPosition){cp,x++,y};p+=(size_t)bytes;
    }
    *next=p;return 1;
}
