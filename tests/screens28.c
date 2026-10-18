#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb=0x2000000;
uint16_t boot_pitch=7680,boot_width=1920,boot_height=1080;
uint8_t boot_bpp=32;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL screens28 line %d\n",__LINE__);exit(1);}}while(0)
static void map_area(uintptr_t p){CHECK(mmap((void*)p,1920*1080*4,3,0x32,-1,0)!=MAP_FAILED);}
static void screenshot(const char *name){FILE*f=fopen(name,"wb");CHECK(f);fprintf(f,"P6\n%d %d\n255\n",sw,sh);for(size_t i=0;i<(size_t)sw*sh;i++){uint32_t c=canvas[i];fputc(c>>16&255,f);fputc(c>>8&255,f);fputc(c&255,f);}CHECK(!fclose(f));}
int main(void){map_area(0x400000);map_area(0xD00000);map_area(0x2000000);sw=1920;sh=1080;mouse_x=1510;mouse_y=768;copy(clock_date,"2026-10-18",11);copy(clock_time,"19:38:00",9);for(int i=0;i<APPS;i++){windows[i].w=1050;windows[i].h=700;windows[i].x=435+14*(i%4);windows[i].y=170+15*(i%4);}repaint();screenshot("build/quartzie28-desktop.ppm");start_open=1;for(int i=0;i<PANEL_STEPS+1;i++)repaint();screenshot("build/quartzie28-start.ppm");start_open=0;for(int i=0;i<PANEL_STEPS+1;i++)repaint();open_window(FILES);for(int i=0;i<WINDOW_OPEN_STEPS+2;i++)repaint();screenshot("build/quartzie28-explorer.ppm");CHECK(round_layer_errors==0&&round_layer_depth==0);puts("PASS: rendered Quartzie 0.28 Fluid Layers desktop, Start and Explorer");}
