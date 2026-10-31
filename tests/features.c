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


static QNtKernel tasks_kernel;
static QDesktop tasks_ui;
static uint32_t tasks_pixels[1024*768],tasks_wall[1024*768],tasks_scratch[1024*768];
static int tasks_terminated;
static int terminate_test_process(QNtKernel *kernel,uint32_t pid){
    tasks_terminated++;
    for(int i=0;i<QNT_PROCESSES;i++)if(kernel->processes[i].alive&&kernel->processes[i].pid==pid){
        kernel->processes[i].alive=0;
        return QNT_OK;
    }
    return QNT_NOT_FOUND;
}
static void check_task_manager(void){
    memset(&tasks_kernel,0,sizeof(tasks_kernel));
    tasks_kernel.processes[0].alive=1;tasks_kernel.processes[0].pid=42;
    tasks_kernel.processes[0].region_count=1;tasks_kernel.processes[0].regions[0].length=1048576;
    q_init(&tasks_ui,1024,768,tasks_pixels,tasks_wall,tasks_scratch);
    q_attach_kernel(&tasks_ui,&tasks_kernel,terminate_test_process);
    q_open(&tasks_ui,Q_NOTES);q_open(&tasks_ui,Q_FILES);q_open(&tasks_ui,Q_TASKS);
    CHECK(tasks_ui.windows[Q_TASKS].open);
    tasks_ui.task_tab=0;tasks_ui.task_selected=Q_NOTES;
    CHECK(q_task_end_selected(&tasks_ui)==1&&!tasks_ui.windows[Q_NOTES].open);
    CHECK(q_task_end_selected(&tasks_ui)==0);
    tasks_ui.task_tab=1;tasks_ui.task_selected=0;
    CHECK(q_task_end_selected(&tasks_ui)==1&&tasks_terminated==1&&!tasks_kernel.processes[0].alive);
    CHECK(q_task_end_selected(&tasks_ui)==0);
    q_tick_timed(&tasks_ui,1000000);
    CHECK(tasks_ui.uptime_us==1000000u);
    q_open(&tasks_ui,Q_TASKS);
    QWindow *window=&tasks_ui.windows[Q_TASKS];
    int x=window->x+32,y=window->y+60;
    q_click(&tasks_ui,x+145*2+25,y+35);
    CHECK(tasks_ui.task_tab==2);
    q_click(&tasks_ui,x+25,y+35);
    CHECK(tasks_ui.task_tab==0);
}


static QDesktop store_ui;
static QStorage store_storage;
static uint32_t store_pixels[1024*768],store_wall[1024*768],store_blur[1024*768];
static uint8_t store_archive[4096];
static void check_store_import(void){
    FILE *file=fopen("build/store-test.qapp","rb");
    CHECK(file);
    size_t bytes=fread(store_archive,1,sizeof(store_archive),file);
    fclose(file);
    CHECK(bytes>300&&bytes<sizeof(store_archive));
    qfs_init(&store_storage,0,0);
    CHECK(qfs_write(&store_storage,0,"\\Store.qapp",store_archive,bytes)==0);
    q_init(&store_ui,1024,768,store_pixels,store_wall,store_blur);
    q_attach_storage(&store_ui,&store_storage,store_archive,sizeof(store_archive));
    q_open(&store_ui,Q_FILES);
    CHECK(store_ui.file_count==1);
    store_ui.file_selected=0;
    q_key(&store_ui,'\r',0);
    CHECK(store_ui.active==Q_STORE&&store_ui.store_pending==1);
    CHECK(!strcmp(store_ui.store_info.name,"Quartzie Store Test"));
    CHECK(!strcmp(store_ui.store_info.id,"quartzie.storetest"));
    QWindow *w=&store_ui.windows[Q_STORE];
    int x=w->x+32,y=w->y+60;
    q_click(&store_ui,x+50,y+265);
    CHECK(store_ui.store_pending==1);
    CHECK(!strcmp(store_ui.store_status,"Attach persistent Home storage to install"));
    q_click(&store_ui,x+193,y+265);
    CHECK(!store_ui.store_pending);
    CHECK(!strcmp(store_ui.store_status,"Installation cancelled"));
    store_archive[bytes/2]^=0x20;
    CHECK(qfs_write(&store_storage,0,"\\Bad.qapp",store_archive,bytes)==0);
    q_attach_storage(&store_ui,&store_storage,store_archive,sizeof(store_archive));
    q_open(&store_ui,Q_FILES);
    int bad=-1;
    for(int i=0;i<store_ui.file_count;i++)if(!strcmp(store_ui.files[i].name,"Bad.qapp"))bad=i;
    CHECK(bad>=0);
    store_ui.file_selected=bad;
    q_key(&store_ui,'\r',0);
    CHECK(!store_ui.store_pending);
    CHECK(!strcmp(store_ui.file_status,"Invalid, corrupt or oversized QApp package"));
}


