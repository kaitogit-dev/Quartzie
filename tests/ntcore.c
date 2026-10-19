#include "ntcore.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static QNtKernel kernel;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL NT core line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int device(void *context,uint32_t code,void *buffer,size_t length){
    int *calls=(int *)context;
    (*calls)++;
    if(buffer&&length)*((uint8_t *)buffer)=(uint8_t)code;
    return 7;
}
int main(void){
    qnt_init(&kernel);
    uint32_t a=0,b=0,t1=0,t2=0;
    CHECK(qnt_process_create(&kernel,&a)==QNT_OK);
    CHECK(qnt_process_create(&kernel,&b)==QNT_OK&&a!=b);
    CHECK(qnt_vm_map(&kernel,a,0x100000,0x4000,QNT_VM_USER|QNT_VM_READ|QNT_VM_EXECUTE)==QNT_OK);
    CHECK(qnt_vm_map(&kernel,a,0x102000,0x1000,QNT_VM_USER|QNT_VM_READ)==QNT_BUSY);
    CHECK(qnt_vm_map(&kernel,a,0x200000,0x1000,QNT_VM_WRITE|QNT_VM_EXECUTE)==QNT_ACCESS);
    QNtRegion region={0};
    CHECK(qnt_vm_query(&kernel,a,0x102300,&region)==QNT_OK&&region.base==0x100000);
    CHECK(qnt_thread_create(&kernel,a,0x102000,0x204000,&t1)==QNT_OK);
    CHECK(qnt_thread_create(&kernel,b,0x105000,0x205000,&t2)==QNT_OK);
    CHECK(t1!=t2&&qnt_schedule(&kernel)==(int)t1);
    for(int i=0;i<4;i++)qnt_tick(&kernel);
    CHECK(kernel.current_tid==t2&&kernel.switches>=2);
    CHECK(qnt_syscall(&kernel,a,QNT_SC_GETPID,0,0,0,0)==a);
    CHECK(qnt_syscall(&kernel,a,QNT_SC_TICKS,0,0,0,0)==4);
    uint64_t handle=0,other=0;
    CHECK(qnt_file_create(&kernel,a,"C:\\Temp\\hello.txt",&handle)==QNT_OK);
    size_t count=0;
    CHECK(qnt_file_write(&kernel,a,handle,0,"Quartzie",8,&count)==QNT_OK&&count==8);
    CHECK(qnt_object_lookup(&kernel,b,handle,QNT_FILE,(uint32_t *)&count)==QNT_ACCESS);
    CHECK(qnt_file_open(&kernel,b,"C:\\Temp\\hello.txt",&other)==QNT_OK);
    char data[20]={0};
    CHECK(qnt_file_read(&kernel,b,other,0,data,20,&count)==QNT_OK&&count==8);
    CHECK(memcmp(data,"Quartzie",8)==0);
    CHECK(qnt_object_close(&kernel,a,handle)==QNT_OK);
    CHECK(qnt_object_close(&kernel,a,handle)==QNT_ACCESS);
    int calls=0;
    uint32_t driver=0;
    CHECK(qnt_driver_register(&kernel,"virtual-display",device,&calls,&driver)==QNT_OK);
    uint8_t value=0;
    CHECK(qnt_driver_request(&kernel,driver,19,&value,1)==7&&calls==1&&value==19);
    CHECK(qnt_syscall(&kernel,b,QNT_SC_WRITE,other,8,(uint64_t)(uintptr_t)" NT",3)==3);
    memset(data,0,sizeof(data));
    CHECK(qnt_syscall(&kernel,b,QNT_SC_READ,other,0,(uint64_t)(uintptr_t)data,11)==11);
    CHECK(memcmp(data,"Quartzie NT",11)==0);
    CHECK(qnt_syscall(&kernel,b,QNT_SC_CLOSE,other,0,0,0)==QNT_OK);
    CHECK(qnt_process_destroy(&kernel,a)==QNT_OK);
    CHECK(qnt_vm_query(&kernel,a,0x102000,&region)==QNT_INVALID);
    puts("PASS: process objects, scheduler, VM rules, handles, RAM files, devices and syscalls");
    return 0;
}
