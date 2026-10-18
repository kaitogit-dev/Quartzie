#ifndef QUARTZIE_PINTA_C
#define QUARTZIE_PINTA_C
#include <stdint.h>
#define PINTA_PENCIL 0
#define PINTA_BRUSH 1
#define PINTA_ERASER 2
#define PINTA_LINE 3
#define PINTA_RECTANGLE 4
#define PINTA_FILL 5
#define PINTA_TOOLS 6

static void pinta_stamp(uint8_t *image, int width, int height, int x, int y, int color, int radius) {
    if (!image || width <= 0 || height <= 0 || radius < 0 || radius > 24) return;
    for (int dy = -radius; dy <= radius; dy++) {
        int py = y + dy;
        if (py < 0 || py >= height) continue;
        for (int dx = -radius; dx <= radius; dx++) {
            int px = x + dx;
            if (px >= 0 && px < width && dy * dy + dx * dx <= radius * radius + radius)
                image[py * width + px] = (uint8_t)color;
        }
    }
}

static void pinta_line(uint8_t *image, int width, int height, int x0, int y0, int x1, int y1, int color, int radius) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    if (dx > 2048 || dy > 2048) return;
    for (;;) {
        pinta_stamp(image, width, height, x0, y0, color, radius);
        if (x0 == x1 && y0 == y1) break;
        int twice = err * 2;
        if (twice > -dy) { err -= dy; x0 += sx; }
        if (twice < dx) { err += dx; y0 += sy; }
    }
}

static void pinta_rectangle(uint8_t *image, int width, int height, int x0, int y0, int x1, int y1, int color, int radius) {
    pinta_line(image, width, height, x0, y0, x1, y0, color, radius);
    pinta_line(image, width, height, x1, y0, x1, y1, color, radius);
    pinta_line(image, width, height, x1, y1, x0, y1, color, radius);
    pinta_line(image, width, height, x0, y1, x0, y0, color, radius);
}

static int pinta_fill(uint8_t *image, int width, int height, int x, int y, int color, uint32_t *scratch, uint32_t scratch_size) {
    if (!image || !scratch || x < 0 || y < 0 || x >= width || y >= height || scratch_size < (uint32_t)(width * height)) return 0;
    int target = image[y * width + x];
    if (target == color) return 1;
    uint32_t top = 0;
    uint32_t start = (uint32_t)(y * width + x);
    scratch[top++] = start;
    image[start] = (uint8_t)color;
    while (top) {
        uint32_t at = scratch[--top];
        int px = (int)(at % (uint32_t)width);
        int py = (int)(at / (uint32_t)width);
        int nx[4] = {px - 1, px + 1, px, px};
        int ny[4] = {py, py, py - 1, py + 1};
        for (int k = 0; k < 4; k++) {
            if (nx[k] < 0 || ny[k] < 0 || nx[k] >= width || ny[k] >= height) continue;
            uint32_t next = (uint32_t)(ny[k] * width + nx[k]);
            if (image[next] != target) continue;
            image[next] = (uint8_t)color;
            if (top < scratch_size) scratch[top++] = next;
        }
    }
    return 1;
}

static void pinta_copy(uint8_t *dst, const uint8_t *src, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}
#endif
