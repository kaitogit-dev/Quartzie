#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb = 0x2000000;
uint16_t boot_pitch = 7680;
uint16_t boot_width = 1920;
uint16_t boot_height = 1080;
uint8_t boot_bpp = 32;
#define require(ok) do { if (!(ok)) { fprintf(stderr, "FAIL fullhd line %d: %s\n", __LINE__, #ok); exit(1); } } while (0)
#define PIXELS (1920u * 1080u)
static void map_buffer(uintptr_t location) {
    void *value = mmap((void *)location, PIXELS * 4u, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    require(value != MAP_FAILED && (uintptr_t)value == location);
}
static void frame(void) {
    repaint();
    uint32_t *physical = (uint32_t *)(uintptr_t)boot_fb;
    require(memcmp(canvas, physical, PIXELS * 4u) == 0);
}
static void screenshot(const char *path) {
    FILE *f = fopen(path, "wb"); require(f);
    fprintf(f, "P6\n1920 1080\n255\n");
    for (size_t i = 0; i < PIXELS; i++) {
        uint32_t pixel = canvas[i];
        fputc((pixel >> 16) & 255, f);
        fputc((pixel >> 8) & 255, f);
        fputc(pixel & 255, f);
    }
    fclose(f);
}
int main(void) {
    map_buffer(0x400000); map_buffer(0xD00000); map_buffer(0x2000000);
    sw = boot_width; sh = boot_height;
    mouse_x = 1450; mouse_y = 800;
    for (int i = 0; i < APPS; i++) {
        windows[i].w = 1050;
        windows[i].h = 700;
        windows[i].x = (sw - windows[i].w) / 2 + 14 * (i % 4);
        windows[i].y = (sh - windows[i].h) / 2 - 12 + 15 * (i % 4);
    }
    copy(clock_time, "20:43:00", 9);
    copy(clock_date, "2026-10-14", 11);
    frame();
    require(sw == 1920 && sh == 1080 && boot_pitch == sw * 4);
    require(full_frames == 1);
    screenshot("build/fullhd-desktop.ppm");
    uint32_t full = full_frames;
    open_window(FILES);
    require(anim_ticks[FILES] == 0);
    for (int i = 0; i < WINDOW_OPEN_STEPS + 1; i++) frame();
    require(full_frames == full + 1 && damage_frames >= WINDOW_OPEN_STEPS);
    require(windows[FILES].opened && !windows[FILES].minimized);
    screenshot("build/fullhd-files.ppm");
    mouse_x = windows[FILES].x + windows[FILES].w - 77;
    mouse_y = windows[FILES].y + 16;
    handle_mouse_byte(9); handle_mouse_byte(0); handle_mouse_byte(0);
    handle_mouse_byte(8); handle_mouse_byte(0); handle_mouse_byte(0);
    require(windows[FILES].minimized && windows[FILES].opened);
    for (int i = 0; i < WINDOW_CLOSE_STEPS + 1; i++) frame();
    open_window(FILES);
    for (int i = 0; i < WINDOW_OPEN_STEPS + 1; i++) frame();
    require(!windows[FILES].minimized);
    start_open = 1;
    dirty = 1;
    for (int i = 0; i < PANEL_STEPS + 1; i++) frame();
    screenshot("build/fullhd-start.ppm");
    start_open = 0;
    dirty = 1;
    for (int i = 0; i < PANEL_STEPS + 1; i++) frame();
    require(panel_step[PANEL_START] == 0);
    timer_counter = 50000;
    timer_accum = 0;
    require(!advance_frame_counter(45000));
    require(advance_frame_counter(39500));
    require(hover_delay_target() == 120);
    printf("PASS: true 1920x1080 framebuffer and 7680-byte pitch, full HD window mouse controls, 120Hz animation pacing, partial-window redraws, popup close cleanup\n");
    return 0;
}
