#include "ui.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static QDesktop ui;
static QStorage storage;
static uint32_t pixels[1280*720],wallpaper[1280*720],workspace[1280*720];
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL Settings line %d: %s\n",__LINE__,#c);exit(1);}}while(0)

static void type_query(const char *value){
    for(int i=0;value[i];i++)q_key(&ui,(uint16_t)value[i],0);
}
static void nav(int page){
    QWindow *w=&ui.windows[Q_SETTINGS];
    q_click(&ui,w->x+32+36,w->y+60+128+page*43+19);
    CHECK(ui.settings_page==page);
}
int main(void){
    qfs_init(&storage,0,0);
    q_init(&ui,1280,720,pixels,wallpaper,workspace);
    q_attach_storage(&ui,&storage,0,0);
    q_draw(&ui);
    q_enable_animations(&ui);
    q_open(&ui,Q_SETTINGS);
    q_draw(&ui);
    QWindow *w=&ui.windows[Q_SETTINGS];
    CHECK(w->w==1056&&w->h==616);
    CHECK(ui.settings_page==0&&ui.active==Q_SETTINGS);
    int bx=w->x+32,by=w->y+60;
    int side=(w->w-64)/4;
    if(side>220)side=220;
    if(side<175)side=175;
    int content=bx+side+25,width=w->w-64-side-25;
    q_click(&ui,content+width/4,by+252);
    CHECK(ui.settings_page==1);
    CHECK(ui.animations_enabled==1);
    q_click(&ui,content+width-40,by+217);
    CHECK(ui.animations_enabled==0);
    q_draw(&ui);
    q_click(&ui,content+width-40,by+217);
    CHECK(ui.animations_enabled==1);
    nav(2);
    q_click(&ui,content+width-48,by+167);
    CHECK(ui.theme==1);
    q_draw(&ui);
    q_click(&ui,content+43,by+167);
    CHECK(ui.theme==0);
    q_click(&ui,content+44+2*61,by+307);
    CHECK(ui.accent==2);
    type_query("accent");
    CHECK(ui.settings_search_length==6&&ui.settings_page==2);
    q_draw(&ui);
    q_key(&ui,'\r',0);
    CHECK(ui.settings_search_length==0&&ui.settings_page==2);
    type_query("zzzzz");
    q_key(&ui,27,0);
    CHECK(ui.settings_search_length==0&&ui.active==Q_SETTINGS);
    nav(4);
    CHECK(storage.count==1);
    q_click(&ui,content+80,by+157);
    CHECK(ui.active==Q_FILES&&ui.file_volume==storage.ram_index);
    q_open(&ui,Q_SETTINGS);
    nav(3);
    int row_h=(w->h-230)/5;
    if(row_h>64)row_h=64;
    if(row_h<45)row_h=45;
    q_click(&ui,content+120,by+92+2*(row_h+9)+row_h/2);
    CHECK(ui.active==Q_PHOTOS);
    q_open(&ui,Q_SETTINGS);
    nav(5);
    q_draw(&ui);
    CHECK(ui.active==Q_SETTINGS);
    CHECK(ui.settings_page==5);
    puts("PASS: Settings navigation, appearance, motion, search, drives and app links");
    return 0;
}
