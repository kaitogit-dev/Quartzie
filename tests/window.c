#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static uint32_t canvas[1920*1080],background[1920*1080],blur[1920*1080],drag[1920*1080];
static QDesktop ui;
static uint32_t dock_backdrop[659*137];
static uint32_t reference_pixels[1920*1080],reference_blur[1920*1080];
static void same_as_full(void){
    QDesktop reference=ui;
    reference.pixels=reference_pixels;reference.blurred=reference_blur;
    reference.drag=-1;reference.drag_base=0;reference.dirty=1;
    reference.hover_only=0;reference.drag_cache_valid=0;
    q_draw(&reference);
    for(size_t i=0;i<1920u*1080u;i++)if(canvas[i]!=reference_pixels[i]){
        fprintf(stderr,"FAIL hover pixel %zu,%zu\n",i%1920u,i/1920u);
        exit(1);
    }
}

#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL window line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void){
    q_init(&ui,1920,1080,canvas,background,blur);
    q_drag_buffers(&ui,drag);
    q_dock_buffers(&ui,dock_backdrop);
    CHECK(ui.width==1920&&ui.height==1080);
    int start_x=(ui.width-555)/2+37,start_y=ui.height-89+35;
    int files_x=(ui.width-555)/2+107;
    CHECK(q_dock_hit(&ui,start_x,start_y)==10);
    CHECK(q_dock_hit(&ui,files_x,start_y)==Q_FILES);
    q_move(&ui,start_x,start_y);
    q_click(&ui,ui.mouse_x,ui.mouse_y);
    CHECK(ui.menu&&!ui.windows[Q_FILES].open);
    q_click(&ui,start_x,start_y);
    CHECK(!ui.menu&&!ui.windows[Q_FILES].open);
    q_move(&ui,files_x,start_y);
    CHECK(ui.hover==Q_FILES);
    q_click(&ui,ui.mouse_x,ui.mouse_y);
    CHECK(ui.windows[Q_FILES].open&&!ui.menu);
    q_click(&ui,ui.windows[Q_FILES].x+ui.windows[Q_FILES].w-40,ui.windows[Q_FILES].y+25);
    CHECK(!ui.windows[Q_FILES].open);
    q_draw(&ui);
    q_open(&ui,Q_NOTES);
    q_draw(&ui);
    QWindow original=ui.windows[Q_NOTES];
    int min_x=original.x+original.w-140,min_y=original.y+26;
    q_click(&ui,min_x,min_y);
    CHECK(ui.windows[Q_NOTES].open&&ui.windows[Q_NOTES].minimized&&ui.active!=Q_NOTES);
    q_draw(&ui);
    int dock_left=(ui.width-555)/2,dock_top=ui.height-89;
    q_click(&ui,dock_left+77+77+30,dock_top+32);
    CHECK(ui.windows[Q_NOTES].open&&!ui.windows[Q_NOTES].minimized&&ui.active==Q_NOTES);
    q_draw(&ui);
    QWindow *w=&ui.windows[Q_NOTES];
    q_click(&ui,w->x+w->w-90,w->y+25);
    CHECK(w->maximized&&w->w==1896&&w->h==998&&w->x==12&&w->y==69);
    q_draw(&ui);
    q_click(&ui,w->x+w->w-90,w->y+25);
    CHECK(!w->maximized&&w->w==original.w&&w->h==original.h);
    q_draw(&ui);
    q_click(&ui,w->x+w->w-40,w->y+25);
    CHECK(!w->open);
    q_open(&ui,Q_FILES);
    q_draw(&ui);
    q_move(&ui,dock_left+77+2*77+30,dock_top+30);
    CHECK(ui.hover==Q_TERMINAL&&ui.hover_only);
    CHECK(ui.dock_cache_valid);
    q_draw(&ui);
    CHECK(ui.damage_valid&&ui.damage_w<ui.width&&ui.damage_h<ui.height);
    CHECK(ui.damage_h<250);
    same_as_full();
    q_move(&ui,dock_left+77+4*77+30,dock_top+30);
    CHECK(ui.hover==Q_SETTINGS&&ui.hover_only);
    q_draw(&ui);
    CHECK(ui.damage_valid);
    same_as_full();
    puts("PASS: 1920x1080 viewport, independent minimize maximize close, restore, partial dock hover");
    return 0;
}
