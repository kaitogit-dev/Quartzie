#define _GNU_SOURCE 1
#include "pe64.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
static uint32_t writes;
static char output[128];
static uint32_t __attribute__((ms_abi)) win_ticks(void){return 5;}
static void * __attribute__((ms_abi)) win_handle(uint32_t which){return which==0xfffffff5u?(void *)(uintptr_t)1:0;}
static int __attribute__((ms_abi)) win_write(void *handle,const void *text,uint32_t length,uint32_t *written,void *overlapped){
    (void)overlapped;
    if(handle!=(void *)(uintptr_t)1||length>sizeof(output)-1)return 0;
    memcpy(output,text,length);output[length]=0;
    if(written)*written=length;
    writes++;
    return 1;
}
static void *resolver(void *context,const char *dll,const char *name,uint16_t ordinal){
    (void)context;
    if(ordinal||!name)return 0;
    if(strcmp(dll,"KERNEL32.dll")!=0)return 0;
    if(strcmp(name,"GetTickCount")==0)return (void *)(uintptr_t)&win_ticks;
    if(strcmp(name,"GetStdHandle")==0)return (void *)(uintptr_t)&win_handle;
    if(strcmp(name,"WriteFile")==0)return (void *)(uintptr_t)&win_write;
    return 0;
}
#define CHECK(condition) do{if(!(condition)){fprintf(stderr,"FAIL PE loader at %d: %s\n",__LINE__,#condition);exit(1);}}while(0)
int main(int argc,char **argv){
    CHECK(argc==2);
    FILE *f=fopen(argv[1],"rb");CHECK(f!=0);
    CHECK(fseek(f,0,SEEK_END)==0);
    long n=ftell(f);CHECK(n>0&&n<1024*1024);
    CHECK(fseek(f,0,SEEK_SET)==0);
    uint8_t *data=malloc((size_t)n);CHECK(data!=0);
    CHECK(fread(data,1,(size_t)n,f)==(size_t)n);
    CHECK(fclose(f)==0);
    void *image=mmap(0,1024*1024,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    CHECK(image!=MAP_FAILED);
    QPe64Resolver host={0,resolver};
    QPe64Image loaded={0};
    CHECK(q_pe64_load(data,(size_t)n,image,1024*1024,&host,&loaded)==Q_PE_OK);
    CHECK(loaded.imports==3);
    uint32_t (__attribute__((ms_abi)) *entry)(void)=(uint32_t (__attribute__((ms_abi)) *)(void))q_pe64_export(&loaded,"QuartzieTest");
    CHECK(entry!=0&&entry()==42);
    void (__attribute__((ms_abi)) *startup)(void)=(void (__attribute__((ms_abi)) *)(void))q_pe64_entry(&loaded);
    CHECK(startup!=0);
    startup();
    CHECK(writes==1&&strcmp(output,"Quartzie Win32 PE64 app\r\n")==0);
    data[0]=0;
    CHECK(q_pe64_load(data,(size_t)n,image,1024*1024,&host,&loaded)==Q_PE_INVALID);
    free(data);
    CHECK(munmap(image,1024*1024)==0);
    puts("PASS: real Windows PE32+ EXE, relocations, KERNEL32 imports, entrypoint and exports");
    return 0;
}
