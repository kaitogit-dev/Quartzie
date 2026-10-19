#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static uint32_t pixels[1280*720],wall[1280*720],work[1280*720],drag_base[1280*720];
static QDesktop ui;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL tween line %d: %s\n",__LINE__,#x);exit(1);}}while(0)

static uint64_t snapshot(void){
    uint64_t hash=1469598103934665603ull;
    for(int y=40;y<660;y+=9)for(int x=210;x<1140;x+=7){
        hash^=pixels[(size_t)y*1280+x];
        hash*=1099511628211ull;
    }
    return hash;
}
static void finish(void){
    for(int n=0;n<16;n++){q_tick(&ui);if(ui.dirty)q_draw(&ui);}
}
int main(void){
    q_init(&ui,1280,720,pixels,wall,work);
    q_drag_buffers(&ui,drag_base);
    q_draw(&ui);
    CHECK(ui.animations_enabled==0);
    q_enable_animations(&ui);
    q_open(&ui,Q_FILES);
    CHECK(ui.windows[Q_FILES].tween_kind==1);
    q_draw(&ui);
    uint64_t first=snapshot();
    q_tick(&ui);
    CHECK(ui.windows[Q_FILES].tween_step==1);
    q_draw(&ui);
    CHECK(snapshot()!=first);
    finish();
    CHECK(ui.windows[Q_FILES].tween_kind==0);
    CHECK(!ui.dirty);
    QWindow *window=&ui.windows[Q_FILES];
    int x=window->x,y=window->y,width=window->w,height=window->h;
    q_click(&ui,x+width-90,y+25);
    CHECK(window->maximized&&window->tween_kind==2);
    CHECK(window->tween_x==x&&window->tween_y==y);
    q_draw(&ui);
    first=snapshot();
    q_tick(&ui);
    q_draw(&ui);
    CHECK(snapshot()!=first);
    finish();
    CHECK(window->tween_kind==0);
    CHECK(window->x==12&&window->y==69);
    q_click(&ui,window->x+window->w-90,window->y+25);
    CHECK(!window->maximized&&window->tween_kind==2);
    finish();
    CHECK(window->x==x&&window->y==y);
    CHECK(window->w==width&&window->h==height);
    int left=(ui.width-555)/2,top=ui.height-89;
    q_move(&ui,left+77+77+30,top+30);
    q_draw(&ui);
    q_tick(&ui);
    CHECK(ui.dock_lift[Q_NOTES]>0);
    q_draw(&ui);
    CHECK(ui.damage_valid&&ui.damage_h<250);
    finish();
    CHECK(ui.dock_lift[Q_NOTES]==256);
    q_move(&ui,20,70);
    q_draw(&ui);
    finish();
    CHECK(ui.dock_lift[Q_NOTES]==0);
    q_click(&ui,window->x+window->w-140,window->y+26);
    CHECK(window->minimized&&window->open);
    CHECK(ui.dock_pulse[Q_FILES]>0);
    finish();
    CHECK(ui.dock_pulse[Q_FILES]==0);
    q_open(&ui,Q_FILES);
    CHECK(window->tween_kind==1);
    q_draw(&ui);
    q_press(&ui,window->x+80,window->y+25);
    CHECK(ui.drag==Q_FILES);
    q_tick(&ui);
    CHECK(window->tween_kind==0);
    q_release(&ui);
    q_draw(&ui);
    CHECK(!ui.drag_cache_valid);
    puts("PASS: window pop, maximize/restore easing, dock lift, minimize pulse, drag cancellation");
    return 0;
}
