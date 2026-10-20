#include "hostcursor.h"
#include <stdint.h>
static void choose(uint16_t selector){
    __asm__ volatile("outw %0,%1"::"a"(selector),"Nd"((uint16_t)0x510));
}
static uint8_t get(void){
    uint8_t value;
    __asm__ volatile("inb %1,%0":"=a"(value):"Nd"((uint16_t)0x511));
    return value;
}
static uint32_t number(void){
    uint32_t n=0;
    for(int i=0;i<4;i++)n=(n<<8)|get();
    return n;
}
static int matches(const uint8_t *name){
    const char *wanted="opt/quartzie/hostcursor";
    for(int i=0;i<56;i++){
        if(!wanted[i])return name[i]==0;
        if(name[i]!=(uint8_t)wanted[i])return 0;
    }
    return 0;
}
int q_host_cursor_enabled(void){
    choose(0);
    if(get()!='Q'||get()!='E'||get()!='M'||get()!='U')return 0;
    choose(0x19);
    uint32_t count=number();
    if(count>256)return 0;
    for(uint32_t i=0;i<count;i++){
        uint8_t file[64];
        for(int j=0;j<64;j++)file[j]=get();
        if(!matches(file+8))continue;
        uint32_t size=((uint32_t)file[0]<<24)|((uint32_t)file[1]<<16)|((uint32_t)file[2]<<8)|file[3];
        uint16_t key=((uint16_t)file[4]<<8)|file[5];
        if(size!=1)return 0;
        choose(key);
        return get()=='1';
    }
    return 0;
}
