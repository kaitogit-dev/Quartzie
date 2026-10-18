#define _GNU_SOURCE
#define CINMIX_STORAGE_TEST
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb = 0x2000000;
uint16_t boot_pitch = 7680;
uint16_t boot_width = 1920;
uint16_t boot_height = 1080;
uint8_t boot_bpp = 32;
#define PIXELS (1920u * 1080u)
#define check(c) do { if (!(c)) { fprintf(stderr,"FAIL Quartzie ui24 line %d: %s\n",__LINE__,#c); exit(1); } } while (0)
static FILE *disk;
int storage_test_read(uint32_t lba, uint8_t *buffer) {
    return fseek(disk,(long)lba*512,SEEK_SET)==0 && fread(buffer,1,512,disk)==512;
}
int storage_test_write(uint32_t lba, const uint8_t *buffer) {
    return fseek(disk,(long)lba*512,SEEK_SET)==0 && fwrite(buffer,1,512,disk)==512 && fflush(disk)==0;
}
static void frame(void) { repaint(); check(memcmp(canvas,(void *)(uintptr_t)boot_fb,PIXELS*4)==0); }
static void frames(int n) { while(n--) frame(); }
static void click_at(int x,int y) {
    mouse_x=x; mouse_y=y;
    handle_mouse_byte(9); handle_mouse_byte(0); handle_mouse_byte(0);
    handle_mouse_byte(8); handle_mouse_byte(0); handle_mouse_byte(0);
}
static void keys(int scancode) { handle_key(0x1d);handle_key((uint8_t)scancode);handle_key(0x9d); }
static void snap(const char *filename) {
    FILE *f=fopen(filename,"wb");check(f);
    fprintf(f,"P6\n1920 1080\n255\n");
    for(size_t i=0;i<PIXELS;i++) {
        uint32_t c=canvas[i]; fputc((c>>16)&255,f);fputc((c>>8)&255,f);fputc(c&255,f);
    }
    fclose(f);
}
int main(int argc,char **argv) {
    check(argc==2);
    disk=fopen(argv[1],"r+b");check(disk);
    check(mmap((void*)0x400000,PIXELS*4,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=MAP_FAILED);
    check(mmap((void*)0xD00000,PIXELS*4,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=MAP_FAILED);
    check(mmap((void*)0x2000000,PIXELS*4,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=MAP_FAILED);
    sw=1920; sh=1080; mouse_x=1780; mouse_y=850;
    for(int i=0;i<APPS;i++){windows[i].w=1050;windows[i].h=700;windows[i].x=(sw-1050)/2+14*(i%4);windows[i].y=(sh-700)/2-12+15*(i%4);}
    copy(clock_time,"17:30:00",9);copy(clock_date,"2026-10-18",11);
    check(storage_mount());check(NT_SUCCESS(NtInitializeIoManager()));files_restore();
    check(storage_slot("hello.exe")>=0);
    frame();
    click_at(60,55);
    check(desktop_focus==0 && !windows[FILES].opened && click_wait==CLICK_WINDOW_TICKS);
    click_at(62,56);
    check(windows[FILES].opened && desktop_focus<0 && icon_motion[FILES]==1);
    check(task_icon_scale[1]<256 && task_icon_scale[5]>256);
    for(int n=0;n<TASK_ICON_FRAMES+3;n++) { dirty=1; frame(); }
    check(icon_motion[FILES]==0);
    check(click_wait==0);
    click_at(50,55);
    check(desktop_focus==0 && !windows[NOTES].opened);
    click_wait=0;
    click_at(50,55);
    check(!windows[NOTES].opened);
    click_at(50,55);
    check(windows[FILES].opened);
    int x=files_list_left(windows[FILES].x+2,windows[FILES].w-4)+15;
    int y=files_row_top(windows[FILES].y+34)+2*files_row_height()+12;
    click_at(x,y);
    check(selected_file==2 && !windows[NOTES].opened);
    click_at(x+2,y+1);
    check(windows[NOTES].opened && active_window()==NOTES && icon_motion[NOTES]==1);
    copy(notes,"Quartzie clipboard",sizeof(notes));notes_len=size(notes);
    keys(0x1e);
    check(notes_select_all);
    keys(0x2e);
    check(clipboard_type==3 && clipboard_text_len==notes_len && same(clipboard_text,notes));
    keys(0x2d);
    check(notes_len==0 && clipboard_type==3);
    keys(0x2f);
    check(same(notes,"Quartzie clipboard") && notes_dirty);
    handle_key(0x38);handle_key(0x3e);handle_key(0xb8);
    check(!windows[NOTES].opened);
    check(active_window()==FILES);
    selected_file=FILE_COUNT;
    check(same(files_name(selected_file),"hello.exe"));
    keys(0x2e);
    check(clipboard_type==1 && same(clipboard_filename,"hello.exe"));
    keys(0x2f);
    check(storage_slot("Copy_hello.exe")>=0);
    char orig[4097], copied[4097];
    int l=storage_load("hello.exe",orig,sizeof(orig));
    check(l>0 && storage_load("Copy_hello.exe",copied,sizeof(copied))==l && memcmp(orig,copied,(size_t)l)==0);
    selected_file=FILE_COUNT;
    keys(0x2d);check(clipboard_type==2);
    keys(0x2f);check(clipboard_type==0 && storage_slot("hello.exe")>=0);
    close_window(FILES,1);check(icon_motion[FILES]==2);
    check(task_icon_down[3]>0);
    open_window(FILES);check(icon_motion[FILES]==3);
    check(task_icon_up[3]<0);
    handle_key(0xe0);handle_key(0x5b);
    check(start_open);
    handle_key(0xe0);handle_key(0xdb);
    handle_key(0xe0);handle_key(0x5b);
    check(!start_open);
    keys(0x12);check(windows[FILES].opened && active_window()==FILES);
    theme=3;dirty=1;frames(3);
    snap("build/quartzie-explorer-24.ppm");
    close_window(FILES,0);click_wait=0;dirty=1;frames(3);
    snap("build/quartzie-desktop-24.ppm");
    check(fclose(disk)==0);
    puts("PASS: Quartzie brand, real desktop/Explorer double-click, Ctrl+E/C/X/V, Notes text clipboard, persistent file copying, Win key, Alt+F4 and taskbar 3-mode bounce");
    return 0;
}
