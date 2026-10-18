#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static uint32_t framebuffer[1280*720],wall[1280*720],blur[1280*720];
static QDesktop desktop;
#define check(x) do { if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
int main(void){
    q_init(&desktop,1280,720,framebuffer,wall,blur);
    check(desktop.width==1280&&desktop.active==-1&&desktop.drag==-1);
    q_draw(&desktop);
    check(desktop.frame_count==1&&!desktop.dirty);
    q_draw(&desktop);
    check(desktop.frame_count==1);
    int task=(1280-448)/2;
    q_click(&desktop,task+30,680);
    check(desktop.menu&&!desktop.notifications);
    q_click(&desktop,(1280-490)/2+25+20,720-542+110+20);
    check(!desktop.menu&&desktop.windows[Q_FILES].open&&desktop.active==Q_FILES);
    q_click(&desktop,1280-150,680);
    check(desktop.notifications);
    q_click(&desktop,task+78+55+23,680);
    check(!desktop.notifications&&desktop.windows[Q_NOTES].open&&desktop.active==Q_NOTES);
    int previous=desktop.note_length;
    q_key(&desktop,'A',0);
    check(desktop.note_length==previous+1&&desktop.notes[previous]=='A');
    q_key(&desktop,8,0);
    check(desktop.note_length==previous);
    QWindow w=desktop.windows[Q_NOTES];
    q_press(&desktop,w.x+100,w.y+15);
    check(desktop.drag==Q_NOTES);
    q_move(&desktop,desktop.mouse_x+90,desktop.mouse_y+30);
    check(desktop.windows[Q_NOTES].x!=w.x);
    q_release(&desktop);
    check(desktop.drag==-1);
    q_click(&desktop,task+78+4*55+23,680);
    check(desktop.active==Q_SETTINGS);
    int theme=desktop.theme;
    QWindow settings=desktop.windows[Q_SETTINGS];
    q_click(&desktop,settings.x+50,settings.y+67+70);
    check(desktop.theme!=theme);
    q_draw(&desktop);
    check(desktop.frame_count==2);
    q_click(&desktop,1280-40,680);
    check(desktop.quick&&!desktop.notifications);
    q_click(&desktop,1280-365+19+35,720-435+80+30);
    check(desktop.theme==theme);
    q_click(&desktop,task+78+3*55+23,680);
    check(desktop.active==Q_CALCULATOR);
    q_key(&desktop,'1',0);q_key(&desktop,'2',0);q_key(&desktop,'+',0);q_key(&desktop,'3',0);q_key(&desktop,'=',0);
    check(strcmp(desktop.answer,"15")==0);
    q_draw(&desktop);
    check(framebuffer[0]!=0&&framebuffer[100*1280+100]!=0);
    printf("PASS: deterministic redraws, isolated notification clicks, menu launches, native window focus and drag, settings, notes, calculator\n");
    return 0;
}
