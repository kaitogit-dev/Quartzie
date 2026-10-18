#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb=0x2000000;
uint16_t boot_pitch=7680;
uint16_t boot_width=1920;
uint16_t boot_height=1080;
uint8_t boot_bpp=32;
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL stability25 line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
#define PIXELS (1920u*1080u)
static void map(uintptr_t a){CHECK(mmap((void*)a,PIXELS*4u,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=MAP_FAILED);}
static void draw(void){repaint();CHECK(memcmp(canvas,(const void*)(uintptr_t)boot_fb,PIXELS*4u)==0);}
static void render_preview(const char *name){FILE *f=fopen(name,"wb");CHECK(f);fprintf(f,"P6\n1920 1080\n255\n");for(size_t i=0;i<PIXELS;i++){uint32_t c=canvas[i];fputc((c>>16)&255,f);fputc((c>>8)&255,f);fputc(c&255,f);}CHECK(!fclose(f));}
static uint32_t expected(uint32_t a,uint32_t b,int w){int inv=256-w;return (((((a>>16)&255)*inv+((b>>16)&255)*w)>>8)<<16)|(((((a>>8)&255)*inv+((b>>8)&255)*w)>>8)<<8)|(((a&255)*inv+(b&255)*w)>>8);}
int main(void){
    map(0x400000);map(0xD00000);map(0x2000000);
    sw=1920;sh=1080;mouse_x=1600;mouse_y=900;
    for(int i=0;i<APPS;i++){windows[i].x=435+14*(i%4);windows[i].y=175+15*(i%4);windows[i].w=1050;windows[i].h=700;}
    for(uint32_t i=0;i<16000;i++){
        uint32_t a=(i*2654435761u)&0xffffffu;
        uint32_t b=(i*2246822519u)&0xffffffu;
        int w=(int)(i%257u);
        CHECK(blend(a,b,w)==expected(a,b,w));
    }
    draw();CHECK(full_frames==1);
    open_window(FILES);
    for(int n=0;n<WINDOW_OPEN_STEPS+2;n++)draw();
    CHECK(windows[FILES].opened);
    render_preview("build/stability25-explorer.ppm");
    for(int n=0;n<5;n++){
        toggle_maximize(FILES);
        for(int j=0;j<WINDOW_RESIZE_STEPS+2;j++)draw();
        CHECK(windows[FILES].maximized);
        CHECK(windows[FILES].w==1912);
        toggle_maximize(FILES);
        for(int j=0;j<WINDOW_RESIZE_STEPS+2;j++)draw();
        CHECK(!windows[FILES].maximized);
    }
    toggle_maximize(FILES);
    for(int j=0;j<WINDOW_RESIZE_STEPS+2;j++)draw();
    render_preview("build/stability25-maximized.ppm");
    notification_open=1;dirty=1;
    for(int j=0;j<PANEL_STEPS+2;j++)draw();
    CHECK(panel_step[PANEL_NOTIFICATIONS]==PANEL_STEPS);
    render_preview("build/stability25-notifications.ppm");
    notification_open=0;dirty=1;
    for(int j=0;j<PANEL_STEPS+2;j++)draw();
    CHECK(panel_step[PANEL_NOTIFICATIONS]==0);
    close_window(FILES,1);
    for(int j=0;j<WINDOW_CLOSE_STEPS+2;j++)draw();
    CHECK(windows[FILES].minimized);
    open_window(FILES);
    for(int j=0;j<WINDOW_OPEN_STEPS+2;j++)draw();
    CHECK(!windows[FILES].minimized);
    uint32_t full=full_frames;
    pointer_only=1;mouse_x+=27;move_pointer_only();
    CHECK(full_frames==full && pointer_frames>0);
    timer_accum=0;timer_counter=60000;
    CHECK(advance_frame_counter(37000)==2);
    notification_open=1;dirty=1;animation_step_allowed=2;
    draw();CHECK(panel_step[PANEL_NOTIFICATIONS]==2);
    animation_step_allowed=1;
    for(int j=0;j<PANEL_STEPS+2;j++)draw();
    CHECK(full_frames==full);
    printf("PASS: 1080p repeated Explorer maximize/restore, optimized scanline present, exact blend arithmetic, slow notification tween, multi-tick catchup and cursor-only redraw\n");
    return 0;
}
