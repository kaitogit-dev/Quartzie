#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb = 0x2000000;
uint16_t boot_pitch = 7680;
uint16_t boot_width = 1920;
uint16_t boot_height = 1080;
uint8_t boot_bpp = 32;
static double ms(struct timespec s, struct timespec e) {return (e.tv_sec-s.tv_sec)*1000.0+(e.tv_nsec-s.tv_nsec)/1000000.;}
static double tick(void) {struct timespec s,e;clock_gettime(CLOCK_MONOTONIC,&s); repaint();clock_gettime(CLOCK_MONOTONIC,&e);return ms(s,e);}
int main(void) {
  for(int k=0;k<3;k++) { uintptr_t a=k==0?0x400000:k==1?0xD00000:0x2000000;
    if(mmap((void*)a,1920u*1080*4,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)==MAP_FAILED)abort();
  }
  sw=1920;sh=1080;mouse_x=1480;mouse_y=900;
  for(int i=0;i<APPS;i++){windows[i].w=1050;windows[i].h=700;windows[i].x=435;windows[i].y=170;}
  printf("desktop %.2f ms\n",tick());
  open_window(FILES);
  for(int i=0;i<WINDOW_OPEN_STEPS+2;i++)printf("open %d %.2f ms\n",i,tick());
  printf("idle %.2f ms\n",tick());
  toggle_maximize(FILES);
  for(int i=0;i<WINDOW_RESIZE_STEPS+2;i++)printf("max %d %.2f ms\n",i,tick());
  notification_open=1;dirty=1;
  for(int i=0;i<PANEL_STEPS+2;i++)printf("notify %d %.2f ms\n",i,tick());
  return 0;
}
