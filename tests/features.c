#include "ui.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static QDesktop ui;
static uint32_t framebuffer[1024*768],wallpaper[1024*768],scratch[1024*768];
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL feature test line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
static void key(char c){q_key(&ui,(uint16_t)c,0);}
int main(void){
    q_init(&ui,1024,768,framebuffer,wallpaper,scratch);
    q_draw(&ui);
    q_key(&ui,0,17);
    CHECK(ui.menu);
    key('c');key('a');key('l');
    CHECK(ui.menu_search_length==3&&!strcmp(ui.menu_search,"cal"));
    q_draw(&ui);
    key('\r');
    CHECK(ui.active==Q_CALCULATOR&&!ui.menu);
    CHECK(ui.menu_search_length==0);
    key('1');key('2');key('*');key('8');key('\r');
    CHECK(!strcmp(ui.answer,"96"));
    CHECK(ui.calc_history_count==1);
    CHECK(!strcmp(ui.calc_history[0],"12*8 = 96"));
    key('7');key('5');key(8);
    CHECK(!strcmp(ui.calc,"7"));
    key('+');key('5');key('\r');
    CHECK(!strcmp(ui.answer,"12"));
    CHECK(ui.calc_history_count==2);
    CHECK(!strcmp(ui.calc_history[0],"7+5 = 12"));
    key('1');key('/');key('0');key('\r');
    CHECK(!strcmp(ui.answer,"ERR")&&ui.calc_history_count==2);
    key('C');
    CHECK(!strcmp(ui.answer,"0")&&ui.calc_length==0);
    q_key(&ui,0,21);
    CHECK(ui.windows[Q_CALCULATOR].maximized);
    q_key(&ui,0,21);
    CHECK(!ui.windows[Q_CALCULATOR].maximized);
    q_key(&ui,0,20);
    CHECK(ui.windows[Q_CALCULATOR].minimized);
    q_key(&ui,0,14);
    CHECK(!ui.windows[Q_CALCULATOR].minimized);
    q_key(&ui,0,22);
    CHECK(!ui.windows[Q_CALCULATOR].open);
    q_key(&ui,0,17);
    CHECK(ui.menu);
    key('z');key('z');
    key('\r');
    CHECK(ui.menu&&ui.active!=Q_CALCULATOR);
    key(8);key(8);
    CHECK(ui.menu_search_length==0);
    q_click(&ui,(1024-510)/2+26+159*1+55,768-607+112+50);
    CHECK(ui.active==Q_NOTES);
    puts("PASS: searchable launcher, typed calculator history/editing, and F10-F12 window controls");
    return 0;
}
