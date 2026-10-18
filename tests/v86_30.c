#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb=0x2000000;
uint16_t boot_pitch=7680,boot_width=1920,boot_height=1080;
uint8_t boot_bpp=32;
#define PIXELS (1920u*1080u)
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL v86_30 line %d: %s\n",__LINE__,#x);exit(1); } } while(0)
static void mapped(uintptr_t at) {
    CHECK(mmap((void*)at,PIXELS*4,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=MAP_FAILED);
}
static void check_frame(void) {
    repaint();
    CHECK(!memcmp(canvas,(void*)(uintptr_t)boot_fb,PIXELS*4));
    CHECK(round_layer_depth==0 && round_layer_errors==0);
}
int main(void) {
    mapped(0x400000);mapped(0xd00000);mapped(0x2000000);
    sw=1920;sh=1080;mouse_x=1700;mouse_y=870;
    for(int app=0;app<APPS;app++) {
        windows[app].x=410+app*9;windows[app].y=160+app*7;
        windows[app].w=1030;windows[app].h=690;
    }
    check_frame();
    CHECK(desktop_cache[500u*sw+700]!=desktop_cache[500u*sw+900]);
    uint32_t before=full_frames;
    for(int n=0;n<36;n++) {
        int app=n%APPS;
        open_window(app);
        CHECK(windows[app].opened&&!windows[app].minimized);
        CHECK(anim_ticks[app]==0);
        check_frame();
        CHECK(full_frames>before);
        before=full_frames;
        int x=windows[app].x+110,y=windows[app].y+74;
        CHECK(canvas[(size_t)y*sw+x]!=desktop_cache[(size_t)y*sw+x]);
        close_window(app,0);
        check_frame();
    }
    puts("PASS: native-resolution wallpaper, immediate full app contents, single structural redraw and unchanged framebuffer in 36 opens");
    return 0;
}
