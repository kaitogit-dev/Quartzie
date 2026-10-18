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
#define check(ok) do { if (!(ok)) { fprintf(stderr, "FAIL ui133 line %d: %s\n", __LINE__, #ok); exit(1); } } while (0)
static void snapshot(const char *path) {
    FILE *f = fopen(path, "wb"); check(f);
    fprintf(f, "P6\n%d %d\n255\n", sw, sh);
    for (int y = 0; y < sh; y++) for (int x = 0; x < sw; x++) {
        uint32_t c = canvas[(size_t)y * sw + x];
        fputc((c >> 16) & 255, f); fputc((c >> 8) & 255, f); fputc(c & 255, f);
    }
    fclose(f);
}
static uint32_t color_at(int x, int y) { return canvas[(size_t)y * sw + x]; }
int main(void) {
    check(mmap((void *)0x400000, 1280 * 720 * 4, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != MAP_FAILED);
    check(mmap((void *)0xD00000, 1280 * 720 * 4, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != MAP_FAILED);
    check(mmap((void *)0x2000000, 1280 * 720 * 4, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != MAP_FAILED);
    sw = 1280; sh = 720; mouse_x = 1070; mouse_y = 610;
    for (int i = 0; i < APPS; i++) {
        windows[i].x = 260 + 14 * (i % 4);
        windows[i].y = 87 + 15 * (i % 4);
        windows[i].w = 760; windows[i].h = 485;
    }
    repaint();
    int bar = taskbar_left(7);
    int y = sh - 27;
    int files_x = bar + 5;
    int notes_x = bar + taskbar_step() + 5;
    uint32_t files_plain = color_at(files_x, y);
    uint32_t notes_plain = color_at(notes_x, y);
    check(WINDOW_OPEN_STEPS == 2 && WINDOW_RESIZE_STEPS == 3);
    check(WINDOW_OPEN_STEPS < WINDOW_CLOSE_STEPS);
    open_window(FILES);
    check(anim_ticks[FILES] == WINDOW_OPEN_STEPS);
    int x, top, w, h;
    animated_window_rect(FILES, &x, &top, &w, &h);
    check(w > windows[FILES].w * 9 / 10 && w < windows[FILES].w);
    check(h > windows[FILES].h * 9 / 10 && h < windows[FILES].h);
    repaint();
    check(anim_ticks[FILES] == WINDOW_OPEN_STEPS - 1);
    check(color_at(files_x, y) != files_plain);
    check(color_at(notes_x, y) == notes_plain);
    for (int i = 0; i < WINDOW_OPEN_STEPS + 1; i++) repaint();
    animated_window_rect(FILES, &x, &top, &w, &h);
    check(anim_ticks[FILES] == 0);
    check(x == windows[FILES].x && w == windows[FILES].w);
    task_hover = NOTES;
    mouse_x = bar + taskbar_step() + taskbar_width() / 2;
    mouse_y = sh - 27;
    dirty = 1;
    repaint();
    check(color_at(notes_x, y) != notes_plain);
    check(color_at(files_x, y) != files_plain);
    snapshot("build/taskbar-highlight-133.ppm");
    task_hover = -1;
    dirty = 1;
    repaint();
    check(color_at(notes_x, y) == notes_plain);
    check(color_at(files_x, y) != files_plain);
    open_window(NOTES);
    check(anim_ticks[NOTES] == WINDOW_OPEN_STEPS);
    repaint();
    check(active_window() == NOTES);
    check(anim_ticks[NOTES] == WINDOW_OPEN_STEPS - 1);
    snapshot("build/window-opening-133.ppm");
    for (int i = 0; i < WINDOW_OPEN_STEPS + 1; i++) repaint();
    check(anim_ticks[NOTES] == 0);
    close_window(NOTES, 1);
    check(anim_ticks[NOTES] == WINDOW_CLOSE_STEPS && anim_exit[NOTES]);
    for (int i = 0; i < WINDOW_CLOSE_STEPS + 1; i++) repaint();
    check(windows[NOTES].minimized && !anim_visible[NOTES]);
    open_window(NOTES);
    check(anim_ticks[NOTES] == WINDOW_OPEN_STEPS && !windows[NOTES].minimized);
    printf("PASS: three-step eased window opening, intact five-step closing and minimize, tinted active and hover taskbar tiles, plain inactive icons\n");
    return 0;
}
