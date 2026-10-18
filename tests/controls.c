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
#define check(ok) do { if (!(ok)) { fprintf(stderr, "FAIL controls line %d: %s\n", __LINE__, #ok); exit(1); } } while (0)
static void packet(int pressed) {
    handle_mouse_byte((uint8_t)(pressed ? 9 : 8));
    handle_mouse_byte(0);
    handle_mouse_byte(0);
}
static void click_at(int x, int y) {
    mouse_x = x;
    mouse_y = y;
    packet(1);
    packet(0);
}
static void control(int app, int distance_from_right) {
    int x, y, w, h;
    animated_window_rect(app, &x, &y, &w, &h);
    click_at(x + w - distance_from_right, y + 15);
}
static void frames(int count) { for (int i = 0; i < count; i++) repaint(); }
int main(void) {
    check(mmap((void *)0x400000, 1280*720*4, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0) != MAP_FAILED);
    check(mmap((void *)0xD00000, 1280*720*4, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0) != MAP_FAILED);
    check(mmap((void *)0x2000000, 1280*720*4, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0) != MAP_FAILED);
    sw=1280;sh=720;mouse_x=1100;mouse_y=600;
    for (int i=0;i<APPS;i++) {windows[i].x=260+14*(i%4);windows[i].y=87+15*(i%4);windows[i].w=760;windows[i].h=485;}
    repaint();
    uint32_t full_before=full_frames;
    open_window(FILES);
    repaint();
    check(windows[FILES].opened && anim_ticks[FILES] == 0);
    check(full_frames == full_before + 1);
    control(FILES,18);
    check(!windows[FILES].opened && anim_exit[FILES] && anim_ticks[FILES] == WINDOW_CLOSE_STEPS);
    frames(8);
    check(!anim_visible[FILES]);
    open_window(FILES);
    frames(8);
    check(windows[FILES].opened && !windows[FILES].minimized);
    control(FILES,78);
    check(windows[FILES].minimized && windows[FILES].opened && anim_exit[FILES]);
    open_window(FILES);
    check(!windows[FILES].minimized && !anim_exit[FILES]);
    frames(8);
    control(FILES,48);
    check(windows[FILES].maximized);
    frames(8);
    control(FILES,48);
    check(!windows[FILES].maximized);
    frames(8);
    control(FILES,18);
    check(!windows[FILES].opened);
    frames(8);
    check(!anim_visible[FILES] && !windows[FILES].opened);
    timer_counter = 50000;
    timer_accum = 0;
    timer_unchanged = 0;
    check(!advance_frame_counter(42000));
    check(advance_frame_counter(30000));
    check(timer_accum < 9943);
    timer_counter = 100;
    timer_accum = 0;
    check(!advance_frame_counter(65000));
    timer_counter = 1234;
    timer_accum = 0;
    timer_unchanged = 0;
    for (int i = 0; i < 2047; i++) check(!advance_frame_counter(1234));
    check(advance_frame_counter(1234));
    printf("PASS: PS/2 mouse click closes during tween, minimizes, restores, maximizes and closes; partial animation redraws; PIT counter and static-clock fallback\n");
    return 0;
}
