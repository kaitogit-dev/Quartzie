#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb = 0x2000000;
uint16_t boot_pitch = 5120;
uint16_t boot_width = 1280;
uint16_t boot_height = 720;
uint8_t boot_bpp = 32;
#define check(ok) do { if (!(ok)) { fprintf(stderr, "FAIL ui12 line %d: %s\n", __LINE__, #ok); exit(1); } } while (0)
static void save(const char *name) {
    FILE *f=fopen(name,"wb"); check(f);
    fprintf(f,"P6\n%d %d\n255\n",sw,sh);
    for(int y=0;y<sh;y++) for(int x=0;x<sw;x++) {
        uint32_t c=canvas[(size_t)y*sw+x];
        fputc((c>>16)&255,f); fputc((c>>8)&255,f); fputc(c&255,f);
    }
    fclose(f);
}
static void frames(int count) { for(int i=0;i<count;i++) repaint(); }
static uint32_t at(int x,int y) { return canvas[(size_t)y*sw+x]; }
int main(void) {
    check(mmap((void*)0x400000,1280*720*4,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=MAP_FAILED);
    check(mmap((void*)0xD00000,1280*720*4,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=MAP_FAILED);
    check(mmap((void*)0x2000000,1280*720*4,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=MAP_FAILED);
    sw=1280;sh=720;mouse_x=1111;mouse_y=600;
    copy(clock_time,"16:24:10",9);
    copy(clock_date,"2026-10-14",11);
    for(int i=0;i<APPS;i++) { windows[i].x=260+14*(i%4);windows[i].y=87+15*(i%4);windows[i].w=760;windows[i].h=485; }
    frames(2);
    int x,y; desktop_position(0,&x,&y);
    check(at(x+19,y+3)==desktop_cache[(size_t)(y+3)*sw+x+19]);
    int bar=taskbar_left(7);
    int bar_y=sh-44;
    uint32_t bar_background=at(bar+2,bar_y);
    check(bar_background!=desktop_cache[(size_t)bar_y*sw+bar+2]);
    check(bar_background!=0);
    save("build/desktop-12.ppm");
    open_window(FILES);open_window(NOTES);open_window(CONSOLE);frames(9);
    check(active_window()==CONSOLE);
    handle_key(0x38);
    handle_key(0x0f);
    check(tab_active && tab_count==3 && tab_apps[0]==CONSOLE && tab_apps[1]==NOTES && tab_selected==1);
    frames(2);
    save("build/switcher-12.ppm");
    handle_key(0x0f);
    check(tab_selected==2 && tab_apps[tab_selected]==FILES);
    handle_key(0xb8);
    check(!tab_active && active_window()==FILES);
    frames(8);
    handle_key(0x38);
    handle_key(0x2a);
    handle_key(0x0f);
    check(tab_active && tab_selected==tab_count-1);
    handle_key(0xaa);
    handle_key(0xb8);
    check(!tab_active && active_window()==NOTES);
    handle_key(0x38);handle_key(0x0f);
    check(tab_active);
    handle_key(0x01);
    check(!tab_active && active_window()==NOTES);
    handle_key(0xb8);
    for(int i=0;i<APPS;i++) {windows[i].opened=0;windows[i].minimized=0;anim_ticks[i]=0;anim_visible[i]=0;}
    dirty=1; frames(2);
    handle_key(0x1d);handle_key(0x38);handle_key(0x20);
    check(desktop_focus==0);
    handle_key(0xb8);handle_key(0x9d);
    frames(1);
    save("build/desktop-focus-12.ppm");
    handle_key(0x50);check(desktop_focus==1);
    handle_key(0x4d);check(desktop_focus==5);
    frames(1);
    handle_key(0x1c);
    check(desktop_focus==-1 && windows[APPEAR].opened);
    frames(8);
    int ax,ay,aw,ah; animated_window_rect(APPEAR,&ax,&ay,&aw,&ah);
    mouse_x=ax+aw-18;mouse_y=ay+15;
    handle_mouse_byte(9);handle_mouse_byte(0);handle_mouse_byte(0);
    handle_mouse_byte(8);handle_mouse_byte(0);handle_mouse_byte(0);
    check(!windows[APPEAR].opened);
    handle_key(0x1d);handle_key(0x38);handle_key(0x20);
    check(desktop_focus==0);
    handle_key(0x01);check(desktop_focus==-1);
    handle_key(0xb8);handle_key(0x9d);
    printf("PASS: clean desktop and taskbar icon surfaces, Alt+Tab switcher cycling and reverse, escape, desktop arrow/Enter navigation, PS/2 close after navigation\n");
    return 0;
}
