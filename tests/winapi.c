#define _GNU_SOURCE 1
#include "winapi.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
static QNtKernel kernel;
static uint32_t page_allocations,page_frees,pool_allocations,pool_frees;
static char output[128];
static size_t length;
static EFI_STATUS EFIAPI allocate_pages(uint32_t how,uint32_t memory_type,uint64_t pages,uint64_t *address){
    if(how!=0||memory_type!=2||!pages||pages>16384||!address)return 1;
    void *p=mmap(0,(size_t)(pages*4096),PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(p==MAP_FAILED)return 1;
    *address=(uint64_t)(uintptr_t)p;page_allocations++;
    return 0;
}
static EFI_STATUS EFIAPI free_pages(uint64_t address,uint64_t pages){
    if(!address||!pages)return 1;
    if(munmap((void *)(uintptr_t)address,(size_t)(pages*4096))!=0)return 1;
    page_frees++;return 0;
}
static EFI_STATUS EFIAPI allocate_pool(uint32_t memory_type,uint64_t bytes,void **result){
    if(memory_type!=2||!bytes||!result)return 1;
    *result=malloc((size_t)bytes);
    if(!*result)return 1;
    pool_allocations++;return 0;
}
static EFI_STATUS EFIAPI free_pool(void *p){
    if(!p)return 1;
    free(p);pool_frees++;return 0;
}
static void capture(uint8_t byte){
    if(length+1<sizeof(output)){output[length++]=(char)byte;output[length]=0;}
}
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL WinAPI line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
int main(int argc,char **argv){
    CHECK(argc==2);
    FILE *f=fopen(argv[1],"rb");CHECK(f!=0);
    CHECK(fseek(f,0,SEEK_END)==0);
    long n=ftell(f);CHECK(n>0&&n<1048576);
    CHECK(fseek(f,0,SEEK_SET)==0);
    uint8_t *exe=malloc((size_t)n);CHECK(exe!=0);
    CHECK(fread(exe,1,(size_t)n,f)==(size_t)n);
    fclose(f);
    EFI_BOOT_SERVICES services={0};
    services.AllocatePages=allocate_pages;
    services.FreePages=free_pages;
    services.AllocatePool=allocate_pool;
    services.FreePool=free_pool;
    qnt_init(&kernel);
    CHECK(qwin_execute(&kernel,&services,exe,(size_t)n,capture)==QNT_OK);
    CHECK(strcmp(output,"Quartzie Win32 PE64 app\r\n")==0);
    CHECK(page_allocations==1&&page_frees==1);
    CHECK(pool_allocations==1&&pool_frees==1);
    for(int i=0;i<QNT_PROCESSES;i++)CHECK(!kernel.processes[i].alive);
    exe[0]=0;
    CHECK(qwin_execute(&kernel,&services,exe,(size_t)n,capture)==Q_PE_INVALID);
    CHECK(page_allocations==page_frees&&pool_allocations==pool_frees);
    free(exe);
    puts("PASS: Win32 heap, error state, console, dynamic PE pages and process cleanup");
    return 0;
}
