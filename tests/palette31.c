#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb=0x2000000;
uint16_t boot_pitch=7680,boot_width=1920,boot_height=1080;
uint8_t boot_bpp=32;
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL palette31 line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static void map_buffer(uintptr_t x) {
    CHECK(mmap((void*)x,1920u*1080u*4u,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=MAP_FAILED);
}
int main(void) {
    map_buffer(0x400000);map_buffer(0xd00000);map_buffer(0x2000000);
    sw=1920;sh=1080;mouse_x=900;mouse_y=650;
    for(int i=0;i<APPS;i++)CHECK(!windows[i].opened&&!windows[i].minimized);
    theme=0;CHECK(WHITE==0xffffffu&&PAPER==0xf9fcffu&&ACTIVE==0x7ec5fcu&&BLUE==0x2167b3u);
    theme=3;CHECK(WHITE==0xf4faffu&&PAPER==0x182637u&&ACTIVE==0xa4dcffu);
    theme=1000;accent_index=1000;
    repaint();
    CHECK(theme==0&&accent_index==0);
    CHECK(WHITE==0xffffffu&&PAPER==0xf9fcffu);
    int x=300,y=200;
    draw_icon(x,y,CALENDAR);
    CHECK(canvas[(size_t)(y+6)*sw+x+8]==0xffffffu);
    draw_icon(x+70,y,NOTES);
    CHECK(canvas[(size_t)(y+6)*sw+x+80]==0xf9fcffu);
    draw_pointer();
    CHECK(pointer_saved);
    int found=0;
    for(int yy=0;yy<CURSOR_H;yy++)for(int xx=0;xx<CURSOR_W;xx++)
      if(cursor_alpha[yy][xx]==255&&canvas[(size_t)(mouse_y+yy)*sw+mouse_x+xx]!=0)found=1;
    CHECK(found);
    puts("PASS: boot palette uses stable literal colors, invalid themes rejected, white cursor and nonblack desktop icons");
    return 0;
}
