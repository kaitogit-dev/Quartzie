#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb = 0x2000000;
uint16_t boot_pitch = 5120;
uint16_t boot_width = 1280;
uint16_t boot_height = 720;
uint8_t boot_bpp = 32;
#define check(ok) do { if (!(ok)) { fprintf(stderr, "FAIL animations line %d: %s\n", __LINE__, #ok); exit(1); } } while (0)
static void frame(void) {
    repaint();
    uint32_t *display = (uint32_t *)(uintptr_t)boot_fb;
    for (size_t k = 0; k < (size_t)sw * sh; k++) check(canvas[k] == display[k]);
}
static void frames(int count) { for (int n = 0; n < count; n++) frame(); }
static void snap(const char *name) {
    FILE *f = fopen(name, "wb");
    check(f);
    fprintf(f,"P6\n%d %d\n255\n",sw,sh);
    for (int y = 0; y < sh; y++) for (int x = 0; x < sw; x++) {
        uint32_t c = canvas[(size_t)y * sw + x];
        fputc((c >> 16) & 255, f);
        fputc((c >> 8) & 255, f);
        fputc(c & 255, f);
    }
    fclose(f);
}
static uint32_t *clean_canvas;
static void assert_clean(void) {
    check(memcmp(canvas, clean_canvas, (size_t)sw * sh * sizeof(uint32_t)) == 0);
}
static void open_and_close(int *state, int panel, const char *preview) {
    *state = 1;
    dirty = 1;
    frames(4);
    check(panel_step[panel] == 4 && panel_goal[panel] == PANEL_STEPS);
    if (preview) snap(preview);
    *state = 0;
    dirty = 1;
    frame();
    check(panel_step[panel] == 3 && panel_goal[panel] == 0);
    *state = 1;
    dirty = 1;
    frame();
    check(panel_step[panel] == 4 && panel_goal[panel] == PANEL_STEPS);
    frames(PANEL_STEPS - 4);
    check(panel_step[panel] == PANEL_STEPS);
    *state = 0;
    dirty = 1;
    frames(PANEL_STEPS + 1);
    check(panel_step[panel] == 0 && !panel_visible(panel) && panel_goal[panel] == 0);
    assert_clean();
}
int main(void) {
    check(mmap((void *)0x400000, 1280*720*4, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0) != MAP_FAILED);
    check(mmap((void *)0xD00000, 1280*720*4, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0) != MAP_FAILED);
    check(mmap((void *)0x2000000, 1280*720*4, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0) != MAP_FAILED);
    sw=1280; sh=720;
    mouse_x=1080;mouse_y=610;
    for (int i = 0; i < APPS; i++) {
        windows[i].x = 260 + 14 * (i % 4);
        windows[i].y = 87 + 15 * (i % 4);
        windows[i].w=760;
        windows[i].h=485;
    }
    frame();
    clean_canvas=malloc((size_t)sw * sh * sizeof(uint32_t));
    check(clean_canvas);
    memcpy(clean_canvas,canvas,(size_t)sw * sh * sizeof(uint32_t));
    start_open = 1;
    dirty = 1;
    for (int i = 0; i < PANEL_STEPS + 2; i++) {
        char filename[48];
        frame();
        snprintf(filename, sizeof(filename), "build/tween-%02d.ppm", i);
        snap(filename);
    }
    start_open = 0;
    dirty = 1;
    for (int i = PANEL_STEPS + 2; i < 2 * (PANEL_STEPS + 2); i++) {
        char filename[48];
        frame();
        snprintf(filename, sizeof(filename), "build/tween-%02d.ppm", i);
        snap(filename);
    }
    check(!panel_visible(PANEL_START));
    assert_clean();
    uint32_t old_full=full_frames;
    open_and_close(&start_open, PANEL_START, "build/start-tween-132.ppm");
    open_and_close(&context_open, PANEL_CONTEXT, "build/context-tween-132.ppm");
    open_and_close(&quick_open, PANEL_QUICK, "build/quick-tween-132.ppm");
    open_and_close(&notification_open, PANEL_NOTIFICATIONS, "build/notifications-tween-132.ppm");
    open_and_close(&run_open, PANEL_RUN, "build/run-tween-132.ppm");
    check(full_frames == old_full);
    mouse_x = 37; mouse_y = sh - 27;
    handle_click();
    check(start_open);
    frame();
    check(panel_step[PANEL_START] == 1);
    handle_click();
    check(!start_open);
    frames(2);
    check(!panel_visible(PANEL_START));
    mouse_x = sw - 40; mouse_y = sh - 27;
    handle_click();
    check(quick_open);
    frame();
    mouse_x = 800; mouse_y = 550;
    handle_click();
    check(!quick_open);
    frames(2);
    check(!panel_visible(PANEL_QUICK));
    mouse_x = sw - 150; mouse_y = sh - 27;
    handle_click();
    check(notification_open);
    frame();
    mouse_x = 750; mouse_y = 500;
    handle_click();
    check(!notification_open);
    frames(2);
    check(!panel_visible(PANEL_NOTIFICATIONS));
    mouse_x = 720; mouse_y = 530;
    handle_right_click();
    check(context_open);
    frame();
    mouse_x = 600; mouse_y = 400;
    handle_click();
    check(!context_open);
    frames(2);
    check(!panel_visible(PANEL_CONTEXT));
    handle_key(0x1d); handle_key(0x13); handle_key(0x9d);
    check(run_open);
    frame();
    handle_key(0x01);
    check(!run_open);
    frames(2);
    check(!panel_visible(PANEL_RUN));
    open_window(FILES);
    open_window(NOTES);
    frames(9);
    start_or_cycle_switcher();
    frames(4);
    check(tab_active && panel_step[PANEL_SWITCHER] == 4);
    snap("build/switcher-tween-132.ppm");
    finish_switcher(0);
    check(!tab_active && tab_count == 2);
    frame();
    check(panel_step[PANEL_SWITCHER] == 3);
    frames(4);
    check(!panel_visible(PANEL_SWITCHER));
    task_hover = FILES;
    hover_visible = 1;
    frames(4);
    check(panel_step[PANEL_TOOLTIP] == 4);
    snap("build/tooltip-tween-132.ppm");
    task_hover = -1;
    hover_visible = 0;
    dirty = 1;
    frames(5);
    check(!panel_visible(PANEL_TOOLTIP));
    toggle_desktop();
    check(desktop_hidden);
    check(windows[FILES].minimized && windows[NOTES].minimized);
    check(anim_visible[FILES] && anim_exit[FILES] && anim_ticks[FILES] == WINDOW_CLOSE_STEPS);
    check(anim_visible[NOTES] && anim_exit[NOTES] && anim_ticks[NOTES] == WINDOW_CLOSE_STEPS);
    frames(8);
    check(!anim_visible[FILES] && !anim_visible[NOTES]);
    toggle_desktop();
    check(!windows[FILES].minimized && !windows[NOTES].minimized);
    check(anim_ticks[FILES] == 0 && !anim_exit[FILES]);
    frames(9);
    check(!anim_visible[FILES] && !anim_visible[NOTES]);
    printf("PASS: reversibly tweened Start/context/Quick/notifications/Run/Alt+Tab/tooltip panels, damage-only rendering, Show Desktop minimize and restore, framebuffer compositing\n");
    return 0;
}
