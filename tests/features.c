#include "ui.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static QDesktop ui;
static uint32_t framebuffer[1024*768],wallpaper[1024*768],scratch[1024*768];
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL feature test line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
static void key(char c){q_key(&ui,(uint16_t)c,0);}
static QStorage selection_storage;
static QDesktop selection_ui;
static uint32_t selection_pixels[1024*768],selection_wall[1024*768],selection_workspace[1024*768];
static int full_search_x_for_test(int width){int full=width-30;int search=full/4;if(search<144)search=144;if(search>225)search=225;return full-search+19;}
static int files_columns_for_test(int width){int cols=(width-46-188)/134;return cols<1?1:cols;}
static void check_file_selection(void){
    qfs_init(&selection_storage,0,0);
    CHECK(qfs_mkdir(&selection_storage,0,"\\FolderA")==0);
    CHECK(qfs_write(&selection_storage,0,"\\alpha.txt","first",5)==0);
    CHECK(qfs_write(&selection_storage,0,"\\beta.txt","second",6)==0);
    q_init(&selection_ui,1024,768,selection_pixels,selection_wall,selection_workspace);
    q_attach_storage(&selection_ui,&selection_storage,0,0);
    q_open(&selection_ui,Q_FILES);
    CHECK(selection_ui.file_count==3&&selection_ui.file_selected==-1);
    QWindow *w=&selection_ui.windows[Q_FILES];
    int cols=files_columns_for_test(w->w);
    int pitch=(w->w-30-188-16-14)/cols;
    int x=w->x+15+188+9+6+pitch/2,y=w->y+60+109+65;
    q_click(&selection_ui,x,y);
    CHECK(selection_ui.active==Q_FILES&&selection_ui.file_selected==0);
    CHECK(!strcmp(selection_ui.file_path,"\\"));
    q_tick_timed(&selection_ui,200000);
    q_click(&selection_ui,x,y);
    CHECK(!strcmp(selection_ui.file_path,"\\FolderA"));
    CHECK(selection_ui.file_selected==-1&&selection_ui.file_click_index==-1);
    q_key(&selection_ui,8,0);
    CHECK(!strcmp(selection_ui.file_path,"\\")&&selection_ui.file_count==3);
    q_click(&selection_ui,x+pitch,y);
    CHECK(selection_ui.file_selected==1&&selection_ui.active==Q_FILES);
    q_tick_timed(&selection_ui,450000);
    q_click(&selection_ui,x+pitch,y);
    CHECK(selection_ui.file_selected==1&&selection_ui.active==Q_FILES);
    q_tick_timed(&selection_ui,200000);
    q_click(&selection_ui,x+pitch*2,y);
    CHECK(selection_ui.file_selected==2&&selection_ui.active==Q_FILES);
    q_click(&selection_ui,x+pitch,y);
    CHECK(selection_ui.file_selected==1&&selection_ui.active==Q_FILES);
    q_tick_timed(&selection_ui,100000);
    q_click(&selection_ui,x+pitch,y);
    CHECK(selection_ui.active==Q_NOTES&&selection_ui.note_length==5);
    CHECK(!memcmp(selection_ui.notes,"first",5));
    q_open(&selection_ui,Q_FILES);
    q_click(&selection_ui,x+pitch*2,y);
    CHECK(selection_ui.file_selected==2&&selection_ui.active==Q_FILES);
    q_key(&selection_ui,'\r',0);
    CHECK(selection_ui.active==Q_NOTES&&selection_ui.note_length==6);
    CHECK(!memcmp(selection_ui.notes,"second",6));
    q_key(&selection_ui,0,4);
    q_key(&selection_ui,'X',0);
    CHECK(!strcmp(selection_ui.notes,"seconXd")&&selection_ui.note_dirty);
    q_key(&selection_ui,8,0);
    CHECK(!strcmp(selection_ui.notes,"second"));
    q_key(&selection_ui,19,0);
    CHECK(!selection_ui.note_dirty);
    q_open(&selection_ui,Q_FILES);
    QWindow *fw=&selection_ui.windows[Q_FILES];
    int ox=fw->x+15,oy=fw->y+60;
    q_click(&selection_ui,ox+500+20,oy+66);
    CHECK(selection_ui.file_menu==3);
    q_click(&selection_ui,ox+500+24,oy+94+6+5*35+16);
    CHECK(selection_ui.file_view==5&&selection_ui.file_menu==0);
    q_click(&selection_ui,ox+407+18,oy+66);
    CHECK(selection_ui.file_menu==2);
    q_click(&selection_ui,ox+407+18,oy+94+6+2*35+16);
    CHECK(selection_ui.file_sort==2);
    q_click(&selection_ui,ox+5+18,oy+66);
    CHECK(selection_ui.file_menu==1);
    q_click(&selection_ui,ox+5+18,oy+94+6+16);
    CHECK(selection_ui.file_operation==1);
    const char *newfolder="New Folder";
    for(int i=0;newfolder[i];i++)q_key(&selection_ui,newfolder[i],0);
    q_key(&selection_ui,'\r',0);
    CHECK(selection_ui.file_operation==0&&selection_ui.file_count==4);
    int exists=0;
    for(int i=0;i<selection_ui.file_count;i++)
        if(!strcmp(selection_ui.files[i].name,"New Folder"))exists=1;
    CHECK(exists);
    q_click(&selection_ui,ox+full_search_x_for_test(fw->w),oy+21);
    CHECK(selection_ui.file_search_active);
    const char *search="alpha";
    for(int i=0;search[i];i++)q_key(&selection_ui,search[i],0);
    CHECK(selection_ui.file_visible_count==1);
    q_key(&selection_ui,8,0);
    CHECK(selection_ui.file_visible_count==1);
    q_key(&selection_ui,27,0);
    CHECK(!selection_ui.file_search_active);
    q_open(&selection_ui,Q_TERMINAL);
    const char *cmd="mkdir Projects";
    for(int i=0;cmd[i];i++)q_key(&selection_ui,cmd[i],0);
    q_key(&selection_ui,'\r',0);
    CHECK(!strcmp(selection_ui.terminal_lines[6],"Folder created"));
    cmd="cd Projects";
    for(int i=0;cmd[i];i++)q_key(&selection_ui,cmd[i],0);
    q_key(&selection_ui,'\r',0);
    CHECK(!strcmp(selection_ui.terminal_path,"\\Projects"));
    cmd="touch draft.txt";
    for(int i=0;cmd[i];i++)q_key(&selection_ui,cmd[i],0);
    q_key(&selection_ui,'\r',0);
    CHECK(!strcmp(selection_ui.terminal_lines[6],"File created"));
    char text_data[32];size_t text_len=0;
    CHECK(qfs_read(&selection_storage,0,"\\Projects\\draft.txt",text_data,sizeof(text_data),&text_len)==0&&text_len==0);
}


