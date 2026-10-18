#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/mman.h>
#include "../src/kernel.c"
uint32_t boot_fb = 0x2000000;
uint16_t boot_pitch = 7680;
uint16_t boot_width = 1920;
uint16_t boot_height = 1080;
uint8_t boot_bpp = 32;
#define PIXELS (1920u * 1080u)
#define CHECK(ok) do { if (!(ok)) { fprintf(stderr, "FAIL taskbar26 line %d: %s\n", __LINE__, #ok); exit(1); } } while (0)
static void alloc_buffer(uintptr_t address) {
    CHECK(mmap((void *)address, PIXELS * 4u, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != MAP_FAILED);
}
static void frame(void) {
    repaint();
    CHECK(memcmp(canvas, (void *)(uintptr_t)boot_fb, PIXELS * 4u) == 0);
}
static void click_at(int x, int y) {
    mouse_x = x;
    mouse_y = y;
    handle_mouse_byte(9);
    handle_mouse_byte(0);
    handle_mouse_byte(0);
    handle_mouse_byte(8);
    handle_mouse_byte(0);
    handle_mouse_byte(0);
}
static void snapshot(const char *file) {
    FILE *f = fopen(file, "wb"); CHECK(f != NULL);
    fprintf(f, "P6\n1920 1080\n255\n");
    for (size_t i = 0; i < PIXELS; i++) {
        uint32_t p = canvas[i];
        fputc((p >> 16) & 255, f); fputc((p >> 8) & 255, f); fputc(p & 255, f);
    }
    CHECK(fclose(f) == 0);
}
int main(void) {
    alloc_buffer(0x400000); alloc_buffer(0xD00000); alloc_buffer(0x2000000);
    sw = 1920; sh = 1080; mouse_x = 1700; mouse_y = 800;
    for (int i = 0; i < APPS; i++) {
        windows[i].x = (sw - 1050) / 2 + (i % 4) * 14;
        windows[i].y = (sh - 700) / 2 - 12 + (i % 4) * 15;
        windows[i].w = 1050; windows[i].h = 700;
    }
    frame();
    int apps[APPS], count = taskbar_items(apps);
    int x = taskbar_left(count) + 20;
    int y = sh - 28;
    CHECK(apps[0] == FILES);
    for (int i = 0; i < 24000; i++) {
        click_at(x, y);
        if ((i & 31) == 0) cm_pump(16, cm_ui_dispatch);
        if ((i & 4095) == 0) frame();
        CHECK(next_order <= 48);
        CHECK(cm_queue_size < CM_QUEUE_LIMIT);
        CHECK(!left_down);
    }
    CHECK(windows[FILES].opened && windows[FILES].minimized);
    click_at(x, y);
    CHECK(windows[FILES].opened && !windows[FILES].minimized);
    CHECK(icon_motion[FILES] == 3);
    for (int k = 0; k < 12; k++) frame();
    CHECK(icon_motion[FILES] == 0);
    int fixed_order = next_order;
    for (int i = 0; i < 15000; i++) {
        click_at(windows[FILES].x + 500, windows[FILES].y + 420);
        if ((i & 15) == 0) cm_pump(16, cm_ui_dispatch);
        if ((i & 8191) == 0) frame();
        CHECK(next_order == fixed_order);
    }
    cm_pump(CM_QUEUE_LIMIT, cm_ui_dispatch);
    CHECK(cm_queue_size == 0);
    next_order = INT_MAX - 1;
    open_window(NOTES);
    CHECK(next_order <= 48);
    CHECK(active_window() == NOTES);
    CHECK(windows[FILES].opened && !windows[FILES].minimized);
    frame();
    close_window(NOTES, 0);
    open_window(FILES);
    CHECK(active_window() == FILES);
    for (int i = 0; i < WINDOW_OPEN_STEPS + 2; i++) frame();
    snapshot("build/taskbar26-recovered.ppm");
    CHECK(cm_dropped == 0);
    printf("PASS: 24000 rapid taskbar minimize/restore clicks, 15000 window clicks, bounded stacking orders and hit testing, stale-handle protection, smooth icon transition recovery, full 1080p framebuffer\n");
    return 0;
}
