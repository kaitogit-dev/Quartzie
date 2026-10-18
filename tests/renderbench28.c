#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb=0x2000000;
uint16_t boot_pitch=7680,boot_width=1920,boot_height=1080;
uint8_t boot_bpp=32;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL renderbench28 line %d\n",__LINE__);exit(1);}}while(0)
static void map_area(uintptr_t p){CHECK(mmap((void*)p,1920*1080*4,3,0x32,-1,0)!=MAP_FAILED);}
static double clock_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (double)t.tv_sec*1000.0+(double)t.tv_nsec/1000000.;}
int main(void){
 map_area(0x400000);map_area(0xD00000);map_area(0x2000000);
 sw=1920;sh=1080;mouse_x=1850;mouse_y=900;
 for(int i=0;i<APPS;i++){windows[i].w=1050;windows[i].h=700;windows[i].x=430+14*(i%4);windows[i].y=170+15*(i%4);}
 double t=clock_ms();repaint();double first=clock_ms()-t;
 double open=0,maximize=0,menus=0;
 for(int i=0;i<8;i++){
   if(windows[FILES].opened){close_window(FILES,0);for(int j=0;j<WINDOW_CLOSE_STEPS+1;j++) repaint();}
   t=clock_ms();open_window(FILES);for(int j=0;j<WINDOW_OPEN_STEPS+1;j++)repaint();open+=clock_ms()-t;
   t=clock_ms();toggle_maximize(FILES);for(int j=0;j<WINDOW_RESIZE_STEPS+1;j++)repaint();maximize+=clock_ms()-t;
   toggle_maximize(FILES);for(int j=0;j<WINDOW_RESIZE_STEPS+1;j++)repaint();
   t=clock_ms();start_open=1;dirty=1;for(int j=0;j<3;j++) repaint();start_open=0;dirty=1;for(int j=0;j<3;j++)repaint();menus+=clock_ms()-t;
 }
 printf("startup_ms %.3f\nexplorer_open_ms %.3f\nmaximize_ms %.3f\nstart_toggle_ms %.3f\nround_layer_errors %u\n",first,open/8,maximize/8,menus/8,round_layer_errors);
 CHECK(round_layer_errors==0);
 return 0;
}
