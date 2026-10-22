#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ui.c"
static QDesktop desktop;
static uint32_t framebuffer[1024*768],wall[1024*768],work[1024*768],dock_cache[659*137];
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL dark theme line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int luminance(uint32_t color){return (int)(color>>16&255)*3+(int)(color>>8&255)*6+(int)(color&255);}
static uint64_t wall_checksum(void){
    uint64_t value=0;
    for(size_t i=0;i<1024u*768u;i++)value+=((uint64_t)wall[i]*(i%31+1));
    return value;
}
int main(void){
    q_init(&desktop,1024,768,framebuffer,wall,work);
    q_dock_buffers(&desktop,dock_cache);
    uint64_t original=wall_checksum();
    q_draw(&desktop);
    CHECK(desktop.dock_cache_valid);
    int dx=(desktop.width-555)/2+85,dy=desktop.height-89+16;
    uint32_t dock_light=desktop.pixels[dy*1024+dx];
    q_open(&desktop,Q_SETTINGS);
    q_draw(&desktop);
    QWindow *window=&desktop.windows[Q_SETTINGS];
    int wx=window->x+window->w/2,wy=window->y+9;
    uint32_t window_light=desktop.pixels[wy*1024+wx];
    desktop.glass_cached_app=Q_SETTINGS;
    desktop.drag_cache_valid=1;
    desktop.scene_tween_active=1;
    q_set_theme(&desktop,1);
    CHECK(desktop.theme&&desktop.dirty);
    CHECK(!desktop.dock_cache_valid&&!desktop.drag_cache_valid);
    CHECK(desktop.glass_cached_app==-1&&!desktop.scene_tween_active);
    CHECK(wall_checksum()==original);
    q_draw(&desktop);
    CHECK(wall_checksum()==original);
    CHECK(desktop.pixels[0]==wall[0]);
    uint32_t window_dark=desktop.pixels[wy*1024+wx];
    uint32_t dock_dark=desktop.pixels[dy*1024+dx];
    CHECK(luminance(window_dark)<luminance(window_light));
    CHECK(luminance(dock_dark)<luminance(dock_light));
    CHECK(luminance(window_dark)<360);
    q_set_theme(&desktop,0);
    CHECK(wall_checksum()==original);
    q_draw(&desktop);
    CHECK(desktop.pixels[0]==wall[0]);
    CHECK(desktop.theme==0);
    q_set_theme(&desktop,1);
    q_open(&desktop,Q_CALCULATOR);
    q_draw(&desktop);
    CHECK(wall_checksum()==original);
    QWindow *calc=&desktop.windows[Q_CALCULATOR];
    int cx=calc->x+32,cy=calc->y+60;
    uint32_t operator_dark=desktop.pixels[(cy+152)*1024+cx+281];
    CHECK(luminance(operator_dark)<480);
    q_click(&desktop,calc->x+calc->w-45,calc->y+24);
    q_open(&desktop,Q_SETTINGS);
    q_draw(&desktop);
    window=&desktop.windows[Q_SETTINGS];
    q_click(&desktop,window->x+60,window->y+60+36+32);
    CHECK(desktop.theme==0);
    CHECK(wall_checksum()==original);
    q_key(&desktop,0,19);
    q_draw(&desktop);
    q_click(&desktop,1024-363+38,72+80+30);
    CHECK(desktop.theme==1);
    CHECK(wall_checksum()==original);
    q_draw(&desktop);
    CHECK(desktop.pixels[0]==wall[0]);

    desktop.width=256;desktop.height=256;desktop.pixels=framebuffer;
    desktop.wallpaper=wall;desktop.clip_left=0;desktop.clip_top=0;
    desktop.clip_right=256;desktop.clip_bottom=256;
    desktop.theme=1;desktop.render_app=-1;desktop.glass_cached_app=-1;
    for(int y=0;y<256;y++)for(int x=0;x<256;x++)framebuffer[y*256+x]=x<128?0x303f61:0xa5b4d5;
    surface(&desktop,32,32,190,190,16,material(&desktop),180);
    uint32_t dark_left=framebuffer[128*256+90],dark_right=framebuffer[128*256+166];
    CHECK(luminance(dark_left)<350);
    CHECK(luminance(dark_right)<480);
    CHECK(luminance(dark_right)>luminance(dark_left)+28);
    puts("PASS: undimmed wallpaper, real dark glass, dark dock, controls, and complete theme invalidation");
    return 0;
}
