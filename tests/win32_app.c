#include <stdint.h>
__declspec(dllimport) uint32_t __stdcall GetTickCount(void);
__declspec(dllimport) void * __stdcall GetStdHandle(uint32_t handle);
__declspec(dllimport) int __stdcall WriteFile(void *handle,const void *bytes,uint32_t length,uint32_t *written,void *overlapped);
static volatile uint32_t number=37;
static volatile uint32_t *volatile address=&number;
__declspec(dllexport) uint32_t __stdcall QuartzieTest(void){
    return *address+GetTickCount();
}
void mainCRTStartup(void){
    static const char message[]="Quartzie Win32 PE64 app\r\n";
    uint32_t written=0;
    WriteFile(GetStdHandle(0xfffffff5u),message,(uint32_t)(sizeof(message)-1),&written,0);
}
