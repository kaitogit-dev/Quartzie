#include "ui.h"
#include "icons.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t scene[1280*720],wall[1280*720],blur[1280*720];
static uint32_t reference[1280*720],reference_blur[1280*720];
static uint32_t dock_cache[659*137];
static QDesktop ui;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL Start tween line %d: %s\n",__LINE__,#x);exit(1);}}while(0)

static void verify_frame(void){
    QDesktop full=ui;
    full.pixels=reference;
    full.blurred=reference_blur;
    full.dock_background=0;
    full.dock_cache_valid=0;
    full.drag_base=0;
    full.drag=-1;
    full.hover_only=0;
    full.dirty=1;
    full.scene_tween_pending=0;
    full.scene_tween_active=0;
    q_draw(&full);
    for(size_t i=0;i<1280u*720u;i++){
        if(scene[i]!=reference[i]){
            fprintf(stderr,"Start hover mismatch at %zu,%zu\n",i%1280u,i/1280u);
            exit(1);
        }
    }
}

int main(void){
    q_init(&ui,1280,720,scene,wall,blur);
    q_dock_buffers(&ui,dock_cache);
    q_draw(&ui);
    CHECK(ui.dock_cache_valid);
    CHECK(q_icon_pixels(QI_START)!=0);
    int x=(ui.width-555)/2+37,y=ui.height-89+36;
    CHECK(q_dock_hit(&ui,x,y)==10);
    CHECK(!ui.start_hover&&!ui.start_lift);
    q_enable_animations(&ui);
    q_move(&ui,x,y);
    CHECK(ui.start_hover&&ui.hover==-1&&ui.hover_only);
    q_draw(&ui);
    verify_frame();
    q_tick_timed(&ui,16000);
    CHECK(ui.start_lift>0&&ui.start_lift<256);
    CHECK(ui.dirty&&ui.hover_only);
    q_draw(&ui);
    CHECK(ui.damage_valid&&ui.damage_h<250);
    verify_frame();
    for(int i=0;i<20;i++){q_tick_timed(&ui,8000);if(ui.dirty)q_draw(&ui);}
    CHECK(ui.start_lift==256);
    verify_frame();
    q_click(&ui,x,y);
    CHECK(ui.menu&&!ui.windows[Q_FILES].open);
    CHECK(ui.start_pulse==256);
    CHECK(ui.scene_tween_pending);
    q_draw(&ui);
    for(int i=0;i<24;i++){q_tick_timed(&ui,16000);if(ui.dirty)q_draw(&ui);}
    CHECK(!ui.scene_tween_active&&!ui.scene_tween_pending);
    CHECK(ui.start_pulse==0);
    CHECK(ui.menu);
    q_click(&ui,x,y);
    CHECK(!ui.menu);
    q_draw(&ui);
    for(int i=0;i<24;i++){q_tick_timed(&ui,16000);if(ui.dirty)q_draw(&ui);}
    CHECK(!ui.scene_tween_active);
    q_move(&ui,25,ui.height-150);
    CHECK(!ui.start_hover);
    q_draw(&ui);
    for(int i=0;i<20;i++){q_tick_timed(&ui,8000);if(ui.dirty)q_draw(&ui);}
    CHECK(ui.start_lift==0);
    verify_frame();
    puts("PASS: Quartzie Start logo, hover lift, tooltip, pulse, menu tween and pixel-exact dock redraw");
    return 0;
}
