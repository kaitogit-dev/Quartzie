#include "ui.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static QDesktop ui;
static uint32_t pixels[1280*720],wallpaper[1280*720],blurred[1280*720];
#define REQUIRE(condition) do{if(!(condition)){fprintf(stderr,"FAIL timing line %d: %s\n",__LINE__,#condition);exit(1);}}while(0)
int main(void){
    q_init(&ui,1280,720,pixels,wallpaper,blurred);
    q_draw(&ui);
    q_enable_animations(&ui);
    q_open(&ui,Q_FILES);
    q_draw(&ui);
    REQUIRE(ui.scene_tween_active);
    REQUIRE(ui.scene_elapsed_us==0);
    REQUIRE(ui.scene_tween_step==0);
    q_tick_timed(&ui,3000);
    REQUIRE(!ui.dirty);
    q_tick_timed(&ui,3000);
    REQUIRE(!ui.dirty);
    q_tick_timed(&ui,3000);
    REQUIRE(!ui.dirty&&ui.scene_elapsed_us==9000);
    q_tick_timed(&ui,7000);
    REQUIRE(ui.dirty&&ui.scene_tween_step==1&&ui.scene_elapsed_us==16000);
    q_draw(&ui);
    uint32_t elapsed=ui.scene_elapsed_us;
    for(int n=0;n<50&&ui.scene_tween_active;n++){
        q_tick_timed(&ui,16000);
        if(ui.dirty)q_draw(&ui);
    }
    REQUIRE(ui.scene_elapsed_us==110000);
    REQUIRE(!ui.scene_tween_active);
    REQUIRE(ui.scene_tween_step>=8);
    REQUIRE(ui.scene_elapsed_us>elapsed);
    QWindow *w=&ui.windows[Q_FILES];
    q_click(&ui,w->x+w->w-90,w->y+25);
    REQUIRE(ui.scene_tween_pending&&ui.scene_tween_kind==2);
    q_draw(&ui);
    REQUIRE(ui.scene_tween_active);
    for(int n=0;n<50&&ui.scene_tween_active;n++){
        q_tick_timed(&ui,9000);
        if(ui.dirty)q_draw(&ui);
    }
    REQUIRE(ui.scene_elapsed_us==125000&&!ui.scene_tween_active);
    REQUIRE(w->maximized);
    puts("PASS: elapsed-time eased opening/maximizing, 16ms frame scheduling without render-time input work");
    return 0;
}
