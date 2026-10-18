#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb = 0x2000000;
uint16_t boot_pitch = 5120;
uint16_t boot_width = 1280;
uint16_t boot_height = 720;
uint8_t boot_bpp = 32;
#define require(cond) do { if (!(cond)) { fprintf(stderr, "FAIL ui11 line %d: %s\n", __LINE__, #cond); exit(1); } } while (0)
static void snap(const char *filename) {
    FILE *f = fopen(filename, "wb");
    require(f != NULL);
    fprintf(f, "P6\n%d %d\n255\n", sw, sh);
    for (int y = 0; y < sh; y++) for (int x = 0; x < sw; x++) {
        uint32_t c = canvas[(size_t)y * sw + x];
        fputc((c >> 16) & 255, f);
        fputc((c >> 8) & 255, f);
        fputc(c & 255, f);
    }
    fclose(f);
}
static void frames(int count) { for (int i=0; i<count; i++) repaint(); }
static void click(int x, int y) { mouse_x=x; mouse_y=y; handle_click(); }
int main(void) {
    require(mmap((void *)0x400000, 1280*720*4, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED,-1,0) != MAP_FAILED);
    require(mmap((void *)0x900000, 1280*720*4, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED,-1,0) != MAP_FAILED);
    require(mmap((void *)0xD00000, 1280*720*4, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED,-1,0) != MAP_FAILED);
    require(mmap((void *)0x2000000, 1280*720*4, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED,-1,0) != MAP_FAILED);
    sw=1280; sh=720;
    copy(clock_time, "15:39:00", 9);
    copy(clock_date, "2026-10-14", 11);
    for (int i=0;i<APPS;i++) {
        windows[i].x=260+14*(i%4);windows[i].y=87+15*(i%4);
        windows[i].w=760;windows[i].h=485;
    }
    mouse_x=1050;mouse_y=610;
    notify("Cinmix 0.13 is ready");
    frames(2);
    require(desktop_regenerations == 1);
    require(cached_wallpaper == 0);
    uint32_t old_full=full_frames;
    uint32_t old_pointer=pointer_frames;
    handle_mouse_byte(0x08);
    handle_mouse_byte(10);
    handle_mouse_byte(0);
    require(pointer_only && !dirty);
    move_pointer_only();
    require(pointer_frames == old_pointer+1 && full_frames == old_full);
    require(!pointer_only && pointer_saved);
    require(desktop_regenerations == 1);
    click(sw-150,sh-27);
    require(notification_open && notification_unread == 0);
    frames(9);
    snap("build/notifications.ppm");
    click(notification_left()+notification_width()-35,notification_top()+15);
    require(notification_count == 0);
    require(notification_open);
    frames(2);
    snap("build/notifications-empty.ppm");
    click(640,620);
    require(!notification_open);
    click(sw-40,sh-27);
    require(quick_open);
    frames(9);
    snap("build/quick-11.ppm");
    int qx=sw-quick_width()-16;
    int qy=sh-60-quick_height();
    click(qx+92,qy+225);
    require(volume_level == 70 && !volume_muted);
    click(qx+170,qy+225);
    require(volume_muted);
    click(qx+170,qy+225);
    require(!volume_muted);
    click(qx+36,qy+225);
    require(volume_level == 60);
    frames(2);
    snap("build/volume.ppm");
    click(640,620);
    require(!quick_open);
    open_window(APPEAR);
    frames(9);
    click(windows[APPEAR].x+2+164+34+155+2*65+20,windows[APPEAR].y+34+370+10);
    require(accent_index == 2);
    frames(2);
    snap("build/accent.ppm");
    open_window(FILES);
    require(anim_ticks[FILES] == 0);
    frames(2);
    require(anim_ticks[FILES] == 0);
    snap("build/animation.ppm");
    frames(7);
    require(anim_ticks[FILES] == 0);
    close_window(FILES,1);
    require(windows[FILES].minimized && anim_visible[FILES] && anim_exit[FILES]);
    frames(3);
    require(anim_visible[FILES]);
    frames(5);
    require(!anim_visible[FILES]);
    open_window(FILES);
    frames(8);
    require(!windows[FILES].minimized);
    task_hover=FILES;
    mouse_x=taskbar_left(7)+20;
    mouse_y=sh-27;
    hover_delay = hover_delay_target();
    hover_visible = 1;
    frames(2);
    snap("build/preview.ppm");
    require(notification_count == 0);
    shadow_pixels=0;
    repaint();
    require(shadow_pixels < 200000);
    int before=desktop_regenerations;
    wallpaper=2;
    repaint();
    require(desktop_regenerations == before+1);
    repaint();
    require(desktop_regenerations == before+1);
    puts("PASS: cached wallpaper invalidation, cursor-only blits, notification open/read/clear, quick panel software volume controls, accent palette, tweened open/close/minimize, delayed taskbar live preview, perimeter shadows");
    return 0;
}
