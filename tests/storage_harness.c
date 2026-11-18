#include "storage.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"storage assertion line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv){
    CHECK(argc==2);const char *base=argv[1];char out[16]={0},a[16]="first",b[16]="second",path[256];
    CHECK(!storage_load(base,123,out,sizeof(out)));
    CHECK(storage_save(base,123,a,sizeof(a)));CHECK(storage_load(base,123,out,sizeof(out)));CHECK(!memcmp(out,a,sizeof(a)));
    CHECK(storage_save(base,123,b,sizeof(b)));CHECK(storage_load(base,123,out,sizeof(out)));CHECK(!memcmp(out,b,sizeof(b)));
    CHECK(!storage_load(base,124,out,sizeof(out)));CHECK(!storage_load(base,123,out,sizeof(out)-1));
    snprintf(path,sizeof(path),"%s.1.dat",base);FILE *f=fopen(path,"wb");CHECK(f);CHECK(fwrite("partial",1,7,f)==7);CHECK(!fclose(f));
    CHECK(storage_load(base,123,out,sizeof(out)));CHECK(!memcmp(out,a,sizeof(a)));
    CHECK(storage_save(base,123,b,sizeof(b)));CHECK(storage_load(base,123,out,sizeof(out)));CHECK(!memcmp(out,b,sizeof(b)));
    snprintf(path,sizeof(path),"%s.1.dat",base);f=fopen(path,"r+b");CHECK(f);CHECK(!fseek(f,24,SEEK_SET));CHECK(fputc('X',f)!=EOF);CHECK(!fclose(f));
    CHECK(storage_load(base,123,out,sizeof(out)));CHECK(!memcmp(out,a,sizeof(a)));
    puts("Two-generation save roundtrip, interrupted write, checksum and identity checks passed");return 0;
}
