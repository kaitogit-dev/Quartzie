#include "ps2.h"
volatile uint32_t q_ps2_bytes_seen;
static uint8_t in8(uint16_t port){uint8_t value;__asm__ volatile("inb %1,%0":"=a"(value):"Nd"(port));return value;}
static void out8(uint16_t port,uint8_t value){__asm__ volatile("outb %0,%1"::"a"(value),"Nd"(port));}
static int ready(void){for(int n=0;n<100000;n++)if(!(in8(0x64)&2))return 1;return 0;}
static int command(uint8_t byte){if(!ready())return 0;out8(0x64,byte);return 1;}
static int data(uint8_t byte){if(!ready())return 0;out8(0x60,byte);return 1;}
static int read_aux(void){
    for(int n=0;n<250000;n++){
        uint8_t flags=in8(0x64);
        if((flags&0x21)==0x21)return in8(0x60);
    }
    return -1;
}
static int mouse_command(uint8_t value){
    if(!command(0xd4)||!data(value))return 0;
    return read_aux()==0xfa;
}
int q_ps2_enable(void){
    if(!command(0xa8))return 0;
    if(!command(0x20))return 0;
    int cfg=-1;
    for(int n=0;n<100000;n++)if(in8(0x64)&1){cfg=in8(0x60);break;}
    if(cfg<0)return 0;
    if(!command(0x60)||!data((uint8_t)(cfg&~0x22u)))return 0;
    if(!mouse_command(0xf6))return 0;
    if(!mouse_command(0xf3)||!mouse_command(200))return 0;
    return mouse_command(0xf4);
}
int q_ps2_feed(QPs2Mouse *mouse,uint8_t value,int *dx,int *dy,int *buttons){
    if(mouse->index==0){
        if((value&0xc8u)!=0x08u)return 0;
    }
    mouse->bytes[mouse->index++]=value;
    if(mouse->index<3)return 0;
    mouse->index=0;
    uint8_t flags=mouse->bytes[0];
    if((flags&0xc8u)!=0x08u)return 0;
    *dx=(int8_t)mouse->bytes[1];
    *dy=-(int8_t)mouse->bytes[2];
    *buttons=flags&3;
    return 1;
}
int q_ps2_poll(QPs2Mouse *mouse,int *dx,int *dy,int *buttons){
    if(!command(0xd4)||!data(0xeb))return 0;
    if(read_aux()!=0xfa)return 0;
    int status=read_aux(),mx=read_aux(),my=read_aux();
    if(status<0||mx<0||my<0)return 0;
    q_ps2_bytes_seen+=3;
    mouse->index=0;
    int a=q_ps2_feed(mouse,(uint8_t)status,dx,dy,buttons);
    int b=q_ps2_feed(mouse,(uint8_t)mx,dx,dy,buttons);
    int c=q_ps2_feed(mouse,(uint8_t)my,dx,dy,buttons);
    return a||b||c;
}
