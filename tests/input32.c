#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb=0x2000000;
uint16_t boot_pitch=7680,boot_width=1920,boot_height=1080;
uint8_t boot_bpp=32;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL input32 line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static void packet(uint8_t a,uint8_t b,uint8_t c) { handle_mouse_byte(a);handle_mouse_byte(b);handle_mouse_byte(c); }
static void click(int x,int y) {
    mouse_x=x;mouse_y=y;
    packet(9,0,0);packet(8,0,0);
    CHECK(!left_down);
}
static void move_to(int x, int y) {
    while (mouse_x != x || mouse_y != y) {
        int dx = x - mouse_x;
        int dy = mouse_y - y;
        if (dx > 100) dx = 100;
        if (dx < -100) dx = -100;
        if (dy > 100) dy = 100;
        if (dy < -100) dy = -100;
        packet((uint8_t)(8 | (dx < 0 ? 16 : 0) | (dy < 0 ? 32 : 0)), (uint8_t)dx, (uint8_t)dy);
    }
}
static void click_current(void) {
    packet(9, 0, 0);
    packet(8, 0, 0);
    CHECK(!left_down);
}
static void render(void) {
    repaint();
    CHECK(!memcmp(canvas,(void*)(uintptr_t)boot_fb,1920u*1080u*4));
    CHECK(!round_layer_errors&&round_layer_depth==0);
}
static void reset_apps(void) {
    for(int i=0;i<APPS;i++) {
        windows[i].opened=0;windows[i].minimized=0;windows[i].order=0;
        anim_ticks[i]=0;anim_visible[i]=0;icon_motion[i]=0;
    }
    next_order=0;start_open=quick_open=notification_open=context_open=run_open=0;
    for(int i=0;i<PANEL_COUNT;i++){panel_step[i]=0;panel_goal[i]=0;panel_was_visible[i]=0;}
    dirty=1;taskbar_dirty=1;click_wait=0;click_class=-1;
}
int main(void) {
    uintptr_t a[]={0x400000,0xd00000,0x2000000};
    for(int i=0;i<3;i++)CHECK(mmap((void*)a[i],1920u*1080u*4,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED,-1,0)!=MAP_FAILED);
    sw=1920;sh=1080;mouse_x=1000;mouse_y=490;
    for(int i=0;i<APPS;i++){windows[i].x=435+(i%4)*14;windows[i].y=155+(i%4)*15;windows[i].w=1050;windows[i].h=700;}
    render();
    CHECK(!notification_open);
    int list[APPS],count=taskbar_items(list);
    for(int j=0;j<7;j++) {
        reset_apps();render();
        count=taskbar_items(list);
        int x=taskbar_left(count)+j*taskbar_step()+taskbar_width()/2;
        click(x,sh-33);
        CHECK(windows[list[j]].opened&&!windows[list[j]].minimized);
        CHECK(active_window()==list[j]);
        CHECK(!notification_open&&!quick_open&&!start_open);
        render();
        CHECK(canvas[(size_t)(windows[list[j]].y+100)*sw+windows[list[j]].x+100]!=desktop_cache[(size_t)(windows[list[j]].y+100)*sw+windows[list[j]].x+100]);
    }
    reset_apps();render();
    click(37,sh-32);CHECK(start_open);render();CHECK(!notification_open);
    int mx=menu_left(),my=menu_top();
    click(mx+23+41,my+139+39);
    CHECK(windows[menu_row_app(0)].opened&&active_window()==menu_row_app(0));
    CHECK(!notification_open&&!start_open);render();
    for (int j=0;j<APPS;j++) {
        reset_apps();render();
        click(37,sh-32);CHECK(start_open);render();
        int app=menu_row_app(j);
        CHECK(app>=0&&app<APPS);
        click(menu_left()+23+(j%5)*92+41,menu_top()+139+(j/5)*93+39);
        CHECK(windows[app].opened&&!windows[app].minimized);
        CHECK(active_window()==app&&!start_open&&!notification_open);
        render();
    }
    reset_apps();render();
    click(sw-156,sh-32);CHECK(notification_open);render();
    int bx=taskbar_left(taskbar_items(list))+taskbar_width()/2;
    click(bx,sh-33);
    CHECK(windows[FILES].opened&&!windows[FILES].minimized);
    CHECK(!notification_open);render();
    CHECK(!panel_visible(PANEL_NOTIFICATIONS));
    reset_apps();render();
    click(sw-156,sh-32);CHECK(notification_open);render();
    click(37,sh-32);CHECK(start_open&&!notification_open);render();
    CHECK(!panel_visible(PANEL_NOTIFICATIONS));
    reset_apps();render();
    click(sw-156,sh-32);CHECK(notification_open);render();
    click(taskbar_left(7)+taskbar_width()/2,sh-33);
    CHECK(windows[FILES].opened&&!notification_open);
    click(37,sh-32);CHECK(start_open);render();
    click(menu_left()+23+41,menu_top()+139+39);
    CHECK(windows[FILES].opened);CHECK(!notification_open);render();
    reset_apps();render();
    mouse_index=0;
    handle_mouse_byte(0xfa);handle_mouse_byte(0xfe);handle_mouse_byte(0x88);
    CHECK(mouse_index==0);
    packet(8,5,0);CHECK(mouse_x>=0);
    reset_apps();render();
    mouse_x=sw/2;mouse_y=sh/2;
    move_to(sw-156,sh-32);
    click_current();
    CHECK(notification_open&&!start_open);
    move_to(taskbar_left(7)+taskbar_width()/2,sh-33);
    click_current();
    CHECK(windows[FILES].opened&&active_window()==FILES);
    CHECK(!notification_open);
    move_to(sw-156,sh-32);
    click_current();
    CHECK(notification_open);
    move_to(37,sh-32);
    click_current();
    CHECK(start_open&&!notification_open);
    move_to(menu_left()+23+41,menu_top()+139+39);
    click_current();
    CHECK(!start_open&&!notification_open);
    CHECK(windows[FILES].opened&&active_window()==FILES);
    render();
    puts("PASS: 7 distinct taskbar apps, all 10 Start pinned launches, notification isolation, popup dismissal, PS/2 ACK rejection, 1080p framebuffer");
}
