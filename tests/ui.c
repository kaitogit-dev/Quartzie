#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static uint32_t framebuffer[1280*720],wall[1280*720],blur[1280*720];
static QDesktop desktop;
static uint32_t drag_back[1280*720];
static uint32_t reference_pixels[1280*720],reference_blur[1280*720];
static void check_reference(QDesktop *actual){
    static int compare_number;
    compare_number++;
    QDesktop reference=*actual;
    reference.pixels=reference_pixels;
    reference.blurred=reference_blur;
    reference.drag=-1;
    reference.drag_base=0;
    reference.drag_cache_valid=0;
    reference.dirty=1;
    reference.damage_valid=0;
    reference.clip_left=0;reference.clip_top=0;
    reference.clip_right=actual->width;reference.clip_bottom=actual->height;
    q_draw(&reference);
    for(size_t i=0;i<(size_t)actual->width*actual->height;i++){
        if(actual->pixels[i]!=reference_pixels[i]){
            fprintf(stderr,"DRAG COMPOSITOR MISMATCH check=%d x=%zu y=%zu got=%08x expected=%08x\n",compare_number,
                i%(size_t)actual->width,i/(size_t)actual->width,
                actual->pixels[i],reference_pixels[i]);
            exit(1);
        }
    }
}
#define check(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void shot(const char *file){
    FILE *f=fopen(file,"wb");check(f!=0);
    fprintf(f,"P6\n%d %d\n255\n",desktop.width,desktop.height);
    for(int i=0;i<desktop.width*desktop.height;i++){
        uint32_t c=desktop.pixels[i];
        fputc(c>>16&255,f);fputc(c>>8&255,f);fputc(c&255,f);
    }
    fclose(f);
}
int main(void){
    q_init(&desktop,1280,720,framebuffer,wall,blur);
    check(desktop.width==1280&&desktop.active==-1&&desktop.drag==-1);
    q_draw(&desktop);check(desktop.frame_count==1&&!desktop.dirty);
    shot("build/desktop.ppm");
    q_draw(&desktop);check(desktop.frame_count==1);
    int left=(1280-632)/2,top=720-89;
    check(q_dock_hit(&desktop,left+38,top+28)==10);
    q_click(&desktop,left+38,top+28);check(desktop.menu&&!desktop.notifications);
    q_draw(&desktop);shot("build/start.ppm");
    q_click(&desktop,(1280-510)/2+50,720-607+140);
    check(!desktop.menu&&desktop.windows[Q_FILES].open&&desktop.active==Q_FILES);
    q_draw(&desktop);shot("build/files.ppm");
    q_key(&desktop,0,18);check(desktop.notifications);
    q_click(&desktop,left+77+77+30,top+32);
    check(!desktop.notifications&&desktop.windows[Q_NOTES].open&&desktop.active==Q_NOTES);
    int previous=desktop.note_length;
    q_key(&desktop,'A',0);check(desktop.note_length==previous+1&&desktop.notes[previous]=='A');
    q_key(&desktop,8,0);check(desktop.note_length==previous);
    q_draw(&desktop);shot("build/notes.ppm");
    QWindow w=desktop.windows[Q_NOTES];
    q_press(&desktop,w.x+100,w.y+25);
    check(desktop.drag==Q_NOTES);
    q_move(&desktop,desktop.mouse_x+90,desktop.mouse_y+30);
    check(desktop.windows[Q_NOTES].x!=w.x);
    q_release(&desktop);check(desktop.drag==-1);
    q_click(&desktop,left+77+4*77+30,top+32);
    check(desktop.active==Q_SETTINGS);
    int theme=desktop.theme;
    QWindow settings=desktop.windows[Q_SETTINGS];
    int settings_x=settings.x+32,settings_y=settings.y+60;
    int settings_sidebar=(settings.w-64)/4;
    if(settings_sidebar<175)settings_sidebar=175;
    if(settings_sidebar>220)settings_sidebar=220;
    int settings_content=settings_x+settings_sidebar+25;
    int settings_width=settings.w-64-settings_sidebar-25;
    q_click(&desktop,settings_x+36,settings_y+128+2*43+19);
    check(desktop.settings_page==2);
    q_click(&desktop,settings_content+settings_width-43,settings_y+166);
    check(desktop.theme!=theme);
    q_draw(&desktop);shot("build/settings.ppm");
    q_key(&desktop,0,19);check(desktop.quick&&!desktop.notifications);
    q_click(&desktop,1280-363+39,72+80+35);check(desktop.theme==theme);
    q_click(&desktop,left+77+3*77+30,top+32);check(desktop.active==Q_CALCULATOR);
    q_key(&desktop,'1',0);q_key(&desktop,'2',0);q_key(&desktop,'+',0);q_key(&desktop,'3',0);q_key(&desktop,'=',0);
    check(strcmp(desktop.answer,"15")==0);
    q_draw(&desktop);shot("build/calculator.ppm");
    for(int i=0;i<Q_APPS;i++){
        int tx=(1280-510)/2+26+(i%4)*116+47;
        int ty=720-607+112+(i/4)*103+42;
        q_click(&desktop,left+38,top+28);
        check(desktop.menu);
        q_click(&desktop,tx,ty);
        check(desktop.active==i&&!desktop.menu);
    }
    q_move(&desktop,left+77+2*77+30,top+30);
    check(desktop.hover==Q_TERMINAL);
    q_draw(&desktop);
    check(framebuffer[0]!=0&&framebuffer[100*1280+100]!=0);
    q_init(&desktop,1280,720,framebuffer,wall,blur);
    q_drag_buffers(&desktop,drag_back);
    q_open(&desktop,Q_NOTES);
    q_draw(&desktop);
    QWindow original=desktop.windows[Q_NOTES];
    q_press(&desktop,original.x+90,original.y+25);
    check(desktop.drag==Q_NOTES);
    q_draw(&desktop);
    check(desktop.drag_cache_valid&&!desktop.damage_valid);
    q_move(&desktop,original.x+130,original.y+60);
    q_draw(&desktop);
    check(desktop.damage_valid&&desktop.damage_w>0&&desktop.damage_h>0);
    check(desktop.windows[Q_NOTES].x==original.x+40);
    q_move(&desktop,original.x+130,719);
    q_draw(&desktop);
    check(desktop.windows[Q_NOTES].y+desktop.windows[Q_NOTES].h>720-89);
    q_release(&desktop);
    q_draw(&desktop);
    check(!desktop.drag_cache_valid&&!desktop.damage_valid);
    q_init(&desktop,1280,720,framebuffer,wall,blur);
    q_drag_buffers(&desktop,drag_back);
    q_open(&desktop,Q_FILES);
    q_open(&desktop,Q_SETTINGS);
    q_draw(&desktop);
    check_reference(&desktop);
    QWindow moving=desktop.windows[Q_SETTINGS];
    int px=moving.x+135,py=moving.y+27;
    q_press(&desktop,px,py);
    q_draw(&desktop);
    check_reference(&desktop);
    for(int n=0;n<14;n++){
        int x=px+((n%7)-3)*72;
        int y=py+((n%5)-2)*83;
        if(n>7)y=703;
        q_move(&desktop,x,y);
        q_draw(&desktop);
        check_reference(&desktop);
    }
    q_release(&desktop);
    q_draw(&desktop);
    check_reference(&desktop);
    printf("PASS: correct layered dragging, pixel-exact fullscreen comparison, dock overlap, input and applications\n");
    return 0;
}
