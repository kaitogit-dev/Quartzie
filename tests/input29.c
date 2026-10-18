#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb=0x2000000;
uint16_t boot_pitch=7680,boot_width=1920,boot_height=1080;
uint8_t boot_bpp=32;
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr,"FAIL input29 line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)
static void map_frame(uintptr_t address) {
    CHECK(mmap((void*)address,1920u*1080u*4u,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=MAP_FAILED);
}
static void send_mouse(int buttons,int dx,int dy) {
    handle_mouse_byte((uint8_t)(8|buttons));
    handle_mouse_byte((uint8_t)dx);
    handle_mouse_byte((uint8_t)(-dy));
}
int main(void) {
    map_frame(0x400000);map_frame(0xD00000);map_frame(0x2000000);
    sw=1920;sh=1080;mouse_x=900;mouse_y=650;
    for(int i=0;i<APPS;i++){
        windows[i].x=440+14*(i%4);windows[i].y=160+15*(i%4);
        windows[i].w=1040;windows[i].h=690;
    }
    repaint();
    CHECK(frame_valid && pointer_saved && cached_wallpaper==wallpaper);
    CHECK((_binary_build_wallpaper_bin_end - _binary_build_wallpaper_bin_start)==256*144*2);
    for (int x=340;x<1870;x+=13) {
        uint32_t pixel=canvas[23u*sw+x];
        CHECK((pixel&0x0000ff00u)!=0);
        CHECK(pixel==desktop_cache[23u*sw+x]);
    }
    int items[APPS],count=taskbar_items(items);
    CHECK(count==7);
    int bar=taskbar_left(count);
    CHECK(taskbar_hit(bar+23,sh-33)==FILES);
    CHECK(taskbar_hit(bar+23,sh-54)==FILES);
    CHECK(taskbar_hit(bar+23,sh-7)==-1);
    int initial_x=mouse_x,initial_y=mouse_y;
    send_mouse(0,7,5);
    CHECK(mouse_x==initial_x+7 && mouse_y==initial_y+5);
    CHECK(pointer_only || dirty);
    if(pointer_only)move_pointer_only();else repaint();
    CHECK(pointer_saved);
    CHECK(pointer_frames>0 || damage_frames>0);
    handle_key(0x1d);handle_key(0x12);handle_key(0x92);handle_key(0x9d);
    CHECK(windows[FILES].opened && !windows[FILES].minimized);
    repaint();
    send_mouse(0,9,-6);
    CHECK(mouse_x==initial_x+16 && mouse_y==initial_y-1);
    if(pointer_only)move_pointer_only();else repaint();
    CHECK(pointer_saved);
    for(int n=0;n<3;n++)repaint();
    CHECK(round_layer_errors==0 && round_layer_depth==0);
    puts("PASS: PNG wallpaper resource, no top-row color corruption, vertically-centered taskbar hit targets, live PS/2 cursor before and after Explorer opening");
}
