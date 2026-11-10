#include <stdint.h>
__declspec(dllimport) uint32_t __stdcall GetTickCount(void);
__declspec(dllimport) void * __stdcall GetStdHandle(uint32_t handle);
__declspec(dllimport) int __stdcall WriteFile(void *handle,const void *bytes,uint32_t length,uint32_t *written,void *overlapped);

__declspec(dllimport) uint64_t __stdcall GetTickCount64(void);
__declspec(dllimport) uint32_t __stdcall GetCurrentProcessId(void);
__declspec(dllimport) void * __stdcall GetProcessHeap(void);
__declspec(dllimport) void * __stdcall HeapAlloc(void *heap,uint32_t flags,uint64_t bytes);
__declspec(dllimport) int __stdcall HeapFree(void *heap,uint32_t flags,void *address);
__declspec(dllimport) void __stdcall SetLastError(uint32_t value);
__declspec(dllimport) uint32_t __stdcall GetLastError(void);
static volatile uint32_t number=37;
static volatile uint32_t *volatile address=&number;
__declspec(dllexport) uint32_t __stdcall QuartzieTest(void){
    return *address+GetTickCount();
}
void mainCRTStartup(void){
    void *heap=GetProcessHeap();
    uint8_t *memory=(uint8_t *)HeapAlloc(heap,8,32);
    if(!memory)return;
    for(int i=0;i<32;i++)if(memory[i])return;
    memory[0]='Q';
    if(GetCurrentProcessId()==0||GetTickCount64()<GetTickCount()||memory[0]!='Q')return;
    SetLastError(41);
    if(GetLastError()!=41)return;
    if(!HeapFree(heap,0,memory))return;
    static const char message[]="Quartzie Win32 PE64 app\r\n";
    uint32_t written=0;
    WriteFile(GetStdHandle(0xfffffff5u),message,(uint32_t)(sizeof(message)-1),&written,0);
}
