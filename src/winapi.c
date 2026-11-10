#include "winapi.h"
#include "pe64.h"
static QNtKernel *running_kernel;
static EFI_BOOT_SERVICES *running_services;
static QWinSerial running_serial;
static uint32_t running_pid;
static uint8_t *module_base;
static uint32_t last_error;
static char command_line[]="Quartzie.exe";
static uint16_t command_line_w[]={ 'Q','u','a','r','t','z','i','e','.','e','x','e',0 };
typedef struct {void *address;uint64_t bytes;} QHeapAllocation;
static QHeapAllocation allocations[128];
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
static uint64_t EFIAPI win_ticks64(void){
    return running_kernel?running_kernel->clock_ticks:0;
}
static void * EFIAPI win_stdout(uint32_t standard){
    if(standard==0xfffffff5u)return (void *)(uintptr_t)1;
    if(standard==0xfffffff4u)return (void *)(uintptr_t)2;
    last_error=6;
    return (void *)(uintptr_t)-1;
}
static int EFIAPI win_write(void *handle,const void *buffer,uint32_t length,uint32_t *written,void *overlapped){
    if(written)*written=0;
    if(overlapped||!(handle==(void *)(uintptr_t)1||handle==(void *)(uintptr_t)2)||(!buffer&&length>0)||!running_serial){
        last_error=87;return 0;
    }
    if(length>1048576u){last_error=87;return 0;}
    for(uint32_t i=0;i<length;i++)running_serial(((const uint8_t *)buffer)[i]);
    if(written)*written=length;
    return 1;
}
static uint32_t EFIAPI win_last_error(void){return last_error;}
static void EFIAPI win_set_last_error(uint32_t error){last_error=error;}
static uint32_t EFIAPI win_pid(void){return running_pid;}
static uint32_t EFIAPI win_tid(void){return running_kernel?running_kernel->current_tid:0;}
static char * EFIAPI win_command_line(void){return command_line;}
static uint16_t * EFIAPI win_command_line_w(void){return command_line_w;}
static void * EFIAPI win_module(void *name){return name?0:module_base;}
static void * EFIAPI win_module_w(void *name){return name?0:module_base;}
static void * EFIAPI win_current_process(void){return (void *)(uintptr_t)-1;}
static void * EFIAPI win_current_thread(void){return (void *)(uintptr_t)-2;}
static uint32_t EFIAPI win_version(void){return 0x0000000au;}
static int EFIAPI win_debugger(void){return 0;}
static void EFIAPI win_sleep(uint32_t milliseconds){
    if(!running_services||!running_services->Stall)return;
    while(milliseconds){
        uint32_t slice=milliseconds>1000?1000:milliseconds;
        running_services->Stall((uint64_t)slice*1000u);
        milliseconds-=slice;
    }
}
static void * EFIAPI win_process_heap(void){return (void *)(uintptr_t)1;}
static int heap_index(const void *pointer){
    for(int i=0;i<128;i++)if(allocations[i].address==pointer&&pointer)return i;
    return -1;
}
static void * EFIAPI win_heap_alloc(void *heap,uint32_t flags,uint64_t bytes){
    if(heap!=(void *)(uintptr_t)1||!running_services||!running_services->AllocatePool||bytes>64u*1024u*1024u){
        last_error=87;return 0;
    }
    int slot=-1;
    for(int i=0;i<128;i++)if(!allocations[i].address){slot=i;break;}
    if(slot<0){last_error=8;return 0;}
    void *memory=0;
    if(running_services->AllocatePool(2,bytes?bytes:1,&memory)!=0||!memory){
        last_error=8;return 0;
    }
    allocations[slot].address=memory;
    allocations[slot].bytes=bytes?bytes:1;
    if(flags&8u){
        uint8_t *p=(uint8_t *)memory;
        for(uint64_t j=0;j<allocations[slot].bytes;j++)p[j]=0;
    }
    return memory;
}
static int EFIAPI win_heap_free(void *heap,uint32_t flags,void *memory){
    (void)flags;
    if(heap!=(void *)(uintptr_t)1){last_error=6;return 0;}
    if(!memory)return 1;
    int slot=heap_index(memory);
    if(slot<0||!running_services||!running_services->FreePool||running_services->FreePool(memory)!=0){
        last_error=6;return 0;
    }
    allocations[slot].address=0;allocations[slot].bytes=0;
    return 1;
}
static void * EFIAPI win_heap_realloc(void *heap,uint32_t flags,void *memory,uint64_t bytes){
    if(heap!=(void *)(uintptr_t)1||!memory||!bytes||(flags&16u)){
        last_error=87;return 0;
    }
    int slot=heap_index(memory);
    if(slot<0){last_error=6;return 0;}
    uint64_t previous=allocations[slot].bytes;
    void *result=win_heap_alloc(heap,flags,bytes);
    if(!result)return 0;
    uint64_t copy=previous<bytes?previous:bytes;
    for(uint64_t i=0;i<copy;i++)((uint8_t *)result)[i]=((uint8_t *)memory)[i];
    if(!win_heap_free(heap,0,memory))return 0;
    return result;
}
static uint64_t EFIAPI win_heap_size(void *heap,uint32_t flags,const void *memory){
    (void)flags;
    int slot=heap_index(memory);
    if(heap!=(void *)(uintptr_t)1||slot<0){last_error=6;return (uint64_t)-1;}
    return allocations[slot].bytes;
}
static void *resolve_symbol(void *context,const char *dll,const char *symbol,uint16_t ordinal){
    (void)context;
    if(ordinal||!symbol||!(ascii_equal(dll,"kernel32.dll")||ascii_equal(dll,"kernelbase.dll")))return 0;
    if(ascii_equal(symbol,"GetTickCount"))return (void *)(uintptr_t)&win_ticks;
    if(ascii_equal(symbol,"GetTickCount64"))return (void *)(uintptr_t)&win_ticks64;
    if(ascii_equal(symbol,"GetStdHandle"))return (void *)(uintptr_t)&win_stdout;
    if(ascii_equal(symbol,"WriteFile"))return (void *)(uintptr_t)&win_write;
    if(ascii_equal(symbol,"GetLastError"))return (void *)(uintptr_t)&win_last_error;
    if(ascii_equal(symbol,"SetLastError"))return (void *)(uintptr_t)&win_set_last_error;
    if(ascii_equal(symbol,"GetCurrentProcessId"))return (void *)(uintptr_t)&win_pid;
    if(ascii_equal(symbol,"GetCurrentThreadId"))return (void *)(uintptr_t)&win_tid;
    if(ascii_equal(symbol,"GetCurrentProcess"))return (void *)(uintptr_t)&win_current_process;
    if(ascii_equal(symbol,"GetCurrentThread"))return (void *)(uintptr_t)&win_current_thread;
    if(ascii_equal(symbol,"GetCommandLineA"))return (void *)(uintptr_t)&win_command_line;
    if(ascii_equal(symbol,"GetCommandLineW"))return (void *)(uintptr_t)&win_command_line_w;
    if(ascii_equal(symbol,"GetModuleHandleA"))return (void *)(uintptr_t)&win_module;
    if(ascii_equal(symbol,"GetModuleHandleW"))return (void *)(uintptr_t)&win_module_w;
    if(ascii_equal(symbol,"GetVersion"))return (void *)(uintptr_t)&win_version;
    if(ascii_equal(symbol,"IsDebuggerPresent"))return (void *)(uintptr_t)&win_debugger;
    if(ascii_equal(symbol,"Sleep"))return (void *)(uintptr_t)&win_sleep;
    if(ascii_equal(symbol,"GetProcessHeap"))return (void *)(uintptr_t)&win_process_heap;
    if(ascii_equal(symbol,"HeapAlloc"))return (void *)(uintptr_t)&win_heap_alloc;
    if(ascii_equal(symbol,"HeapFree"))return (void *)(uintptr_t)&win_heap_free;
    if(ascii_equal(symbol,"HeapReAlloc"))return (void *)(uintptr_t)&win_heap_realloc;
    if(ascii_equal(symbol,"HeapSize"))return (void *)(uintptr_t)&win_heap_size;
    return 0;
}
static void clear_running(void){
    if(running_services&&running_services->FreePool)
        for(int i=0;i<128;i++)if(allocations[i].address)
            running_services->FreePool(allocations[i].address);
    for(int i=0;i<128;i++){allocations[i].address=0;allocations[i].bytes=0;}
    running_kernel=0;running_services=0;running_serial=0;running_pid=0;module_base=0;last_error=0;
}
int qwin_execute(QNtKernel *kernel,EFI_BOOT_SERVICES *services,const uint8_t *exe,size_t bytes,QWinSerial writer){
    if(!kernel||!services||!services->AllocatePages||!services->FreePages||!exe||!bytes||!writer)return QNT_INVALID;
    if(running_kernel)return QNT_BUSY;
    size_t required=q_pe64_image_size(exe,bytes);
    if(!required)return Q_PE_INVALID;
    uint64_t pages=((uint64_t)required+4095u)/4096u;
    uint32_t pid=0,tid=0;
    int status=qnt_process_create(kernel,&pid);
    if(status!=QNT_OK)return status;
    uint64_t address=0;
    if(services->AllocatePages(0,2,pages,&address)!=0||!address){
        qnt_process_destroy(kernel,pid);
        return QNT_FULL;
    }
    QPe64Resolver resolver={0,resolve_symbol};
    QPe64Image image={0};
    running_kernel=kernel;running_services=services;running_serial=writer;
    running_pid=pid;module_base=(uint8_t *)(uintptr_t)address;
    status=q_pe64_load(exe,bytes,(void *)(uintptr_t)address,(size_t)(pages*4096),&resolver,&image);
    if(status==Q_PE_OK){
        status=qnt_vm_map(kernel,pid,address,pages*4096,QNT_VM_READ|QNT_VM_EXECUTE|QNT_VM_USER);
        if(status==QNT_OK){
            void *entry=q_pe64_entry(&image);
            uint64_t stack=(uint64_t)(uintptr_t)&tid;
            if(!entry||qnt_thread_create(kernel,pid,(uint64_t)(uintptr_t)entry,stack,&tid)!=QNT_OK)status=QNT_INVALID;
            else{
                qnt_schedule(kernel);
                void (EFIAPI *start)(void)=(void (EFIAPI *)(void))entry;
                start();
            }
        }
    }
    qnt_process_destroy(kernel,pid);
    clear_running();
    services->FreePages(address,pages);
    return status;
}
