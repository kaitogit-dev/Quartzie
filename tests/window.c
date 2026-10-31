#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static uint32_t canvas[1920*1080],background[1920*1080],blur[1920*1080],drag[1920*1080];
static QDesktop ui;
static uint32_t dock_backdrop[736*137];
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

static void check_snap_and_desktop(void){
    q_init(&ui,1280,720,canvas,background,blurred);
    q_open(&ui,Q_NOTES);
    QWindow *w=&ui.windows[Q_NOTES];
    QWindow initial=*w;
    q_snap_window(&ui,Q_NOTES,1);
    CHECK(w->snap_zone==1&&w->x==8&&w->w==632&&w->h==615);
    q_snap_window(&ui,Q_NOTES,2);
    CHECK(w->x==640&&w->w==632);
    q_snap_window(&ui,Q_NOTES,6);
    CHECK(w->x==640&&w->y==315&&w->h==308);
    q_snap_window(&ui,Q_NOTES,0);
    CHECK(!w->snap_zone&&w->x==initial.x&&w->y==initial.y&&w->w==initial.w);
    q_snap_window(&ui,Q_NOTES,1);
    q_key(&ui,0,21);
    CHECK(w->maximized&&!w->snap_zone);
    q_key(&ui,0,21);
    CHECK(!w->maximized&&w->x==initial.x&&w->w==initial.w);
    q_open(&ui,Q_FILES);
    q_switch_desktop(&ui,1);
    CHECK(ui.active==-1&&ui.desktop==1);
    q_open(&ui,Q_TERMINAL);
    CHECK(ui.windows[Q_TERMINAL].desktop==1);
    q_move_to_desktop(&ui,Q_NOTES,1);
    q_key(&ui,9,0);
    CHECK(ui.active==Q_NOTES);
    q_switch_desktop(&ui,0);
    CHECK(ui.active==Q_FILES);
    q_open(&ui,Q_NOTES);
    CHECK(ui.active==Q_NOTES&&ui.desktop==1);
    q_snap_window(&ui,Q_NOTES,1);
    q_press(&ui,w->x+155,w->y+28);
    q_move(&ui,530,230);
    q_release(&ui);
    CHECK(w->snap_zone==0&&w->w==initial.w);
    q_press(&ui,w->x+125,w->y+25);
    q_move(&ui,1,340);
    q_release(&ui);
    CHECK(w->snap_zone==1);
}


static void check_window_controls(void){
    q_init(&ui,1280,720,canvas,background,blurred);
    q_open(&ui,Q_NOTES);
    QWindow *w=&ui.windows[Q_NOTES];
    q_move(&ui,w->x+w->w-90,w->y+26);
    CHECK(ui.snap_menu_app==Q_NOTES);
    int sx=w->x+w->w-265,sy=w->y+48;
    q_click(&ui,sx+11+81+25,sy+37+22);
    CHECK(w->snap_zone==2);
    int dock_left=(ui.width-632)/2,dock_top=ui.height-89;
    q_click(&ui,dock_left+615,dock_top+30);
    CHECK(ui.desktop_view);
    int vx=(ui.width-522)/2,vy=dock_top-173;
    q_click(&ui,vx+16+2*169+35,vy+43+77);
    CHECK(w->desktop==2&&ui.desktop==0);
    q_click(&ui,dock_left+615,dock_top+30);
    q_click(&ui,vx+16+2*169+35,vy+43+20);
    CHECK(ui.desktop==2&&ui.active==Q_NOTES);
}

int main(void){
    q_init(&ui,1920,1080,canvas,background,blur);
    q_drag_buffers(&ui,drag);
    q_dock_buffers(&ui,dock_backdrop);
    CHECK(ui.width==1920&&ui.height==1080);
    int start_x=(ui.width-632)/2+37,start_y=ui.height-89+35;
    int files_x=(ui.width-632)/2+107;
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
    q_click(&ui,files_x,start_y);
    CHECK(ui.windows[Q_FILES].open&&ui.windows[Q_FILES].minimized&&ui.active==-1);
    q_click(&ui,files_x,start_y);
    CHECK(ui.windows[Q_FILES].open&&!ui.windows[Q_FILES].minimized&&ui.active==Q_FILES);
    q_open(&ui,Q_NOTES);
    CHECK(ui.active==Q_NOTES&&!ui.windows[Q_NOTES].minimized);
    q_click(&ui,files_x,start_y);
    CHECK(ui.active==Q_FILES&&!ui.windows[Q_FILES].minimized);
    CHECK(ui.windows[Q_NOTES].open&&!ui.windows[Q_NOTES].minimized);
    q_click(&ui,files_x,start_y);
    CHECK(ui.windows[Q_FILES].minimized&&ui.active==Q_NOTES);
    q_click(&ui,files_x,start_y);
    CHECK(!ui.windows[Q_FILES].minimized&&ui.active==Q_FILES);
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
    int dock_left=(ui.width-632)/2,dock_top=ui.height-89;
    q_click(&ui,dock_left+77+77+30,dock_top+32);
    CHECK(ui.windows[Q_NOTES].open&&!ui.windows[Q_NOTES].minimized&&ui.active==Q_NOTES);
    q_draw(&ui);
    QWindow *w=&ui.windows[Q_NOTES];
    q_click(&ui,w->x+w->w-90,w->y+25);
    CHECK(w->maximized&&w->w==ui.width&&w->h==ui.height&&w->x==0&&w->y==0);
    q_draw(&ui);
    CHECK(canvas[0]!=background[0]);
    CHECK(canvas[ui.width-1]!=background[ui.width-1]);
    CHECK(canvas[(size_t)(ui.height-1)*ui.width]!=background[(size_t)(ui.height-1)*ui.width]);
    CHECK(canvas[(size_t)ui.height*ui.width-1]!=background[(size_t)ui.height*ui.width-1]);
    q_set_theme(&ui,1);
    q_draw(&ui);
    CHECK(canvas[0]!=background[0]);
    CHECK(canvas[ui.width-1]!=background[ui.width-1]);
    CHECK(canvas[(size_t)ui.height*ui.width-1]!=background[(size_t)ui.height*ui.width-1]);
    q_set_theme(&ui,0);
    q_draw(&ui);
    q_click(&ui,w->x+w->w-90,w->y+25);
    CHECK(!w->maximized&&w->x==original.x&&w->y==original.y&&w->w==original.w&&w->h==original.h);
    q_draw(&ui);
    int title_x=w->x+210,title_y=w->y+25;
    q_press(&ui,title_x,title_y);
    q_move(&ui,title_x+85,6);
    q_release(&ui);
    CHECK(w->maximized&&w->x==0&&w->y==0&&w->w==ui.width&&w->h==ui.height);
    q_draw(&ui);
    q_press(&ui,260,25);
    q_release(&ui);
    CHECK(w->maximized);
    q_press(&ui,260,25);
    q_move(&ui,430,210);
    CHECK(!w->maximized&&ui.drag==Q_NOTES&&w->w==original.w&&w->h==original.h);
    q_release(&ui);
    CHECK(ui.drag==-1&&!w->maximized&&w->x>0&&w->y>12);
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
    puts("PASS: dock toggle, focus, maximize, drag-to-top, drag-to-restore, theme fill, and hover");
    check_snap_and_desktop();
    check_window_controls();
    return 0;
}
