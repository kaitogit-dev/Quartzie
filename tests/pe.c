#define _GNU_SOURCE
#define CINMIX_HOST_TEST
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "../src/proc.c"
#define REQUIRE(cond) do { if (!(cond)) { fprintf(stderr,"FAIL pe-test line %d: %s\n",__LINE__,#cond);exit(1); } } while(0)
int main(void) {
    FILE *f=fopen("build/hello.exe","rb"); REQUIRE(f);
    uint8_t pe[4096]={0};
    size_t size=fread(pe,1,sizeof(pe),f); fclose(f);
    REQUIRE(size==2048);
    REQUIRE(mmap((void *)0x03000000u,0x800000,PROT_READ|PROT_WRITE,MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS,-1,0)!=MAP_FAILED);
    REQUIRE(cm_proc_initialize());
    REQUIRE(cm_pe_launches==0);
    REQUIRE(cm_proc_launch_pe(pe,(uint32_t)size)==CM_PE_OK);
    REQUIRE(cm_pe_launches==1);
    REQUIRE(cm_proc_has_work());
    REQUIRE(cm_user_tasks[1].state==CM_THREAD_READY && cm_user_tasks[1].pid==201);
    REQUIRE(((CmInterruptFrame *)(uintptr_t)cm_user_tasks[1].frame)->eip==CM_PE_BASE+0x1000);
    uint8_t *space=(uint8_t *)(uintptr_t)cm_user_tasks[1].physical_base;
    REQUIRE(space[0x1000]==0xff && space[0x1001]==0x15);
    REQUIRE(*(uint32_t *)(void *)(space+0x2070)==CM_PE_BASE+CM_PE_THUNK_OFFSET+CM_PE_THUNK_PID);
    REQUIRE(*(uint32_t *)(void *)(space+0x2074)==CM_PE_BASE+CM_PE_THUNK_OFFSET+CM_PE_THUNK_TICKS);
    REQUIRE(*(uint32_t *)(void *)(space+0x2078)==CM_PE_BASE+CM_PE_THUNK_OFFSET+CM_PE_THUNK_EXIT);
    REQUIRE(space[CM_PE_THUNK_OFFSET+CM_PE_THUNK_PID]==0xb8);
    REQUIRE(space[CM_PE_THUNK_OFFSET+CM_PE_THUNK_EXIT]==0x8b);
    REQUIRE(cm_task_page(1,CM_PE_BASE+0x1000,1)==cm_user_tasks[1].physical_base+0x1000);
    REQUIRE(cm_task_page(0,CM_PE_BASE+0x1000,1)==cm_user_tasks[0].physical_base+0x1000);
    REQUIRE(cm_task_page(1,0x400000,1)==0);
    uint8_t bad[4096];
    memcpy(bad,pe,sizeof(bad)); bad[0]='Q'; REQUIRE(cm_proc_launch_pe(bad,(uint32_t)size)==CM_PE_BAD_IMAGE);
    memcpy(bad,pe,sizeof(bad)); bad[0x84]=0x64; bad[0x85]=0x86;
    REQUIRE(cm_proc_launch_pe(bad,(uint32_t)size)==CM_PE_UNSUPPORTED_MACHINE);
    memcpy(bad,pe,sizeof(bad)); bad[0x98+68]=2;
    REQUIRE(cm_proc_launch_pe(bad,(uint32_t)size)==CM_PE_UNSUPPORTED_RUNTIME);
    memcpy(bad,pe,sizeof(bad)); bad[0x98+96+14*8]=0x80;
    REQUIRE(cm_proc_launch_pe(bad,(uint32_t)size)==CM_PE_UNSUPPORTED_RUNTIME);
    memcpy(bad,pe,sizeof(bad)); bad[0x4a0]='N';
    REQUIRE(cm_proc_launch_pe(bad,(uint32_t)size)==CM_PE_IMPORT_MISSING);
    memcpy(bad,pe,sizeof(bad)); bad[0x4c2]='Z';
    REQUIRE(cm_proc_launch_pe(bad,(uint32_t)size)==CM_PE_IMPORT_MISSING);
    REQUIRE(cm_proc_launch_pe(pe,40)==CM_PE_BAD_IMAGE);
    REQUIRE(cm_proc_launch_pe(pe,(uint32_t)size)==CM_PE_OK);
    cm_proc_run();
    CmInterruptFrame *first=(CmInterruptFrame *)(uintptr_t)cm_user_tasks[0].frame;
    first->eax=CM_SYSCALL_YIELD;
    REQUIRE(cm_proc_interrupt(first,1)==cm_user_tasks[1].frame);
    CmInterruptFrame *second=(CmInterruptFrame *)(uintptr_t)cm_user_tasks[1].frame;
    second->eax=CM_SYSCALL_GETPID;
    REQUIRE(cm_proc_interrupt(second,1)==cm_user_tasks[1].frame);
    REQUIRE(second->eax==202);
    second->eax=CM_SYSCALL_EXIT;
    second->ebx=42;
    REQUIRE(cm_proc_interrupt(second,1)==cm_user_tasks[0].frame);
    REQUIRE(cm_user_tasks[1].state==CM_THREAD_STOPPED);
    REQUIRE(cm_pe_completed==1 && cm_pe_last_exit==42);
    REQUIRE(cm_proc_has_work()==0);

    REQUIRE(cm_proc_active_tasks()==1);
    REQUIRE(cm_proc_launch_pe(pe,(uint32_t)size)==CM_PE_OK);
    REQUIRE(cm_user_tasks[1].pid==203 && cm_pe_launches==3);
    puts("PASS: real PE32 image layout and entry, KERNEL32 import resolution, ring3 launch, i386/GUI/CLR rejection, invalid-image safety, syscall execution and process exit/relaunch");
    return 0;
}