static QStorage move_storage;
static QDesktop move_ui;
static void select_named(const char *name){
    move_ui.file_selected=-1;
    move_ui.file_checked_count=0;
    for(int i=0;i<move_ui.file_count;i++){
        int found=!strcmp(move_ui.files[i].name,name);
        move_ui.file_checked[i]=(uint8_t)found;
        if(found){move_ui.file_selected=i;move_ui.file_checked_count=1;}
    }
    CHECK(move_ui.file_selected>=0);
}
static void select_file_path(const char *path){
    move_ui.file_address_active=1;
    move_ui.file_address_length=(int)strlen(path);
    strcpy(move_ui.file_address,path);
    q_key(&move_ui,'\r',0);
    CHECK(!strcmp(move_ui.file_path,path));
}
static void check_file_moves(void){
    qfs_init(&move_storage,0,0);
    CHECK(qfs_mkdir(&move_storage,0,"\\Source")==0);
    CHECK(qfs_write(&move_storage,0,"\\Source\\a.txt","one",3)==0);
    CHECK(qfs_write(&move_storage,0,"\\Source\\b.txt","two",3)==0);
    CHECK(qfs_mkdir(&move_storage,0,"\\Destination")==0);
    q_init(&move_ui,1024,768,selection_pixels,selection_wall,selection_workspace);
    q_attach_storage(&move_ui,&move_storage,0,0);
    q_open(&move_ui,Q_FILES);
    select_named("Source");
    q_key(&move_ui,24,0);
    CHECK(move_ui.file_clip_count==1&&move_ui.file_clip_cut);
    q_key(&move_ui,22,0);
    CHECK(!strcmp(move_ui.file_status,"Move complete"));
    CHECK(move_ui.file_clip_count==0);
    QFsEntry items[QFS_ENTRIES];
    CHECK(qfs_list(&move_storage,0,"\\Source",items,QFS_ENTRIES)==2);
    select_file_path("\\Destination");
    q_key(&move_ui,22,0);
    CHECK(!strcmp(move_ui.file_status,"Clipboard is empty"));
    select_file_path("\\");
    select_named("Source");
    q_key(&move_ui,24,0);
    select_file_path("\\Destination");
    q_key(&move_ui,22,0);
    CHECK(!strcmp(move_ui.file_status,"Move complete"));
    char buffer[32];size_t length=0;
    CHECK(qfs_read(&move_storage,0,"\\Destination\\Source\\a.txt",buffer,sizeof(buffer),&length)==0);
    CHECK(length==3&&!memcmp(buffer,"one",3));
    CHECK(qfs_read(&move_storage,0,"\\Destination\\Source\\b.txt",buffer,sizeof(buffer),&length)==0);
    CHECK(length==3&&!memcmp(buffer,"two",3));
    CHECK(qfs_list(&move_storage,0,"\\Source",items,QFS_ENTRIES)<0);
    select_named("Source");
    q_key(&move_ui,3,0);
    q_key(&move_ui,22,0);
    CHECK(qfs_list(&move_storage,0,"\\Destination\\Copy 1 - Source",items,QFS_ENTRIES)==2);
    qfs_init(&move_storage,0,0);
    CHECK(qfs_mkdir(&move_storage,0,"\\Batch")==0);
    CHECK(qfs_write(&move_storage,0,"\\Batch\\a.txt","alpha",5)==0);
    CHECK(qfs_write(&move_storage,0,"\\Batch\\b.txt","beta",4)==0);
    CHECK(qfs_mkdir(&move_storage,0,"\\Target")==0);
    for(int i=0;i<19;i++){
        char name[16];
        snprintf(name,sizeof(name),"\\filler%02d",i);
        CHECK(qfs_write(&move_storage,0,name,"x",1)==0);
    }
    q_init(&move_ui,1024,768,selection_pixels,selection_wall,selection_workspace);
    q_attach_storage(&move_ui,&move_storage,0,0);
    q_open(&move_ui,Q_FILES);
    select_named("Batch");
    q_key(&move_ui,24,0);
    select_file_path("\\Target");
    q_key(&move_ui,22,0);
    CHECK(!strcmp(move_ui.file_status,"Some items could not be copied or moved"));
    CHECK(move_ui.file_clip_count==1);
    CHECK(qfs_list(&move_storage,0,"\\Target",items,QFS_ENTRIES)==0);
    CHECK(qfs_read(&move_storage,0,"\\Batch\\a.txt",buffer,sizeof(buffer),&length)==0&&length==5);
    CHECK(qfs_read(&move_storage,0,"\\Batch\\b.txt",buffer,sizeof(buffer),&length)==0&&length==4);
}

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
    key('2');key('+');key('3');key('*');key('4');key('\r');
    CHECK(!strcmp(ui.answer,"14"));
    key('2');key('0');key('/');key('2');key('+');key('4');key('*');key('2');key('\r');
    CHECK(!strcmp(ui.answer,"18"));
    key('1');key('0');key('0');key('0');key('0');key('0');key('0');key('0');key('0');key('0');key('*');key('2');key('\r');
    CHECK(!strcmp(ui.answer,"OVERFLOW"));
    key('C');
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
    q_click(&ui,(1024-510)/2+26+116*1+47,768-607+112+50);
    CHECK(ui.active==Q_NOTES);
    q_open(&ui,Q_STORE);
    CHECK(ui.active==Q_STORE&&ui.windows[Q_STORE].open);
    q_draw(&ui);
    QWindow *store=&ui.windows[Q_STORE];
    q_click(&ui,store->x+32+87,store->y+60+78+51);
    CHECK(ui.active==Q_NOTES&&ui.windows[Q_NOTES].open);
    q_open(&ui,Q_STORE);
    q_draw(&ui);
    q_click(&ui,store->x+32+230+87,store->y+60+78+51);
    CHECK(ui.active==Q_CALCULATOR&&ui.windows[Q_CALCULATOR].open);
    check_file_selection();
    check_file_moves();
    puts("PASS: file selection, double-click navigation, app launcher, Store and window controls");
    return 0;
}