static QDesktop media_ui;
static QStorage media_storage;
static uint32_t media_pixels[1024*768],media_wall[1024*768],media_blur[1024*768];
static uint8_t media_payload[4096],media_export[4096];
static void test_write32(uint8_t *b,int n,uint32_t v){
    for(int i=0;i<4;i++)b[n+i]=(uint8_t)(v>>(8*i));
}
static void check_media_actions(void){
    qfs_init(&media_storage,0,0);
    CHECK(qfs_mkdir(&media_storage,0,"\\Pictures")==0);
    uint8_t bmp[70]={0};
    bmp[0]='B';bmp[1]='M';
    test_write32(bmp,10,54);test_write32(bmp,14,40);test_write32(bmp,18,2);test_write32(bmp,22,2);
    bmp[26]=1;bmp[28]=24;
    bmp[54]=0;bmp[55]=0;bmp[56]=255;
    bmp[62]=255;bmp[63]=0;bmp[64]=0;
    CHECK(qfs_write(&media_storage,0,"\\Pictures\\First.bmp",bmp,sizeof(bmp))==0);
    CHECK(qfs_write(&media_storage,0,"\\Pictures\\Second.bmp",bmp,sizeof(bmp))==0);
    q_init(&media_ui,1024,768,media_pixels,media_wall,media_blur);
    q_attach_storage(&media_ui,&media_storage,media_payload,sizeof(media_payload));
    q_attach_photo_output(&media_ui,media_export,sizeof(media_export));
    q_open(&media_ui,Q_FILES);
    media_ui.file_selected=0;
    q_key(&media_ui,'\r',0);
    CHECK(!strcmp(media_ui.file_path,"\\Pictures"));
    media_ui.file_selected=0;
    q_key(&media_ui,'\r',0);
    CHECK(media_ui.active==Q_PHOTOS&&media_ui.media_track_count==2);
    CHECK(media_ui.media_track_index==0);
    QWindow *w=&media_ui.windows[Q_PHOTOS];
    int left=w->x+32,top=w->y+60,view=w->h-223;
    q_click(&media_ui,left+15,top+104+view+18);
    CHECK(media_ui.photo_rotation==1);
    q_click(&media_ui,left+101,top+104+view+18);
    CHECK(media_ui.photo_flipped==1);
    q_click(&media_ui,left+184,top+104+view+18);
    char data[128];size_t length=0;
    CHECK(qfs_read(&media_storage,0,"\\Pictures\\Edited-001.bmp",data,sizeof(data),&length)==0&&length==70);
    QMedia sample;
    CHECK(qm_open(&sample,data,length)==0);
    CHECK(qm_pixel(&sample,0,0)==qm_image_pixel(&media_ui.media,1,1,0,0));
    q_click(&media_ui,left+395,top+104+view+18);
    CHECK(media_ui.media_track_index==1);
    q_click(&media_ui,left+306,top+104+view+18);
    CHECK(media_ui.media_track_index==0);
    uint8_t wav[52]={0};
    memcpy(wav,"RIFF",4);memcpy(wav+8,"WAVEfmt ",8);
    test_write32(wav,16,16);wav[20]=1;wav[22]=1;
    test_write32(wav,24,8000);test_write32(wav,28,8000);
    wav[32]=1;wav[34]=8;
    memcpy(wav+36,"data",4);test_write32(wav,40,8);
    for(int i=0;i<8;i++)wav[44+i]=(uint8_t)(i*32);
    CHECK(qfs_write(&media_storage,0,"\\Pictures\\Song1.wav",wav,sizeof(wav))==0);
    CHECK(qfs_write(&media_storage,0,"\\Pictures\\Song2.wav",wav,sizeof(wav))==0);
    q_open(&media_ui,Q_FILES);
    q_key(&media_ui,8,0);
    media_ui.file_selected=0;
    q_key(&media_ui,'\r',0);
    CHECK(!strcmp(media_ui.file_path,"\\Pictures"));
    int selected=-1;
    for(int i=0;i<media_ui.file_count;i++)if(!strcmp(media_ui.files[i].name,"Song1.wav"))selected=i;
    CHECK(selected>=0);
    media_ui.file_selected=selected;q_key(&media_ui,'\r',0);
    CHECK(media_ui.active==Q_MEDIA&&media_ui.media.kind==QM_AUDIO);
    CHECK(media_ui.media_track_count==2);
    w=&media_ui.windows[Q_MEDIA];
    left=w->x+32;top=w->y+60;view=w->h-223;
    q_click(&media_ui,left+99,top+104+view+18);
    CHECK(media_ui.media_track_index==1&&media_ui.media.playing);
    q_click(&media_ui,left+190,top+104+view+18);
    CHECK(media_ui.media_repeat);
    q_click(&media_ui,left+255,top+104+view+18);
    CHECK(media_ui.media_volume_level==70);
    q_click(&media_ui,left+390,top+104+view+18);
    CHECK(media_ui.media_muted);
    q_tick_timed(&media_ui,3000);
    CHECK(media_ui.media.playing&&media_ui.media_track_index==0);
}
static void check_network_ui(void){
    static QNetwork network;
    static QDesktop desktop;
    static uint32_t pixels[1024*768],wall[1024*768],blur[1024*768];
    q_init(&desktop,1024,768,pixels,wall,blur);
    network.count=1;
    strcpy(network.adapters[0].mac,"52:54:00:12:AB:FE");
    network.adapters[0].link_known=1;
    network.adapters[0].link_present=1;
    q_attach_network(&desktop,&network);
    q_open(&desktop,Q_SETTINGS);
    QWindow *w=&desktop.windows[Q_SETTINGS];
    q_click(&desktop,w->x+42,w->y+60+128+6*43+15);
    CHECK(desktop.settings_page==6);
    q_open(&desktop,Q_TERMINAL);
    const char *command="netinfo";
    for(int i=0;command[i];i++)q_key(&desktop,command[i],0);
    q_key(&desktop,'\r',0);
    CHECK(!strcmp(desktop.terminal_lines[5],"52:54:00:12:AB:FE"));
    CHECK(!strcmp(desktop.terminal_lines[6],"Link connected"));
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
    check_task_manager();
    check_store_import();
    check_media_actions();
    check_network_ui();
    puts("PASS: file selection, double-click navigation, app launcher, Store and window controls");
    return 0;
}
