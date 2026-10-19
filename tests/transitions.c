#include "ui.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t pixels[1280*720],wall[1280*720],work[1280*720],reference_pixels[1280*720],reference_work[1280*720];
static QDesktop ui;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL transition line %d: %s\n",__LINE__,#x);exit(1);}}while(0)

static int distance_color(uint32_t a,uint32_t b){
    int dr=(int)(a>>16&255)-(int)(b>>16&255);
    int dg=(int)(a>>8&255)-(int)(b>>8&255);
    int db=(int)(a&255)-(int)(b&255);
    return abs(dr)+abs(dg)+abs(db);
}
static uint64_t checksum(void){
    uint64_t n=1469598103934665603ull;
    for(int y=70;y<690;y+=7)for(int x=28;x<1250;x+=11){
        n=(n^pixels[(size_t)y*1280+x])*1099511628211ull;
    }
    return n;
}
static void same_as_fresh(void){
    QDesktop ref=ui;
    ref.pixels=reference_pixels;
    ref.blurred=reference_work;
    ref.animations_enabled=0;
    ref.scene_tween_active=0;
    ref.scene_tween_pending=0;
    ref.drag=-1;ref.drag_base=0;
    ref.drag_cache_valid=0;ref.hover_only=0;
    ref.clip_left=0;ref.clip_top=0;ref.clip_right=ui.width;ref.clip_bottom=ui.height;
    ref.dirty=1;
    q_draw(&ref);
    for(size_t i=0;i<1280u*720u;i++)if(pixels[i]!=reference_pixels[i]){
        fprintf(stderr,"FAIL final frame differs at %zu,%zu\n",i%1280u,i/1280u);
        exit(1);
    }
}
static void animate(void){
    CHECK(ui.scene_tween_pending);
    uint64_t before=checksum();
    q_draw(&ui);
    CHECK(ui.scene_tween_active);
    CHECK(checksum()==before);
    CHECK(ui.damage_valid);
    q_tick(&ui);q_draw(&ui);
    CHECK(checksum()!=before);
    for(int n=0;n<16;n++){q_tick(&ui);if(ui.dirty)q_draw(&ui);}
    CHECK(!ui.scene_tween_active);
    same_as_fresh();
}
int main(void){
    q_init(&ui,1280,720,pixels,wall,work);
    q_draw(&ui);
    q_enable_animations(&ui);
    q_open(&ui,Q_FILES);
    QWindow *w=&ui.windows[Q_FILES];
    int center=w->x+w->w/2+(w->y+w->h/2)*1280;
    uint32_t before=pixels[center];
    q_draw(&ui);
    q_tick(&ui);
    q_draw(&ui);
    uint32_t during=pixels[center];
    QDesktop fresh=ui;
    fresh.pixels=reference_pixels;
    fresh.blurred=reference_work;
    fresh.animations_enabled=0;
    fresh.scene_tween_active=0;
    fresh.scene_tween_pending=0;
    fresh.drag=-1;
    fresh.drag_base=0;
    fresh.dirty=1;
    fresh.hover_only=0;
    q_draw(&fresh);
    CHECK(distance_color(during,reference_pixels[center])<distance_color(during,before));
    for(int n=0;n<16;n++){q_tick(&ui);if(ui.dirty)q_draw(&ui);}
    CHECK(!ui.scene_tween_active);
    same_as_fresh();
    q_click(&ui,w->x+w->w-90,w->y+25);
    CHECK(w->maximized);
    animate();
    q_click(&ui,w->x+w->w-90,w->y+25);
    CHECK(!w->maximized);
    animate();
    q_click(&ui,w->x+w->w-140,w->y+25);
    CHECK(w->minimized&&w->open);
    animate();
    q_open(&ui,Q_FILES);
    animate();
    q_click(&ui,w->x+w->w-40,w->y+25);
    CHECK(!w->open);
    animate();
    q_key(&ui,0,17);
    CHECK(ui.menu);
    animate();
    q_key(&ui,27,0);
    CHECK(!ui.menu);
    animate();
    q_key(&ui,0,18);
    CHECK(ui.notifications);
    animate();
    q_key(&ui,0,19);
    CHECK(ui.quick&&!ui.notifications);
    animate();
    q_key(&ui,27,0);
    animate();
    puts("PASS: opening, resizing, minimize, restore, close, launcher and popovers match full rendering");
    return 0;
}
