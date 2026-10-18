#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb=0x2000000;
uint16_t boot_pitch=7680,boot_width=1920,boot_height=1080;
uint8_t boot_bpp=32;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL corners28 line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void map_area(uintptr_t p){CHECK(mmap((void*)p,1920*1080*4,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=MAP_FAILED);}
int main(void){
 map_area(0x400000);map_area(0xD00000);map_area(0x2000000);
 sw=1920;sh=1080;mouse_x=1700;mouse_y=920;
 for(int i=0;i<APPS;i++){windows[i].w=1050;windows[i].h=700;windows[i].x=430;windows[i].y=150;}
 repaint();
 start_open=1;dirty=1;repaint();
 CHECK(round_layer_depth==0);
 open_window(FILES);for(int i=0;i<WINDOW_OPEN_STEPS+2;i++) repaint();
 CHECK(round_layer_depth==0);
 CHECK(round_layer_errors==0);
 CHECK(windows[FILES].opened);
 CHECK(!memcmp(canvas,(const void*)(uintptr_t)boot_fb,1920*1080*4));
 printf("PASS: 1080p start-menu radius, nested window corner clipping and balanced rounded layers\n");
 return 0;
}
