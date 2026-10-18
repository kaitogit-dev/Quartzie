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
#define PIXELS (1920u*1080u)
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL appopen28 line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static void map_area(uintptr_t p){ CHECK(mmap((void*)p,PIXELS*4u,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=MAP_FAILED); }
static void frame(void){repaint();CHECK(round_layer_depth==0);CHECK(round_layer_errors==0);}
int main(void){
    map_area(0x400000);map_area(0xD00000);map_area(0x2000000);
    sw=1920;sh=1080;mouse_x=1750;mouse_y=955;
    for(int i=0;i<APPS;i++){windows[i].x=440+14*(i%4);windows[i].y=170+15*(i%4);windows[i].w=1040;windows[i].h=690;}
    frame();
    int outer_x=1650,outer_y=120;
    uint32_t before=canvas[(size_t)outer_y*sw+outer_x];
    round_capture(outer_x,outer_y,200,130,20);
    rect(outer_x,outer_y,200,130,0xaabbee);
    round_capture(outer_x+19,outer_y+19,85,48,10);
    rect(outer_x+19,outer_y+19,85,48,0x152334);
    round_finish(outer_x+19,outer_y+19,85,48,10);
    round_finish(outer_x,outer_y,200,130,20);
    CHECK(canvas[(size_t)outer_y*sw+outer_x]==before);
    CHECK(round_layer_depth==0 && round_layer_errors==0);
    for(int n=0;n<30;n++){
        int app=n%APPS;
        open_window(app);frame();frame();
        CHECK(windows[app].opened && !windows[app].minimized);
        toggle_maximize(app);frame();
        CHECK(windows[app].maximized);
        toggle_maximize(app);frame();
        CHECK(!windows[app].maximized);
        close_window(app,1);frame();
        CHECK(windows[app].minimized);
        open_window(app);frame();
        CHECK(!windows[app].minimized);
        close_window(app,0);frame();
        CHECK(!windows[app].opened);
        if((n%5)==0){
            start_open=1;dirty=1;frame();
            start_open=0;dirty=1;frame();
            quick_open=1;dirty=1;frame();
            quick_open=0;dirty=1;frame();
            notification_open=1;dirty=1;frame();
            notification_open=0;dirty=1;frame();
        }
        CHECK(next_order<49);
        if((n%3)==0)CHECK(!memcmp(canvas,(void*)(uintptr_t)boot_fb,PIXELS*4u));
    }
    CHECK(round_layer_depth==0 && round_layer_errors==0);
    puts("PASS: 30 cycles across all apps, maximize/restore/minimize/reopen, Start/Quick/notifications, exact nested glass corners, frame consistency");
}
