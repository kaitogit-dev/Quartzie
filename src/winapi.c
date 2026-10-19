#include "winapi.h"
#include "pe64.h"
static QNtKernel *running_kernel;
static QWinSerial running_serial;
static uint32_t running_pid;
static uint8_t *module_base;
static int ascii_equal(const char *a,const char *b){
    if(!a||!b)return 0;
    while(*a&&*b){
        char ac=*a++,bc=*b++;
        if(ac>='A'&&ac<='Z')ac=(char)(ac+32);
        if(bc>='A'&&bc<='Z')bc=(char)(bc+32);
        if(ac!=bc)return 0;
    }
    return *a==*b;
}
static uint32_t EFIAPI win_ticks(void){
    return running_kernel?(uint32_t)running_kernel->clock_ticks:0;
}
static void * EFIAPI win_stdout(uint32_t standard){
    return standard==0xfffffff5u||(standard==0xfffffff4u)?(void *)(uintptr_t)1:0;
}
static int EFIAPI win_write(void *handle,const void *buffer,uint32_t length,uint32_t *written,void *overlapped){
    (void)overlapped;
    if(written)*written=0;
    if(handle!=(void *)(uintptr_t)1||!buffer||!running_serial||length>32768)return 0;
    for(uint32_t i=0;i<length;i++)running_serial(((const uint8_t *)buffer)[i]);
    if(written)*written=length;
    return 1;
}
static uint32_t EFIAPI win_last_error(void){return 0;}
static uint32_t EFIAPI win_pid(void){return running_pid;}
static uint32_t EFIAPI win_tid(void){return running_kernel?running_kernel->current_tid:0;}
static char command_line[]="Quartzie.exe";
static char * EFIAPI win_command_line(void){return command_line;}
static void * EFIAPI win_module(void *name){return name?0:module_base;}
static void *resolve_symbol(void *context,const char *dll,const char *symbol,uint16_t ordinal){
    (void)context;
    if(ordinal||!symbol||!(ascii_equal(dll,"kernel32.dll")||ascii_equal(dll,"kernelbase.dll")))return 0;
    if(ascii_equal(symbol,"GetTickCount"))return (void *)(uintptr_t)&win_ticks;
    if(ascii_equal(symbol,"GetStdHandle"))return (void *)(uintptr_t)&win_stdout;
    if(ascii_equal(symbol,"WriteFile"))return (void *)(uintptr_t)&win_write;
    if(ascii_equal(symbol,"GetLastError"))return (void *)(uintptr_t)&win_last_error;
    if(ascii_equal(symbol,"GetCurrentProcessId"))return (void *)(uintptr_t)&win_pid;
    if(ascii_equal(symbol,"GetCurrentThreadId"))return (void *)(uintptr_t)&win_tid;
    if(ascii_equal(symbol,"GetCommandLineA"))return (void *)(uintptr_t)&win_command_line;
    if(ascii_equal(symbol,"GetModuleHandleA"))return (void *)(uintptr_t)&win_module;
    return 0;
}
int qwin_execute(QNtKernel *kernel,EFI_BOOT_SERVICES *services,const uint8_t *exe,size_t bytes,QWinSerial writer){
    if(!kernel||!services||!services->AllocatePages||!exe||!bytes||!writer)return QNT_INVALID;
    uint32_t pid=0,tid=0;
    if(qnt_process_create(kernel,&pid)!=QNT_OK)return QNT_FULL;
    uint64_t address=0;
    uint64_t pages=256;
    if(services->AllocatePages(0,1,pages,&address)!=0||!address){
        qnt_process_destroy(kernel,pid);
        return QNT_FULL;
    }
    QPe64Resolver resolver={0,resolve_symbol};
    QPe64Image image={0};
    running_kernel=kernel;running_serial=writer;running_pid=pid;module_base=(uint8_t *)(uintptr_t)address;
    int result=q_pe64_load(exe,bytes,(void *)(uintptr_t)address,(size_t)(pages*4096),&resolver,&image);
    if(result!=Q_PE_OK){qnt_process_destroy(kernel,pid);return result;}
    if(qnt_vm_map(kernel,pid,address,pages*4096,QNT_VM_READ|QNT_VM_EXECUTE|QNT_VM_USER)!=QNT_OK){
        qnt_process_destroy(kernel,pid);return QNT_INVALID;
    }
    void *entry=q_pe64_entry(&image);
    uint64_t stack=(uint64_t)(uintptr_t)&tid;
    if(!entry||qnt_thread_create(kernel,pid,(uint64_t)(uintptr_t)entry,stack,&tid)!=QNT_OK){
        qnt_process_destroy(kernel,pid);return QNT_INVALID;
    }
    qnt_schedule(kernel);
    void (EFIAPI *start)(void)=(void (EFIAPI *)(void))entry;
    start();
    qnt_process_destroy(kernel,pid);
    running_kernel=0;running_serial=0;running_pid=0;module_base=0;
    return QNT_OK;
}
