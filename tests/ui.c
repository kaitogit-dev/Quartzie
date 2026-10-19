#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static uint32_t framebuffer[1280*720],wall[1280*720],blur[1280*720];
static QDesktop desktop;
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
    int left=(1280-555)/2,top=720-89;
    check(q_dock_hit(&desktop,left+38,top+28)==10);
    q_click(&desktop,left+38,top+28);check(desktop.menu&&!desktop.notifications);
    q_draw(&desktop);shot("build/start.ppm");
    q_click(&desktop,(1280-510)/2+50,720-607+140);
    check(!desktop.menu&&desktop.windows[Q_FILES].open&&desktop.active==Q_FILES);
    q_draw(&desktop);shot("build/files.ppm");
    q_click(&desktop,1280-97,30);check(desktop.notifications);
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
    q_move(&desktop,desktop.mouse_x,719);
    check(desktop.windows[Q_NOTES].y+desktop.windows[Q_NOTES].h>top);
    q_release(&desktop);check(desktop.drag==-1);
    q_click(&desktop,left+77+4*77+30,top+32);
    check(desktop.active==Q_SETTINGS);
    int theme=desktop.theme;
    QWindow settings=desktop.windows[Q_SETTINGS];
    q_click(&desktop,settings.x+60,settings.y+85+71);
    check(desktop.theme!=theme);
    q_draw(&desktop);shot("build/settings.ppm");
    q_click(&desktop,1280-47,30);check(desktop.quick&&!desktop.notifications);
    q_click(&desktop,1280-363+39,72+80+35);check(desktop.theme==theme);
    q_click(&desktop,left+77+3*77+30,top+32);check(desktop.active==Q_CALCULATOR);
    q_key(&desktop,'1',0);q_key(&desktop,'2',0);q_key(&desktop,'+',0);q_key(&desktop,'3',0);q_key(&desktop,'=',0);
    check(strcmp(desktop.answer,"15")==0);
    q_draw(&desktop);shot("build/calculator.ppm");
    for(int i=0;i<Q_APPS;i++){
        int tx=(1280-510)/2+26+(i%3)*159+55;
        int ty=720-607+112+(i/3)*144+60;
        q_click(&desktop,left+38,top+28);
        check(desktop.menu);
        q_click(&desktop,tx,ty);
        check(desktop.active==i&&!desktop.menu);
    }
    q_move(&desktop,left+77+2*77+30,top+30);
    check(desktop.hover==Q_TERMINAL);
    q_draw(&desktop);
    check(framebuffer[0]!=0&&framebuffer[100*1280+100]!=0);
    printf("PASS: antialiased desktop, routed dock and menu input, mouse hover, window focus and drag, settings, notes and calculator\n");
    return 0;
}
