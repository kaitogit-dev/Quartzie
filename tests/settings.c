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

static void check_saved_preferences(void){
    static QStorage preferences;
    static QDesktop first,second;
    qfs_init(&preferences,0,0);
    q_init(&first,1280,720,pixels,wallpaper,workspace);
    q_attach_storage(&first,&preferences,0,0);
    q_enable_animations(&first);
    q_set_theme(&first,1);
    q_set_accent(&first,3);
    first.animations_enabled=0;
    CHECK(q_save_preferences(&first)==0);
    q_init(&second,1280,720,pixels,wallpaper,workspace);
    q_attach_storage(&second,&preferences,0,0);
    q_enable_animations(&second);
    CHECK(second.settings_loaded&&second.theme==1&&second.accent==3&&!second.animations_enabled);
    uint8_t bad[10]={0};
    CHECK(qfs_write(&preferences,preferences.home_index,"\\QUARTZIE.CFG",bad,sizeof(bad))==0);
    q_init(&second,1280,720,pixels,wallpaper,workspace);
    q_attach_storage(&second,&preferences,0,0);
    q_enable_animations(&second);
    CHECK(!second.settings_loaded&&!second.theme&&second.animations_enabled);
}


static QDesktop profile_ui;
static QStorage profile_storage;
static uint32_t profile_pixels[800*600],profile_wall[800*600],profile_blur[800*600];
static uint8_t profile_buffer[65536];
static void check_profile_wallpaper_ui(void){
    qfs_init(&profile_storage,0,0);
    CHECK(qprofiles_create(&profile_storage,"Mira")==0);
    q_init(&profile_ui,800,600,profile_pixels,profile_wall,profile_blur);
    q_attach_storage(&profile_ui,&profile_storage,profile_buffer,sizeof(profile_buffer));
    q_lock(&profile_ui);
    CHECK(profile_ui.locked&&profile_ui.profile_count==2);
    int mira=-1;
    for(int i=0;i<profile_ui.profile_count;i++)if(!strcmp(profile_ui.profiles[i].name,"Mira"))mira=i;
    CHECK(mira>=0);
    profile_ui.profile_target=mira;
    q_key(&profile_ui,'\r',0);
    CHECK(!profile_ui.locked&&!strcmp(profile_storage.profile,"Mira"));
    CHECK(!strcmp(profile_ui.file_path,"\\Users\\Mira"));
    CHECK(qprofiles_set_pin(&profile_storage,"2468")==0);
    CHECK(q_wallpaper_mode(&profile_ui,1)==0&&profile_ui.wallpaper_mode==1);
    q_lock(&profile_ui);
    CHECK(profile_ui.locked);
    q_key(&profile_ui,'1',0);q_key(&profile_ui,'2',0);q_key(&profile_ui,'3',0);q_key(&profile_ui,'4',0);
    q_key(&profile_ui,'\r',0);
    CHECK(profile_ui.locked);
    for(int i=0;i<4;i++)q_key(&profile_ui,8,0);
    q_key(&profile_ui,'2',0);q_key(&profile_ui,'4',0);q_key(&profile_ui,'6',0);q_key(&profile_ui,'8',0);
    q_key(&profile_ui,'\r',0);
    CHECK(!profile_ui.locked);
    QDesktop reboot;
    static uint32_t pixels2[800*600],wall2[800*600],blur2[800*600];
    q_init(&reboot,800,600,pixels2,wall2,blur2);
    q_attach_storage(&reboot,&profile_storage,profile_buffer,sizeof(profile_buffer));
    CHECK(reboot.wallpaper_mode==1&&reboot.settings_loaded==1&&reboot.locked);
    q_key(&reboot,'2',0);q_key(&reboot,'4',0);q_key(&reboot,'6',0);q_key(&reboot,'8',0);
    q_key(&reboot,'\r',0);
    CHECK(!reboot.locked);
    uint8_t bmp[70]={0};
    bmp[0]='B';bmp[1]='M';bmp[10]=54;bmp[14]=40;bmp[18]=2;bmp[22]=2;bmp[26]=1;bmp[28]=24;
    bmp[54]=255;bmp[55]=0;bmp[56]=0;
    CHECK(qm_open(&reboot.media,bmp,sizeof(bmp))==0);
    CHECK(q_wallpaper_photo(&reboot)==0&&reboot.wallpaper_mode==3);
    CHECK(q_wallpaper_mode(&reboot,0)==0);
    CHECK(q_wallpaper_mode(&reboot,3)==0);
    CHECK(reboot.wallpaper_mode==3);
    size_t count=0;uint8_t saved[80];
    CHECK(qfs_read(&profile_storage,0,"\\Users\\Mira\\WALLPAPER.QIMG",saved,sizeof(saved),&count)==0&&count==70);
}


static QDesktop compact_ui;
static QStorage compact_storage;
static uint32_t compact_pixels[800*600],compact_wall[800*600],compact_blur[800*600];
static void check_accounts_and_recovery_navigation(void){
    qfs_init(&compact_storage,0,0);
    q_init(&compact_ui,800,600,compact_pixels,compact_wall,compact_blur);
    q_attach_storage(&compact_ui,&compact_storage,0,0);
    q_open(&compact_ui,Q_SETTINGS);
    QWindow *w=&compact_ui.windows[Q_SETTINGS];
    int bx=w->x+32,by=w->y+60;
    int side=(w->w-64)/4;
    if(side<175)side=175;
    if(side>220)side=220;
    int left=bx+side+25;
    q_click(&compact_ui,bx+40,by+128+7*30+15);
    CHECK(compact_ui.settings_page==7);
    q_click(&compact_ui,left+30,by+322+15);
    CHECK(compact_ui.profile_edit==1);
    const char *name="Kite";
    for(int i=0;name[i];i++)q_key(&compact_ui,name[i],0);
    q_key(&compact_ui,'\r',0);
    CHECK(compact_ui.profile_edit==0);
    QProfile entries[QPROFILE_LIMIT];
    CHECK(qprofiles_list(&compact_storage,entries,QPROFILE_LIMIT)==1);
    CHECK(!strcmp(entries[0].name,"Kite"));
    q_click(&compact_ui,bx+40,by+128+8*30+15);
    CHECK(compact_ui.settings_page==8);
    int row=(w->h-175)/6;
    q_click(&compact_ui,left+80,by+92+2*row+20);
    CHECK(compact_ui.active==Q_TERMINAL);
    CHECK(!strcmp(compact_ui.terminal_lines[6],"Desktop renderer: running"));
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
    CHECK(w->w==1060&&w->h==616);
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
    q_click(&ui,content+width*5/8,by+300);
    CHECK(ui.accent==2);
    q_draw(&ui);
    q_click(&ui,content+width*7/8,by+350);
    CHECK(ui.accent==7);
    q_click(&ui,content+width*5/8,by+300);
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
    check_saved_preferences();
    check_profile_wallpaper_ui();
    check_accounts_and_recovery_navigation();
    return 0;
}
