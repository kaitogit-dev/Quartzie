#include <stdint.h>
#include <stddef.h>
#include "proc.h"
#include "pe.h"
#include "pinta.c"
#ifndef __i386__
volatile uint32_t cm_pe_last_exit;
volatile uint32_t cm_pe_completed;
volatile uint32_t cm_pe_launches;
int cm_proc_launch_pe(const uint8_t *data, uint32_t count) { (void)data; (void)count; return CM_PE_UNSUPPORTED_RUNTIME; }
int cm_paint_submit(int x, int y, uint32_t color, uint32_t radius) { (void)x; (void)y; (void)color; (void)radius; return 0; }
#endif

extern uint32_t boot_fb;
extern uint16_t boot_pitch;
extern uint16_t boot_width;
extern uint16_t boot_height;
extern uint8_t boot_bpp;
extern const uint8_t quartzie_font[96][16][8];
extern const uint8_t _binary_build_wallpaper_bin_start[];
extern const uint8_t _binary_build_wallpaper_bin_end[];

static uint8_t pe_file_cache[4097];
static uint32_t pe_reported_exits;
static int pe_last_launch_status;
#ifdef __i386__
static int cm_userspace_ready;
#endif
static int theme;
static int previous_theme;
static int do_not_disturb;
static int wallpaper;
static int glass_level = 2;
static int accent_index;
static const uint32_t accent_colors[4] = {0x2167b3,0x178f99,0x7852ba,0xcf7343};
static int desktop_hidden;
static int desktop_saved[11];
#define INK (theme == 3 ? 0xe9f2ffu : theme == 2 ? 0x1d2b4au : theme == 1 ? 0x3a597cu : 0x243d60u)
#define INK2 (theme == 3 ? 0xb9d6f0u : theme == 2 ? 0x435c86u : theme == 1 ? 0x6e93b4u : 0x537caau)
#define STEEL (theme == 3 ? 0x97b4d0u : theme == 2 ? 0x8ea7ccu : theme == 1 ? 0xb7cadfu : 0xa7bfd8u)
#define PAPER (theme == 3 ? 0x182637u : theme == 2 ? 0xeef4fcu : theme == 1 ? 0xf8fbfeu : 0xf9fcffu)
#define WHITE (theme == 3 ? 0xf4faffu : theme == 2 ? 0xffffffu : theme == 1 ? 0xffffffu : 0xffffffu)
#define FLOOR (theme == 3 ? 0x132b44u : theme == 2 ? 0x122c58u : theme == 1 ? 0x638eacu : 0x285f9au)
#define HAIR (theme == 3 ? 0x39546fu : theme == 2 ? 0xb5c5dfu : theme == 1 ? 0xdbe5edu : 0xd4e2efu)
#define LIGHT (theme == 3 ? 0x26384du : theme == 2 ? 0xe3eaf7u : theme == 1 ? 0xfafcffu : 0xf2f7fcu)
#define BLUE (accent_index > 0 && accent_index < 4 ? (accent_index == 1 ? 0x178f99u : accent_index == 2 ? 0x7852bau : 0xcf7343u) : (theme == 3 ? 0x5fa9f2u : theme == 2 ? 0x4679c5u : theme == 1 ? 0x3d729fu : 0x2167b3u))
#define ACTIVE (theme == 3 ? 0xa4dcffu : theme == 2 ? 0x93b9fbu : theme == 1 ? 0x9ed6fau : 0x7ec5fcu)
#define BLACK (theme == 3 ? 0xb1729u : theme == 2 ? 0x7152cu : theme == 1 ? 0x213b56u : 0x10223bu)
#define DESK2 (theme == 3 ? 0x243955u : theme == 2 ? 0x22416cu : theme == 1 ? 0x7797b8u : 0x3067a0u)
#define TILE (theme == 3 ? 0x304863u : theme == 2 ? 0xdce7fau : theme == 1 ? 0xeef4fau : 0xe5f0fcu)
#define NIGHT (theme == 3)
#define UI_CARD (NIGHT ? 0x162235 : 0xf7fbff)
#define UI_FIELD (NIGHT ? 0x111b2a : 0xfcfeff)
#define UI_HOVER (NIGHT ? 0x27415e : 0xe3f1ff)
#define UI_EDGE (NIGHT ? 0x7ca1c1 : 0x9cbfde)
#define UI_PANEL (NIGHT ? 0x18283d : 0xe5f1fd)
#define UI_HEAD (NIGHT ? 0x213a58 : 0x78c0f4)

#define FILES 0
#define NOTES 1
#define CONSOLE 2
#define SYSTEM 3
#define CLOCK 4
#define CALC 5
#define APPEAR 6
#define SKETCH 7
#define CALENDAR 8
#define WINDOWS_APP 9
#define APPS 10
#define WINDOW_OPEN_STEPS 2
#define WINDOW_RESIZE_STEPS 3
#define WINDOW_CLOSE_STEPS 4
#define SKETCH_W 224
#define SKETCH_H 110
#define CURSOR_W 22
#define CURSOR_H 21
#define CURSOR_BOX_W (CURSOR_W + 1)
#define CURSOR_BOX_H (CURSOR_H + 1)

typedef struct { int x; int y; int w; int h; int opened; int minimized; int order; int maximized; int restore_x; int restore_y; int restore_w; int restore_h; } Window;

static Window windows[APPS];
static const char *app_name[APPS] = {"Files", "Notes", "Console", "System", "Clock", "Calculator", "Settings", "Pinta", "Calendar", "Windows"};
static int anim_x0[APPS], anim_y0[APPS], anim_w0[APPS], anim_h0[APPS], anim_ticks[APPS], anim_total[APPS], anim_visible[APPS], anim_exit[APPS];
static int quick_open;
static int start_anim;
static int context_anim;
static int quick_anim;
static int notification_open;
static int notification_anim;
static int notification_count;
static int notification_unread;
static char notification_text[5][48];
static char notification_time[5][9];
static int volume_level = 60;
static int volume_muted;
static uint32_t preview_buffer[172 * 92];
static uint32_t *const canvas = (uint32_t *)0x400000;
static uint32_t *const desktop_cache = (uint32_t *)0xD00000;
static int cached_wallpaper = -1;
static int cached_wallpaper_night = -1;
static int pointer_only;
static int taskbar_dirty = 1;
static uint16_t timer_counter;
static uint32_t timer_accum;
static uint32_t timer_poll;
static uint32_t timer_unchanged;
static int animation_step_allowed = 1;
static int pointer_old_x;
static int pointer_old_y;
static int pointer_saved;
static uint32_t pointer_back[CURSOR_BOX_H][CURSOR_BOX_W];
static uint32_t shadow_pixels;
static uint32_t desktop_regenerations;
static uint32_t full_frames;
static uint32_t pointer_frames;
static uint32_t damage_frames;
static uint32_t presented_pixels;
static int frame_valid;
static int frame_theme = -1, frame_accent = -1, frame_glass = -1, frame_wallpaper = -1;
typedef struct { int x, y, w, h; } DamageRect;
static DamageRect damage[32];
static int damage_count;
static DamageRect last_window_rect[APPS];
static uint8_t last_window_visible[APPS];
static int last_window_order[APPS];
static int last_active = -1;
static int last_start, last_quick, last_context, last_notification, last_hover = -1;
static int last_hover_x, last_hover_running;
static int hover_delay;
static int hover_visible;
static int last_hover_visible;
static int last_context_x, last_context_y, last_snap_side;
#define ROUND_LAYER_CAPACITY 8
#define ROUND_RADIUS_LIMIT 24
typedef struct {
    int x, y, w, h, r;
    uint32_t back[4][ROUND_RADIUS_LIMIT][ROUND_RADIUS_LIMIT];
} RoundLayer;
static RoundLayer round_layers[ROUND_LAYER_CAPACITY];
static int round_layer_depth;
static uint32_t round_layer_errors;
static int sw;
static int sh;
static int clip_left;
static int clip_top;
static int clip_right;
static int clip_bottom;
static int mouse_x;
static int mouse_y;
static int left_down;
static int drag_app = -1;
static int drag_dx;
static int drag_dy;
static int start_open;
static int next_order;
static int task_hover = -1;
static int desktop_hover = -1;
static int desktop_focus = -1;
static int last_desktop_focus = -1;
static int last_desktop_hover = -1;
#define CLICK_WINDOW_TICKS 36
#define TASK_ICON_FRAMES 10
static int click_wait;
static int click_class = -1;
static int click_item = -1;
static int click_x, click_y;
static uint8_t icon_motion[APPS];
static uint8_t icon_frame[APPS];
static int keyboard_extended;
static int clipboard_type;
static char clipboard_filename[24];
static char clipboard_text[2048];
static int clipboard_text_len;
static int notes_select_all;
static int tab_active;
static int tab_apps[APPS];
static int tab_count;
static int tab_selected;
static int last_tab_active;
static int last_tab_selected;
#define PANEL_START 0
#define PANEL_CONTEXT 1
#define PANEL_QUICK 2
#define PANEL_NOTIFICATIONS 3
#define PANEL_RUN 4
#define PANEL_SWITCHER 5
#define PANEL_TOOLTIP 6
#define PANEL_COUNT 7
#define PANEL_STEPS 18
static uint8_t panel_step[PANEL_COUNT];
static uint8_t panel_goal[PANEL_COUNT];
static uint8_t panel_was_visible[PANEL_COUNT];
static int tooltip_render_app = -1;
static int tooltip_render_running;
static int context_open;
static int context_x;
static int context_y;
static int right_down;
static int resize_app = -1;
static int snap_side[APPS];
static int selected_file;
static int file_location = 6;
static int file_sort;
static int file_view;
static int file_details = 1;
static int file_created;
static int file_search_focus;
static char file_search[32];
static int file_search_len;
static int settings_page = 1;
static int settings_search_focus;
static char settings_search[32];
static int settings_search_len;
#define FILE_COUNT 5
static const char *file_names[FILE_COUNT] = {"readme.txt", "boot.log", "notes.txt", "wallpaper.bmp", "calendar.dat"};
static const char *file_types[FILE_COUNT] = {"Text Document", "Log File", "Text Document", "Bitmap Image", "Data File"};
static const char *file_sizes[FILE_COUNT] = {"2 KB", "3 KB", "1 KB", "128 KB", "4 KB"};
static const char *file_dates[FILE_COUNT] = {"2026-10-14 20:42", "2026-10-14 20:41", "2026-10-14 20:40", "2026-10-14 20:15", "2026-10-14 19:58"};
static char file_extra_names[3][24] = {"New document.txt", "New document 2.txt", "New document 3.txt"};
static char notes_filename[24] = "notes.txt";
static int notes_dirty;
static int notes_save_failed;
static int sketch_color = 1;
static int sketch_brush = 1;
static uint8_t sketch_pixels[SKETCH_H][SKETCH_W];
static uint8_t sketch_undo[SKETCH_H][SKETCH_W];
static uint8_t sketch_redo[SKETCH_H][SKETCH_W];
static uint32_t sketch_fill_stack[SKETCH_H * SKETCH_W];
static int sketch_has_undo;
static int sketch_has_redo;
static int sketch_tool = PINTA_BRUSH;
static int sketch_stroking;
static int sketch_x0, sketch_y0, sketch_prev_x, sketch_prev_y;
static int sketch_dirty;
static int sketch_saved;
static int sketch_disk_loaded;
static int calendar_shift;
static int calendar_day;
static char menu_query[32];
static int menu_query_len;
static int run_open;
static int run_anim;
static char run_input[40];
static int run_len;
static const uint32_t ink_colors[9] = {0xffffff,0x222d37,0xde9060,0x447e85,0x567cb0,0x73946c,0xb35464,0xf0d26a,0x9165b5};
static int dirty = 1;
static int shift_down;
static int control_down;
static int alt_down;
static uint8_t mouse_packet[3];
static int mouse_index;
static char notes[2048] = "";
static int notes_len;
static char console_input[80];
static int console_len;
static char log_lines[32][72];
static int log_count;
static char cpu_vendor[13];
static char clock_time[9] = "00:00:00";
static char clock_date[11] = "2000-01-01";
static uint8_t last_second = 255;
static char calc_input[32] = "";
static int calc_len;
static char calc_result[32] = "0";
#define CURSOR_W 22
#define CURSOR_H 21
#define CURSOR_BOX_W (CURSOR_W + 1)
#define CURSOR_BOX_H (CURSOR_H + 1)
static const uint8_t cursor_alpha[CURSOR_H][CURSOR_W] = {
    {0,0,4,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,98,244,239,136,27,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {4,244,255,255,255,249,160,46,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {2,239,255,255,255,255,255,254,184,69,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,136,255,255,255,255,255,255,255,255,209,102,8,0,0,0,0,0,0,0,0,0},
    {0,26,249,255,255,255,255,255,255,255,255,255,231,128,17,0,0,0,0,0,0,0},
    {0,0,160,255,255,255,255,255,255,255,255,255,255,255,242,145,30,0,0,0,0,0},
    {0,0,44,254,255,255,255,255,255,255,255,255,255,255,255,255,253,179,55,0,0,0},
    {0,0,0,184,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,188,43,0},
    {0,0,0,69,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,220,0},
    {0,0,0,0,208,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,251,5},
    {0,0,0,0,93,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,162,0},
    {0,0,0,0,5,227,255,255,255,255,255,255,255,255,255,255,253,215,163,99,5,0},
    {0,0,0,0,0,116,255,255,255,255,255,255,255,214,114,63,13,0,0,0,0,0},
    {0,0,0,0,0,14,240,255,255,255,255,255,255,119,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,140,255,255,255,255,255,255,68,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,28,250,255,255,255,255,254,18,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,164,255,255,255,255,219,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,46,254,255,255,255,167,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,188,255,255,255,104,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,41,212,250,160,5,0,0,0,0,0,0,0,0,0}
};


static inline uint8_t in8(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void out8(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" :: "a"(value), "Nd"(port));
}

#include "storage.c"
#include "objects.c"
#include "ntos.c"

static int same(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

static int begins(const char *a, const char *b) {
    while (*b) if (*a++ != *b++) return 0;
    return 1;
}

static int size(const char *text) {
    int n = 0;
    while (text[n]) n++;
    return n;
}

static void copy(char *dst, const char *src, int limit) {
    int i = 0;
    while (i < limit - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void append(char *dst, const char *src, int cap) {
    int n = size(dst);
    int i = 0;
    while (src[i] && n < cap - 1) dst[n++] = src[i++];
    dst[n] = 0;
}

static int same_fold(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return 0;
        i++;
    }
    return !a[i] && !b[i];
}


static void number(char *dst, uint32_t n) {
    char rev[12];
    int i = 0;
    do { rev[i++] = (char)('0' + n % 10); n /= 10; } while (n);
    int j = 0;
    while (i) dst[j++] = rev[--i];
    dst[j] = 0;
}

static uint8_t cmos_read(uint8_t reg) {
    out8(0x70, (uint8_t)(reg | 0x80));
    return in8(0x71);
}

static uint8_t rtc_decimal(uint8_t value, uint8_t control) {
    if (control & 4) return value;
    return (uint8_t)((value & 15) + (value >> 4) * 10);
}

static void rtc_two(char *out, uint8_t n) {
    out[0] = (char)('0' + (n / 10) % 10);
    out[1] = (char)('0' + n % 10);
}

static void refresh_clock(void) {
    if (cmos_read(10) & 0x80) return;
    uint8_t sec = cmos_read(0);
    if (sec == last_second) return;
    last_second = sec;
    uint8_t min = cmos_read(2);
    uint8_t hour = cmos_read(4);
    uint8_t day = cmos_read(7);
    uint8_t month = cmos_read(8);
    uint8_t year = cmos_read(9);
    uint8_t ctrl = cmos_read(11);
    uint8_t pm = hour & 0x80;
    hour = rtc_decimal(hour & 0x7f, ctrl);
    if (!(ctrl & 2)) hour = (uint8_t)((hour % 12) + (pm ? 12 : 0));
    rtc_two(clock_time, hour);
    rtc_two(clock_time + 3, rtc_decimal(min, ctrl));
    rtc_two(clock_time + 6, rtc_decimal(sec, ctrl));
    rtc_two(clock_date + 2, rtc_decimal(year, ctrl));
    rtc_two(clock_date + 5, rtc_decimal(month, ctrl));
    rtc_two(clock_date + 8, rtc_decimal(day, ctrl));
    dirty = 1;
    taskbar_dirty = 1;
}

static void notify(const char *message) {
    if (notification_count == 5) {
        for (int k = 1; k < 5; k++) {
            copy(notification_text[k - 1], notification_text[k], 48);
            copy(notification_time[k - 1], notification_time[k], 9);
        }
        notification_count = 4;
    }
    copy(notification_text[notification_count], message, 48);
    copy(notification_time[notification_count], clock_time, 9);
    notification_count++;
    if (!do_not_disturb) notification_unread = notification_count;
    dirty = 1;
    taskbar_dirty = 1;
}

static void toggle_desktop(void);
static void open_window(int app);
static void icon_kick(int app, int mode);
static void cm_ui_dispatch(int app, const struct CmMessage *event);

static void select_theme(int selected) {
    if (selected < 0 || selected > 3) return;
    if (selected != 3) previous_theme = selected;
    if (theme == selected) return;
    theme = selected;
    nt_save_theme((uint32_t)theme);
    dirty = 1;
    taskbar_dirty = 1;
}

static void toggle_dark_aero(void) {
    select_theme(NIGHT ? previous_theme : 3);
}

static void toggle_quiet(void) {
    do_not_disturb = !do_not_disturb;
    if (do_not_disturb) notification_unread = 0;
    dirty = 1;
    taskbar_dirty = 1;
}

static void signed_number(char *out, int32_t n) {
    if (n < 0) { out[0] = '-'; number(out + 1, (uint32_t)(-(int64_t)n)); }
    else number(out, (uint32_t)n);
}

static void calculate(void) {
    unsigned i = 0;
    int32_t lhs = 0;
    int32_t rhs = 0;
    while (calc_input[i] >= '0' && calc_input[i] <= '9') {
        lhs = lhs * 10 + (calc_input[i++] - '0');
        if (lhs > 100000) { copy(calc_result, "range", 32); return; }
    }
    char op = calc_input[i++];
    if (op != '+' && op != '-' && op != '*' && op != '/') { copy(calc_result, "syntax", 32); return; }
    if (!calc_input[i]) { copy(calc_result, "syntax", 32); return; }
    while (calc_input[i] >= '0' && calc_input[i] <= '9') {
        rhs = rhs * 10 + (calc_input[i++] - '0');
        if (rhs > 100000) { copy(calc_result, "range", 32); return; }
    }
    if (calc_input[i]) { copy(calc_result, "syntax", 32); return; }
    if (op == '/' && rhs == 0) { copy(calc_result, "division by zero", 32); return; }
    if (op == '*' && lhs > 0 && rhs > 2147483647 / lhs) { copy(calc_result, "range", 32); return; }
    int32_t result = op == '+' ? lhs + rhs : op == '-' ? lhs - rhs : op == '*' ? lhs * rhs : lhs / rhs;
    signed_number(calc_result, result);
}

static void calc_press(char key) {
    if (key == 'C') { calc_len = 0; calc_input[0] = 0; copy(calc_result, "0", 32); }
    else if (key == 'B') { if (calc_len) calc_input[--calc_len] = 0; }
    else if (key == '=') calculate();
    else if (calc_len < 29) {
        if ((key == '+' || key == '-' || key == '*' || key == '/') && calc_len == 0) return;
        calc_input[calc_len++] = key;
        calc_input[calc_len] = 0;
    }
    dirty = 1;
}

static void clip_full(void) {
    clip_left = 0;
    clip_top = 0;
    clip_right = sw;
    clip_bottom = sh;
}

static void pixel(int x, int y, uint32_t color) {
    if (x < clip_left || x >= clip_right || y < clip_top || y >= clip_bottom) return;
    if ((unsigned)x >= (unsigned)sw || (unsigned)y >= (unsigned)sh) return;
    canvas[(size_t)y * (size_t)sw + (size_t)x] = color;
}

static void rect(int x, int y, int w, int h, uint32_t color) {
    int x0 = x > clip_left ? x : clip_left;
    int y0 = y > clip_top ? y : clip_top;
    int x1 = x + w < clip_right ? x + w : clip_right;
    int y1 = y + h < clip_bottom ? y + h : clip_bottom;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > sw) x1 = sw;
    if (y1 > sh) y1 = sh;
    int width = x1 - x0;
    if (width <= 0) return;
    for (int py = y0; py < y1; py++) {
        uint32_t *line = canvas + (size_t)py * (size_t)sw + x0;
        if (width >= 64) {
            uint32_t *dst = line;
            size_t count = (size_t)width;
            __asm__ volatile("cld; rep stosl" : "+D"(dst), "+c"(count) : "a"(color) : "memory");
        } else for (int px = 0; px < width; px++) line[px] = color;
    }
}

static uint32_t blend(uint32_t a, uint32_t b, int weight) {
    int inv = 256 - weight;
    uint32_t rb = ((((a & 0x00ff00ffu) * (uint32_t)inv) + ((b & 0x00ff00ffu) * (uint32_t)weight)) >> 8) & 0x00ff00ffu;
    uint32_t g = ((((a & 0x0000ff00u) * (uint32_t)inv) + ((b & 0x0000ff00u) * (uint32_t)weight)) >> 8) & 0x0000ff00u;
    return rb | g;
}

static int clamp8(int value) {
    return value < 0 ? 0 : value > 255 ? 255 : value;
}

static uint32_t saturate_color(uint32_t color, int amount) {
    int r = (int)((color >> 16) & 255);
    int g = (int)((color >> 8) & 255);
    int b = (int)(color & 255);
    int avg = (r + g + b) / 3;
    r = clamp8(avg + (((r - avg) * amount) >> 8));
    g = clamp8(avg + (((g - avg) * amount) >> 8));
    b = clamp8(avg + (((b - avg) * amount) >> 8));
    return (uint32_t)(r << 16 | g << 8 | b);
}

static void frosted(int x, int y, int w, int h, uint32_t tint, int opacity, int sample_mix) {
    if (w <= 0 || h <= 0) return;
    int x0 = x > clip_left ? x : clip_left;
    int y0 = y > clip_top ? y : clip_top;
    int x1 = x + w < clip_right ? x + w : clip_right;
    int y1 = y + h < clip_bottom ? y + h : clip_bottom;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > sw) x1 = sw;
    if (y1 > sh) y1 = sh;
    if (x0 >= x1 || y0 >= y1) return;
    int weight = sample_mix + (256 - sample_mix) / 2;
    for (int py = y0; py < y1; py++) {
        int uy = py > 4 ? py - 4 : 0;
        int ly = py + 4 < sh ? py + 4 : sh - 1;
        const uint32_t *upper = desktop_cache + (size_t)uy * sw;
        const uint32_t *middle = desktop_cache + (size_t)py * sw;
        const uint32_t *lower = desktop_cache + (size_t)ly * sw;
        uint32_t *destination = canvas + (size_t)py * sw;
        for (int px = x0; px < x1; px++) {
            int lx = px > 4 ? px - 4 : 0;
            int rx = px + 4 < sw ? px + 4 : sw - 1;
            uint32_t a = upper[px], b = middle[lx], c = middle[px], d = middle[rx], e = lower[px];
            uint32_t blurred = (((((a >> 16 & 255) + (b >> 16 & 255) + 2 * (c >> 16 & 255) + (d >> 16 & 255) + (e >> 16 & 255)) / 6) << 16) |
                ((((a >> 8 & 255) + (b >> 8 & 255) + 2 * (c >> 8 & 255) + (d >> 8 & 255) + (e >> 8 & 255)) / 6) << 8) |
                (((a & 255) + (b & 255) + 2 * (c & 255) + (d & 255) + (e & 255)) / 6));
            uint32_t sampled = opacity ? saturate_color(blurred, NIGHT ? 280 : 300) : blurred;
            destination[px] = blend(destination[px], blend(sampled, tint, opacity), weight);
        }
    }
}

static int ease(int now, int total) {
    if (total <= 0) return 256;
    if (now < 0) now = 0;
    if (now > total) now = total;
    int t = now * 256 / total;
    int inv = 256 - t;
    return 256 - (inv * inv >> 8);
}

static int tween_value(int from, int to, int phase, int total) {
    int e = ease(phase, total);
    return from + ((to - from) * e >> 8);
}

static int panel_visible(int id) {
    return panel_step[id] > 0 || panel_goal[id] > 0;
}

static int panel_shift(int id, int distance) {
    return ((256 - ease(panel_step[id], PANEL_STEPS)) * distance) >> 8;
}

static void panel_sync(void) {
    int desired[PANEL_COUNT] = {
        start_open, context_open, quick_open, notification_open,
        run_open, tab_active,
        hover_visible && task_hover >= 0 && !start_open && !context_open &&
        !quick_open && !notification_open && !run_open && !tab_active
    };
    for (int i = 0; i < PANEL_COUNT; i++) {
        panel_goal[i] = desired[i] ? PANEL_STEPS : 0;
    }
    if (desired[PANEL_TOOLTIP]) {
        tooltip_render_app = task_hover;
        tooltip_render_running = windows[task_hover].opened && !windows[task_hover].minimized;
    }
}

static void panel_advance(void) {
    int steps = animation_step_allowed > 0 ? animation_step_allowed : 1;
    for (int i = 0; i < PANEL_COUNT; i++) {
        if (panel_step[i] < panel_goal[i]) {
            int next = panel_step[i] + steps;
            panel_step[i] = (uint8_t)(next > panel_goal[i] ? panel_goal[i] : next);
        } else if (panel_step[i] > panel_goal[i]) {
            int next = panel_step[i] - steps;
            panel_step[i] = (uint8_t)(next < panel_goal[i] ? panel_goal[i] : next);
            if (!panel_step[i]) dirty = 1;
        }
    }
}

static void begin_window_animation(int app, int from_x, int from_y, int from_w, int from_h, int steps, int keep_visible) {
    anim_x0[app] = from_x;
    anim_y0[app] = from_y;
    anim_w0[app] = from_w;
    anim_h0[app] = from_h;
    anim_total[app] = steps;
    anim_ticks[app] = steps;
    anim_visible[app] = keep_visible;
    anim_exit[app] = 0;
    dirty = 1;
}

static void animated_window_rect(int app, int *x, int *y, int *w, int *h);

static void notes_commit(void) {
    if (!notes_dirty) return;
    if (storage_ready) {
        HANDLE handle = 0;
        char path[48] = "\\??\\D:\\";
        append(path, notes_filename, sizeof(path));
        IO_STATUS_BLOCK io = {0};
        NTSTATUS status = nt_open_ascii(&handle, path, FILE_WRITE_DATA, FILE_OVERWRITE_IF);
        if (NT_SUCCESS(status)) status = NtWriteFile(handle, 0, NULL, NULL, &io, notes, (uint32_t)notes_len, NULL, NULL);
        if (NT_SUCCESS(status)) status = NtFlushBuffersFile(handle, &io);
        if (handle) {
            if (NT_SUCCESS(status)) status = NtClose(handle);
            else nt_file_abort(handle);
        }
        if (NT_SUCCESS(status)) { notes_dirty = 0; notes_save_failed = 0; }
        else notes_save_failed = 1;
    }
    dirty = 1;
}

static int notes_select(const char *name) {
    if (notes_dirty) {
        notes_commit();
        if (storage_ready && notes_dirty) return 0;
    }
    copy(notes_filename, name, 24);
    notes_len = 0;
    notes[0] = 0;
    if (storage_ready) {
        HANDLE handle = 0;
        char path[48] = "\\??\\D:\\";
        append(path, notes_filename, sizeof(path));
        if (NT_SUCCESS(nt_open_ascii(&handle, path, FILE_READ_DATA, FILE_OPEN))) {
            IO_STATUS_BLOCK io = {0};
            FILE_STANDARD_INFORMATION info;
            NtQueryInformationFile(handle, &io, &info, sizeof(info), NT_FILE_STANDARD_INFORMATION);
            NTSTATUS status = NtReadFile(handle, 0, NULL, NULL, &io, notes, sizeof(notes) - 1, NULL, NULL);
            if (NT_SUCCESS(status) || status == STATUS_END_OF_FILE) {
                notes_len = (int)io.Information;
                notes[notes_len] = 0;
            }
            NtClose(handle);
        }
    }
    notes_dirty = 0;
    notes_save_failed = 0;
    return 1;
}

static int pinta_save_document(void);
static int pinta_load_document(void);

static void close_window(int app, int minimizing) {
    if (!windows[app].opened || windows[app].minimized) return;
    if (app == NOTES) notes_commit();
    if (app == SKETCH && sketch_dirty) pinta_save_document();
    uint32_t handle = cm_window_handle(app);
    if (handle) {
        cm_post(handle, minimizing ? CM_MSG_MINIMIZE : CM_MSG_CLOSE, 0, 0);
        cm_pump(CM_QUEUE_LIMIT, cm_ui_dispatch);
        cm_window_close(app, minimizing);
    }
    Window *w = &windows[app];
    int x, y, width, height;
    animated_window_rect(app, &x, &y, &width, &height);
    begin_window_animation(app, x, y, width, height, WINDOW_CLOSE_STEPS, 1);
    anim_exit[app] = 1;
    if (minimizing) { w->minimized = 1; icon_kick(app, 2); }
    else w->opened = 0;
}

static void animated_window_rect(int app, int *x, int *y, int *w, int *h) {
    *x = windows[app].x;
    *y = windows[app].y;
    *w = windows[app].w;
    *h = windows[app].h;
    if (anim_ticks[app] > 0) {
        int phase = anim_total[app] - anim_ticks[app];
        int to_x = anim_exit[app] ? anim_x0[app] + anim_w0[app] / 7 : windows[app].x;
        int to_y = anim_exit[app] ? anim_y0[app] + 24 : windows[app].y;
        int to_w = anim_exit[app] ? anim_w0[app] * 6 / 7 : windows[app].w;
        int to_h = anim_exit[app] ? anim_h0[app] * 9 / 10 : windows[app].h;
        *x = tween_value(anim_x0[app], to_x, phase, anim_total[app]);
        *y = tween_value(anim_y0[app], to_y, phase, anim_total[app]);
        *w = tween_value(anim_w0[app], to_w, phase, anim_total[app]);
        *h = tween_value(anim_h0[app], to_h, phase, anim_total[app]);
    }
}

static void glass(int x, int y, int w, int h, uint32_t tint, int opacity) {
    int x0 = x > clip_left ? x : clip_left;
    int y0 = y > clip_top ? y : clip_top;
    int x1 = x + w < clip_right ? x + w : clip_right;
    int y1 = y + h < clip_bottom ? y + h : clip_bottom;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > sw) x1 = sw;
    if (y1 > sh) y1 = sh;
    for (int py = y0; py < y1; py++) {
        uint32_t *row = canvas + (size_t)py * sw;
        for (int px = x0; px < x1; px++) row[px] = blend(row[px], tint, opacity);
    }
}

static void blur_region(int x, int y, int w, int h) {
    frosted(x, y, w, h, 0, 0, 256);
}

static void smooth_pixel(int x, int y, uint32_t color, int coverage) {
    if (coverage <= 0 || x < clip_left || x >= clip_right || y < clip_top || y >= clip_bottom ||
        (unsigned)x >= (unsigned)sw || (unsigned)y >= (unsigned)sh) return;
    uint32_t *dst = canvas + (size_t)y * sw + x;
    *dst = coverage >= 256 ? color : blend(*dst, color, coverage);
}

static void rounded(int x, int y, int w, int h, int radius, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (radius < 2 || w < radius * 2 || h < radius * 2) { rect(x, y, w, h, color); return; }
    rect(x + radius, y, w - radius * 2, h, color);
    rect(x, y + radius, radius, h - radius * 2, color);
    rect(x + w - radius, y + radius, radius, h - radius * 2, color);
    for (int row = 0; row < radius; row++) {
        for (int col = 0; col < radius; col++) {
            int coverage = 0;
            for (int a = 1; a <= 3; a += 2)
                for (int b = 1; b <= 3; b += 2) {
                    int dx = radius * 4 - (col * 4 + a);
                    int dy = radius * 4 - (row * 4 + b);
                    if (dx * dx + dy * dy <= radius * radius * 16) coverage += 64;
                }
            if (!coverage) continue;
            smooth_pixel(x + col, y + row, color, coverage);
            smooth_pixel(x + w - 1 - col, y + row, color, coverage);
            smooth_pixel(x + col, y + h - 1 - row, color, coverage);
            smooth_pixel(x + w - 1 - col, y + h - 1 - row, color, coverage);
        }
    }
}

static void round_capture(int x, int y, int w, int h, int r) {
    if (w <= 0 || h <= 0 || r <= 0 || r > ROUND_RADIUS_LIMIT || r * 2 > w || r * 2 > h ||
        round_layer_depth >= ROUND_LAYER_CAPACITY) { round_layer_errors++; return; }
    RoundLayer *layer = &round_layers[round_layer_depth++];
    layer->x = x; layer->y = y; layer->w = w; layer->h = h; layer->r = r;
    for (int c = 0; c < 4; c++)
        for (int yy = 0; yy < r; yy++)
            for (int xx = 0; xx < r; xx++) {
                int px = x + ((c & 1) ? w - r : 0) + xx;
                int py = y + ((c & 2) ? h - r : 0) + yy;
                layer->back[c][yy][xx] = (unsigned)px < (unsigned)sw && (unsigned)py < (unsigned)sh ? canvas[(size_t)py * sw + px] : 0;
            }
}

static void round_finish(int x, int y, int w, int h, int r) {
    if (round_layer_depth <= 0) { round_layer_errors++; return; }
    RoundLayer *layer = &round_layers[--round_layer_depth];
    if (layer->x != x || layer->y != y || layer->w != w || layer->h != h || layer->r != r) {
        round_layer_errors++;
        return;
    }
    for (int c = 0; c < 4; c++)
        for (int yy = 0; yy < r; yy++)
            for (int xx = 0; xx < r; xx++) {
                int px = x + ((c & 1) ? w - r : 0) + xx;
                int py = y + ((c & 2) ? h - r : 0) + yy;
                if ((unsigned)px >= (unsigned)sw || (unsigned)py >= (unsigned)sh ||
                    px < clip_left || px >= clip_right || py < clip_top || py >= clip_bottom) continue;
                int sx = (c & 1) ? r - 1 - xx : xx;
                int sy = (c & 2) ? r - 1 - yy : yy;
                int coverage = 0;
                for (int a = 1; a <= 3; a += 2)
                    for (int b = 1; b <= 3; b += 2) {
                        int dx = 4 * r - (4 * sx + a);
                        int dy = 4 * r - (4 * sy + b);
                        if (dx * dx + dy * dy <= 16 * r * r) coverage += 64;
                    }
                uint32_t *dst = &canvas[(size_t)py * sw + px];
                if (!coverage) *dst = layer->back[c][yy][xx];
                else if (coverage < 256) *dst = blend(layer->back[c][yy][xx], *dst, coverage);
            }
}

static void outline(int x, int y, int w, int h, uint32_t color);

static void circle(int cx, int cy, int radius, uint32_t color) {
    if (radius <= 0) return;
    int rr = radius * radius * 16;
    for (int yy = -radius; yy <= radius; yy++) {
        for (int xx = -radius; xx <= radius; xx++) {
            int coverage = 0;
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sx = -1; sx <= 1; sx += 2) {
                    int dx = xx * 4 + sx;
                    int dy = yy * 4 + sy;
                    if (dx * dx + dy * dy <= rr) coverage += 64;
                }
            int px = cx + xx, py = cy + yy;
            if (!coverage || px < clip_left || px >= clip_right || py < clip_top || py >= clip_bottom) continue;
            if ((unsigned)px >= (unsigned)sw || (unsigned)py >= (unsigned)sh) continue;
            uint32_t *dest = canvas + (size_t)py * sw + px;
            *dest = coverage == 256 ? color : blend(*dest, color, coverage);
        }
    }
}

static __attribute__((unused)) void glass_frame(int x, int y, int w, int h, uint32_t tint, int strength) {
    glass(x, y, w, h, tint, strength);
    glass(x + 1, y + 1, w - 2, 1, 0xffffff, 120);
    rect(x + 1, y + h - 2, w - 2, 1, 0x7893ac);
    rect(x, y + 1, 1, h - 2, 0x527493);
    rect(x + w - 1, y + 1, 1, h - 2, 0x527493);
}

static __attribute__((unused)) void glass_button(int x, int y, int w, int h, uint32_t tint, int strength) {
    glass(x, y, w, h, tint, strength);
    glass(x + 1, y + 1, w - 2, h / 2, 0xffffff, 36);
    outline(x, y, w, h, 0x63819f);
    rect(x + 1, y + 1, w - 2, 1, 0xffffff);
}

static void soft_shadow(int x, int y, int w, int h, int radius, int depth) {
    if (w <= 0 || h <= 0) return;
    int spread = 14 + depth;
    int strength = 66 + depth * 5;
    if (strength > 132) strength = 132;
    int top = y + depth;
    int left = x - spread, right = x + w + spread;
    int upper = top - spread, lower = top + h + spread;
    if (left < clip_left) left = clip_left;
    if (upper < clip_top) upper = clip_top;
    if (right > clip_right) right = clip_right;
    if (lower > clip_bottom) lower = clip_bottom;
    if (left < 0) left = 0;
    if (upper < 0) upper = 0;
    if (right > sw) right = sw;
    if (lower > sh) lower = sh;
    for (int py = upper; py < lower; py++) {
        int dy = py < top + radius ? top + radius - py :
            py >= top + h - radius ? py - (top + h - radius - 1) : 0;
        uint32_t *row = canvas + (size_t)py * sw;
        for (int px = left; px < right; px++) {
            int dx = px < x + radius ? x + radius - px :
                px >= x + w - radius ? px - (x + w - radius - 1) : 0;
            int wide = dx > dy ? dx : dy;
            int narrow = dx > dy ? dy : dx;
            int distance = wide + (narrow * 3) / 8 - radius;
            if (distance <= 0 || distance >= spread) continue;
            int fade = spread - distance;
            int alpha = strength * fade * fade / (spread * spread);
            if (alpha > 0) {
                row[px] = blend(row[px], 0x081322, alpha);
                shadow_pixels++;
            }
        }
    }
}

static void outline(int x, int y, int w, int h, uint32_t color) {
    rect(x, y, w, 1, color);
    rect(x, y + h - 1, w, 1, color);
    rect(x, y, 1, h, color);
    rect(x + w - 1, y, 1, h, color);
}

static void glyph(int x, int y, char c, uint32_t color, int scale) {
    if ((unsigned char)c < 32 || (unsigned char)c > 127) return;
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            int amount = quartzie_font[(int)c - 32][row][col];
            if (!amount) continue;
            if (scale == 1) {
                int px = x + col, py = y + row;
                if (px < clip_left || px >= clip_right || py < clip_top || py >= clip_bottom) continue;
                if ((unsigned)px >= (unsigned)sw || (unsigned)py >= (unsigned)sh) continue;
                uint32_t bg = canvas[(size_t)py * sw + px];
                pixel(px, py, blend(bg, color, amount));
            } else if (amount > 100) rect(x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

static void text(int x, int y, const char *value, uint32_t color) {
    while (*value && x + 8 <= clip_right) { glyph(x, y, *value++, color, 1); x += 8; }
}

static void big_text(int x, int y, const char *value, uint32_t color) {
    while (*value && x + 16 <= clip_right) { glyph(x, y, *value++, color, 2); x += 16; }
}

static void line(int x1, int y1, int x2, int y2, uint32_t color) {
    int ax = x2 - x1, ay = y2 - y1;
    int steep = (ay < 0 ? -ay : ay) > (ax < 0 ? -ax : ax);
    if (steep) { int t=x1; x1=y1; y1=t; t=x2; x2=y2; y2=t; }
    if (x1 > x2) { int t=x1; x1=x2; x2=t; t=y1; y1=y2; y2=t; }
    int dx=x2-x1;
    if (!dx) { if (steep) pixel(y1,x1,color); else pixel(x1,y1,color); return; }
    int step=((y2-y1)*256)/dx;
    int fy=y1*256;
    for (int x=x1; x<=x2; x++) {
        int y=fy>>8;
        int weight=fy-(y<<8);
        if (steep) { smooth_pixel(y,x,color,256-weight); smooth_pixel(y+1,x,color,weight); }
        else { smooth_pixel(x,y,color,256-weight); smooth_pixel(x,y+1,color,weight); }
        fy+=step;
    }
}

static int active_window(void);
static int inside(int px, int py, int x, int y, int w, int h);

static void draw_mark(int x, int y, uint32_t color) {
    rect(x + 5, y + 1, 12, 3, color);
    rect(x + 2, y + 4, 3, 14, color);
    rect(x + 17, y + 4, 3, 14, color);
    rect(x + 5, y + 18, 12, 3, color);
    line(x + 12, y + 14, x + 22, y + 23, color);
}

static void draw_icon(int x, int y, int id) {
    if (id == FILES) {
        rounded(x + 3, y + 11, 35, 25, 4, 0xe6bc7d);
        rounded(x + 3, y + 6, 16, 10, 3, 0xf4d29b);
        rect(x + 7, y + 17, 27, 3, 0xd7a05c);
        rect(x + 7, y + 32, 27, 1, 0xb17e43);
    } else if (id == NOTES) {
        rounded(x + 7, y + 3, 28, 35, 3, PAPER);
        rect(x + 11, y + 9, 19, 4, BLUE);
        rect(x + 11, y + 19, 19, 2, HAIR);
        rect(x + 11, y + 25, 18, 2, HAIR);
        rect(x + 11, y + 31, 14, 2, HAIR);
    } else if (id == CONSOLE) {
        rounded(x + 2, y + 6, 37, 30, 4, BLACK);
        rect(x + 5, y + 10, 30, 2, 0x638db2);
        line(x + 9, y + 16, x + 17, y + 22, ACTIVE);
        line(x + 17, y + 22, x + 9, y + 28, ACTIVE);
        rect(x + 21, y + 28, 12, 2, WHITE);
    } else if (id == CLOCK) {
        rounded(x + 4, y + 4, 34, 34, 5, LIGHT);
        outline(x + 4, y + 4, 34, 34, BLUE);
        line(x + 21, y + 9, x + 21, y + 22, INK);
        line(x + 21, y + 22, x + 30, y + 27, INK);
    } else if (id == CALC) {
        rounded(x + 8, y + 2, 27, 37, 4, LIGHT);
        rounded(x + 12, y + 7, 19, 9, 2, BLUE);
        for (int k = 0; k < 9; k++) rect(x + 12 + k % 3 * 7, y + 20 + k / 3 * 6, 5, 4, INK2);
    } else if (id == APPEAR) {
        rect(x + 4, y + 4, 16, 16, ACTIVE);
        rect(x + 22, y + 4, 16, 16, BLUE);
        rect(x + 4, y + 22, 16, 16, LIGHT);
        rect(x + 22, y + 22, 16, 16, STEEL);
    } else if (id == CALENDAR) {
        rounded(x + 4, y + 4, 34, 34, 4, WHITE);
        rect(x + 4, y + 9, 34, 9, BLUE);
        text(x + 11, y + 20, "15", INK);
    } else if (id == WINDOWS_APP) {
        rounded(x + 3, y + 7, 36, 29, 4, BLUE);
        rect(x + 7, y + 11, 28, 3, WHITE);
        rect(x + 8, y + 19, 11, 12, 0xb3d4f1);
        rect(x + 21, y + 19, 12, 12, WHITE);
    } else if (id == SKETCH) {
        rounded(x + 4, y + 6, 34, 30, 4, PAPER);
        outline(x + 4, y + 6, 34, 30, STEEL);
        line(x + 10, y + 31, x + 31, y + 12, BLUE);
        rect(x + 25, y + 11, 7, 7, ACTIVE);
    } else {
        draw_mark(x + 10, y + 9, ACTIVE);
    }
}

static const int desk_shortcuts[8] = {FILES, NOTES, CONSOLE, SKETCH, CALENDAR, APPEAR, CLOCK, CALC};

static void desktop_position(int slot, int *x, int *y) {
    if (sw >= 1700) { *x = 24 + (slot / 4) * 116; *y = 25 + (slot % 4) * 96; }
    else if (sw >= 600) { *x = 18 + (slot / 4) * 96; *y = 17 + (slot % 4) * 76; }
    else { *x = 7 + (slot % 4) * 77; *y = 9 + (slot / 4) * 62; }
}

static void desktop_slot_bounds(int slot, int *x, int *y, int *w, int *h) {
    desktop_position(slot, x, y);
    if (sw >= 1700) { *w = 94; *h = 85; }
    else if (sw >= 600) { *w = 87; *h = 77; }
    else { *w = 73; *h = 54; }
}

static int desktop_slot_hit(int px, int py) {
    if (py >= sh - 52 || start_open || quick_open || notification_open || context_open || run_open || tab_active) return -1;
    for (int i = 0; i < APPS; i++) {
        if (!windows[i].opened || windows[i].minimized) continue;
        int wx, wy, ww, wh;
        animated_window_rect(i, &wx, &wy, &ww, &wh);
        if (inside(px, py, wx, wy, ww, wh) || inside(px, py, windows[i].x, windows[i].y, windows[i].w, windows[i].h)) return -1;
    }
    for (int k = 0; k < 8; k++) {
        int x, y, w, h;
        desktop_slot_bounds(k, &x, &y, &w, &h);
        if (y + h < sh - 52 && inside(px, py, x, y, w, h)) return k;
    }
    return -1;
}

static const int pinned_apps[7] = {FILES, NOTES, CONSOLE, SKETCH, CALENDAR, CALC, APPEAR};

static int pinned_app(int id) {
    for (int i = 0; i < 7; i++) if (pinned_apps[i] == id) return 1;
    return 0;
}

static int taskbar_width(void) { return 46; }
static int taskbar_step(void) { return 54; }

static int taskbar_items(int *items) {
    int maximum = (sw - (sw >= 520 ? 220 : 98)) / taskbar_step();
    if (maximum > APPS) maximum = APPS;
    if (maximum < 1) maximum = 1;
    int count = 0;
    for (int i = 0; i < 7 && count < maximum; i++) items[count++] = pinned_apps[i];
    for (int i = 0; i < APPS && count < maximum; i++) {
        if (windows[i].opened && !pinned_app(i)) items[count++] = i;
    }
    return count;
}

static int taskbar_left(int count) {
    int span = count * taskbar_step() - (taskbar_step() - taskbar_width());
    int x = (sw - span) / 2;
    if (x < 80) x = 80;
    return x;
}

static int taskbar_hit(int x, int y) {
    if (y < sh - 55 || y >= sh - 9) return -1;
    int apps[APPS];
    int count = taskbar_items(apps);
    int start = taskbar_left(count);
    for (int i = 0; i < count; i++) {
        if (x >= start + taskbar_step() * i && x < start + taskbar_step() * i + taskbar_width()) return apps[i];
    }
    return -1;
}

static void draw_task_icon(int x, int y, int id) {
    if (id == FILES) {
        rounded(x + 2, y + 8, 27, 19, 3, 0xeebf72);
        rect(x + 4, y + 5, 13, 8, 0xf5d69b);
        glass(x + 4, y + 13, 23, 3, 0xffffff, 75);
        rect(x + 5, y + 24, 21, 1, 0xb77e39);
    } else if (id == NOTES) {
        rounded(x + 5, y + 2, 21, 27, 2, 0xf8faff);
        rect(x + 8, y + 7, 16, 4, 0x568cc9);
        rect(x + 8, y + 15, 14, 2, 0x92b2cd);
        rect(x + 8, y + 21, 11, 2, 0x92b2cd);
    } else if (id == CONSOLE) {
        rounded(x + 2, y + 4, 28, 23, 4, 0x152a43);
        rect(x + 5, y + 8, 22, 1, 0x739abc);
        line(x + 8, y + 13, x + 14, y + 18, 0xc4edff);
        line(x + 14, y + 18, x + 8, y + 23, 0xc4edff);
        rect(x + 17, y + 22, 7, 2, 0xb2d9ed);
    } else if (id == SKETCH) {
        rounded(x + 3, y + 3, 26, 25, 3, 0xf2f6fc);
        rect(x + 7, y + 8, 18, 15, 0xd8eaf8);
        line(x + 8, y + 21, x + 22, y + 10, 0x246eaf);
        rect(x + 19, y + 9, 5, 5, 0x6e95cf);
    } else if (id == CALENDAR) {
        rounded(x + 3, y + 3, 26, 25, 4, 0xf8fbff);
        rect(x + 3, y + 7, 26, 6, 0x4188bf);
        text(x + 7, y + 14, "15", 0x274365);
    } else if (id == CALC) {
        rounded(x + 6, y + 2, 21, 27, 3, 0xe9f1f9);
        rect(x + 9, y + 6, 15, 6, 0x316ba8);
        for (int k = 0; k < 9; k++) rect(x + 9 + k % 3 * 6, y + 15 + k / 3 * 5, 4, 3, 0x587ca9);
    } else if (id == APPEAR) {
        rounded(x + 3, y + 4, 26, 24, 4, 0xd7e9f6);
        rect(x + 6, y + 7, 10, 9, 0x3e8dce);
        rect(x + 18, y + 7, 8, 9, 0x79b7e4);
        rect(x + 6, y + 18, 10, 7, 0xa2c3db);
        rect(x + 18, y + 18, 8, 7, 0x45698c);
    } else if (id == CLOCK) {
        circle(x + 16, y + 16, 13, 0xe9f6ff);
        circle(x + 16, y + 16, 10, 0x68a5d4);
        circle(x + 16, y + 16, 8, 0xf6fbff);
        line(x + 16, y + 16, x + 16, y + 9, 0x294f80);
        line(x + 16, y + 16, x + 21, y + 19, 0x294f80);
    } else if (id == WINDOWS_APP) {
        rounded(x + 3, y + 5, 26, 22, 3, 0x4c91ce);
        rect(x + 6, y + 8, 20, 3, 0xf4fbff);
        rect(x + 7, y + 15, 7, 8, 0xe4f4ff);
        rect(x + 16, y + 15, 9, 8, 0xb8dafa);
    } else {
        circle(x + 16, y + 16, 12, 0xd8f0ff);
        circle(x + 16, y + 16, 8, 0x4c85b9);
        circle(x + 16, y + 16, 4, 0xeefaff);
    }
}

static uint32_t task_icon_base[32 * 32];
static uint32_t task_icon_art[32 * 32];
static const uint16_t task_icon_scale[10] = {256, 226, 213, 232, 272, 284, 272, 258, 255, 256};
static const int8_t task_icon_down[10] = {0, 3, 7, 10, 9, 6, 2, -1, 0, 0};
static const int8_t task_icon_up[10] = {0, -3, -7, -11, -9, -5, 1, 2, 0, 0};

static void draw_bouncing_icon(int x, int y, int app) {
    int mode = icon_motion[app];
    if (!mode) { draw_task_icon(x, y, app); return; }
    int phase = icon_frame[app];
    if (phase >= TASK_ICON_FRAMES) phase = TASK_ICON_FRAMES - 1;
    if (mode != 1) {
        draw_task_icon(x, y + (mode == 2 ? task_icon_down[phase] : task_icon_up[phase]), app);
        return;
    }
    int scaled = task_icon_scale[phase];
    if (scaled == 256) { draw_task_icon(x, y, app); return; }
    for (int yy = 0; yy < 32; yy++) for (int xx = 0; xx < 32; xx++) {
        int px = x + xx, py = y + yy;
        task_icon_base[yy * 32 + xx] = (px >= 0 && px < sw && py >= 0 && py < sh) ? canvas[(size_t)py * sw + px] : 0;
    }
    draw_task_icon(x, y, app);
    for (int yy = 0; yy < 32; yy++) for (int xx = 0; xx < 32; xx++) {
        int px = x + xx, py = y + yy;
        task_icon_art[yy * 32 + xx] = (px >= 0 && px < sw && py >= 0 && py < sh) ? canvas[(size_t)py * sw + px] : 0;
        if (px >= clip_left && px < clip_right && py >= clip_top && py < clip_bottom) {
            canvas[(size_t)py * sw + px] = task_icon_base[yy * 32 + xx];
        }
    }
    int extent = (32 * scaled + 128) / 256;
    for (int yy = 0; yy < extent; yy++) for (int xx = 0; xx < extent; xx++) {
        int sx = xx * 32 / extent, sy = yy * 32 / extent;
        int sp = sy * 32 + sx;
        if (task_icon_base[sp] == task_icon_art[sp]) continue;
        int px = x + 16 - extent / 2 + xx;
        int py = y + 16 - extent / 2 + yy;
        if (px >= clip_left && px < clip_right && py >= clip_top && py < clip_bottom) {
            canvas[(size_t)py * sw + px] = task_icon_art[sp];
        }
    }
}

static void taskbar_mark(int cx, int cy) {
    rounded(cx - 16, cy - 16, 32, 32, 11, 0x5266ed);
    rounded(cx - 12, cy - 12, 24, 24, 7, 0x9bc6ff);
    rounded(cx - 9, cy - 9, 18, 18, 5, 0x3652bd);
    rounded(cx - 5, cy - 5, 10, 10, 3, 0xeef7ff);
    rect(cx + 3, cy + 4, 9, 3, 0xeef7ff);
}

#define WALLPAPER_SOURCE_W 256
#define WALLPAPER_SOURCE_H 144

static void regenerate_wallpaper(void) {
    for (int py = 0; py < sh; py++) {
        int v = sh > 1 ? (py * 2048) / (sh-1) : 0;
        int base_r = 9 + v * 7 / 2048;
        int base_g = 24 + v * 32 / 2048;
        int base_b = 56 + v * 52 / 2048;
        for (int px = 0; px < sw; px++) {
            int u = sw > 1 ? (px * 2048) / (sw-1) : 0;
            int glow = 540 - ((u > 1230 ? u-1230 : 1230-u) / 2 +
                              (v > 980 ? v-980 : 980-v));
            if (glow < 0) glow = 0;
            int r = base_r + glow / 16;
            int g = base_g + glow / 4;
            int b = base_b + glow / 3;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            uint32_t color = ((uint32_t)r<<16) | ((uint32_t)g<<8) | (uint32_t)b;
            int curve = 1050 + ((v-940)*(v-940))/2350;
            int ribbon = u-curve;
            if (ribbon >= -110 && ribbon <= 155) {
                int alpha = ribbon < 0 ? 170 + ribbon : 210-ribbon;
                if (alpha > 0) color = blend(color,0x7cd7f5,alpha);
            }
            int beam = u - (290 + v*3/5);
            if (beam >= -11 && beam <= 11) {
                int alpha = 190-(beam<0?-beam:beam)*12;
                if (alpha>0) color=blend(color,0x80edff,alpha);
            }
            int bend = u-(1610-(v-1150)*(v-1150)/2100);
            if (bend >= -16 && bend <= 16) {
                int alpha=130-(bend<0?-bend:bend)*7;
                if (alpha>0) color=blend(color,0xa7e8ff,alpha);
            }
            if (wallpaper == 1) color = blend(color,0x105b8e,38);
            else if (wallpaper == 2) color = blend(color,0x713b8f,48);
            if (NIGHT) color = blend(color,0x050b16,64);
            canvas[(size_t)py*sw+px]=color;
        }
    }
}

static void draw_wallpaper(void) {
    if (cached_wallpaper != wallpaper || cached_wallpaper_night != NIGHT) {
        int old_l = clip_left, old_t = clip_top, old_r = clip_right, old_b = clip_bottom;
        clip_full();
        regenerate_wallpaper();
        for (size_t k = 0; k < (size_t)sw * sh; k++) desktop_cache[k] = canvas[k];
        clip_left = old_l; clip_top = old_t; clip_right = old_r; clip_bottom = old_b;
        cached_wallpaper = wallpaper;
        cached_wallpaper_night = NIGHT;
        desktop_regenerations++;
    }
    for (int y = clip_top; y < clip_bottom; y++) {
        size_t offset = (size_t)y * sw + clip_left;
        for (int x = clip_left; x < clip_right; x++) canvas[offset++] = desktop_cache[(size_t)y * sw + x];
    }
}

static void draw_desktop(void) {
    draw_wallpaper();
    for (int k = 0; k < 8; k++) {
        int x, y, w, h;
        desktop_slot_bounds(k, &x, &y, &w, &h);
        if (y + h >= sh - 52) continue;
        int id = desk_shortcuts[k];
        int hover = desktop_hover == k;
        int focus = desktop_focus == k;
        if (hover || focus) {
            round_capture(x + 2, y + 1, w - 6, h - 8, 12);
            frosted(x + 2, y + 1, w - 6, h - 8, hover ? (NIGHT ? 0x244768 : 0xe6f3ff) : (NIGHT ? 0x2d5171 : 0xf0f8ff), hover ? 102 : 118, hover ? 74 : 60);
            if (hover) glass(x + 5, y + 4, w - 12, 16, 0xffffff, 34);
            round_finish(x + 2, y + 1, w - 6, h - 8, 12);
        }
        draw_icon(x + 20, y + 4, id);
        int tw = size(app_name[id]) * 8;
        if (hover || focus) {
            rounded(x + (w - tw - 14) / 2, y + h - 25, tw + 14, 19, 8, hover ? (NIGHT ? 0x2b4763 : 0xcde8fb) : (NIGHT ? 0x375977 : 0xe1f2ff));
        }
        text(x + 40 - tw / 2 + 1, y + 54, app_name[id], 0x162746);
        text(x + 40 - tw / 2, y + 53, app_name[id], WHITE);
        if (focus) rounded(x + 21, y + h - 9, 38, 3, 1, 0xbdefff);
    }
    if (sw >= 600 && sh >= 480) {
        text(sw - 166, sh - 105, "quartzie", 0xd5eeff);
        rect(sw - 148, sh - 84, 69, 1, 0x81ceee);
    }
    int items[APPS];
    int count = taskbar_items(items);
    int left = taskbar_left(count);
    int island_left = left - 13;
    int island_width = count * taskbar_step() + 16;
    if (island_left < 74) { island_width -= 74 - island_left; island_left = 74; }
    if (island_left + island_width > sw - 192) island_width = sw - 192 - island_left;
    soft_shadow(island_left, sh - 59, island_width, 52, 20, 5);
    round_capture(island_left, sh - 59, island_width, 52, 20);
    frosted(island_left, sh - 59, island_width, 52, NIGHT ? 0x111c32 : 0xf1f6ff, NIGHT ? 118 : 138, NIGHT ? 74 : 62);
    glass(island_left + 2, sh - 58, island_width - 4, 1, 0xffffff, 122);
    glass(island_left + 2, sh - 9, island_width - 4, 1, NIGHT ? 0x7396ce : 0xa3bdea, 106);
    round_finish(island_left, sh - 59, island_width, 52, 20);
    soft_shadow(11, sh - 57, 53, 49, 18, 4);
    round_capture(11, sh - 57, 53, 49, 18);
    frosted(11, sh - 57, 53, 49, NIGHT ? 0x10243c : 0xdceeff, 132, 66);
    glass(12, sh - 56, 51, 1, 0xffffff, 122);
    round_finish(11, sh - 57, 53, 49, 18);
    taskbar_mark(37, sh - 32);
    soft_shadow(sw - 180, sh - 57, 169, 49, 17, 4);
    round_capture(sw - 180, sh - 57, 169, 49, 17);
    frosted(sw - 180, sh - 57, 169, 49, NIGHT ? 0x111c32 : 0xf1f6ff, 132, 66);
    glass(sw - 179, sh - 56, 167, 1, 0xffffff, 108);
    round_finish(sw - 180, sh - 57, 169, 49, 17);
    for (int n = 0; n < count; n++) {
        int id = items[n];
        int x = left + n * taskbar_step();
        int active = active_window() == id && !windows[id].minimized;
        int running = windows[id].opened;
        int hovered = task_hover == id;
        if (active || hovered) {
            round_capture(x + 2, sh - 53, taskbar_width() - 4, 39, 10);
            frosted(x + 2, sh - 53, taskbar_width() - 4, 39,
                active ? blend(BLUE, NIGHT ? 0x0b1727 : 0xffffff, NIGHT ? 108 : 86) : (NIGHT ? 0x1d2f45 : 0xf5f9fd),
                active ? (NIGHT ? 124 : 138) : 102, active ? 88 : 56);
            glass(x + 5, sh - 51, taskbar_width() - 10, 11, 0xffffff, active ? 64 : 38);
            round_finish(x + 2, sh - 53, taskbar_width() - 4, 39, 10);
        }
        draw_bouncing_icon(x + 7, sh - 49, id);
        if (running) {
            if (active) {
                rounded(x + 10, sh - 11, 26, 3, 1, 0xc5f5ff);
            } else {
                rounded(x + 19, sh - 11, 8, 3, 1, 0x79c2f4);
            }
        }
    }
    if (sw >= 700) {
        int bx = sw - 171;
        if (notification_open || do_not_disturb) rounded(bx - 3, sh - 45, 39, 39, 10, NIGHT ? 0x254968 : 0x5486a8);
        circle(bx + 17, sh - 29, 8, 0xb6deee);
        rounded(bx + 11, sh - 23, 13, 3, 1, 0xffffff);
        circle(bx + 17, sh - 18, 2, 0xffffff);
        if (notification_unread && !do_not_disturb) {
            circle(bx + 29, sh - 40, 8, 0xe07760);
            char n[4]; number(n, (uint32_t)notification_unread);
            text(bx + 25, sh - 47, n, WHITE);
        }
    }
    if (sw >= 520) {
        text(sw - 111, sh - 44, clock_time, WHITE);
        text(sw - 111, sh - 25, clock_date + 5, NIGHT ? 0xc6dff9 : 0x5c7c9d);
    }
}

static void log_write(const char *value) {
    if (log_count == 32) {
        for (int i = 1; i < 32; i++) copy(log_lines[i - 1], log_lines[i], 72);
        log_count = 31;
    }
    copy(log_lines[log_count++], value, 72);
    dirty = 1;
}

static void print_info(void) {
    log_write("help ver ls cat clear mem cpu date");
    log_write("notes files clock calc sketch theme");
    log_write("calendar windows echo reboot halt");
    log_write("tasklist handles disk save");
    log_write("screen snapleft snapright");
    log_write("context resize runpe peinfo compat");
}

static void reboot_machine(void) {
    for (int i = 0; i < 100000; i++) if (!(in8(0x64) & 2)) break;
    out8(0x64, 0xfe);
    for (;;) __asm__ volatile("cli; hlt");
}

static void out16(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" :: "a"(value), "Nd"(port));
}

static void halt_machine(void) {
    out16(0x604, 0x2000);
    out16(0xb004, 0x2000);
    out16(0x4004, 0x3400);
    for (;;) __asm__ volatile("cli; hlt");
}

static void snap_window(int app, int side);

static void execute_command(void) {
    char linebuf[120] = "> ";
    append(linebuf, console_input, 120);
    log_write(linebuf);
    if (!console_len) return;
    if (same(console_input, "help")) print_info();
    else if (same(console_input, "ver")) log_write("Quartzie 0.31  |  PE32 executable loader  |  i386");
    else if (same(console_input, "clear")) log_count = 0;
    else if (same(console_input, "context")) log_write("Right click on desktop");
    else if (same(console_input, "dark")) { select_theme(3); log_write("Dark Glass enabled"); }
    else if (same(console_input, "light")) { select_theme(previous_theme); log_write("Light Glass enabled"); }
    else if (same(console_input, "quiet")) { toggle_quiet(); log_write(do_not_disturb ? "Quiet mode on" : "Quiet mode off"); }
    else if (same(console_input, "resize")) log_write("Drag window lower-right corner");
    else if (same(console_input, "quick")) log_write("Clock opens Quick Panel");
    else if (same(console_input, "notify")) { notify("Notification from Console"); log_write("Notification delivered"); }
    else if (same(console_input, "ls")) {
        log_write("/ram/readme.txt");
        log_write("/ram/boot.log");
        log_write("/ram/notes.txt");
    } else if (same(console_input, "cat readme.txt")) log_write("Quartzie 0.31; PE32/CUI; .NET/GTK not yet available");
    else if (same(console_input, "cat boot.log")) log_write("El Torito -> kernel -> protected mode -> UI");
    else if (same(console_input, "cat notes.txt")) {
        char one[72] = "";
        int i = 0;
        while (notes[i] && notes[i] != '\n' && i < 65) { one[i] = notes[i]; i++; }
        one[i] = 0;
        log_write(one);
    } else if (same(console_input, "disk")) log_write(storage_ready ? "D: QuartzieFS on ATA primary master" : "No formatted writable disk detected");
    else if (same(console_input, "save")) { notes_commit(); log_write(storage_ready && !notes_dirty ? "Notes saved on D:" : "No writable storage"); }
    else if (same(console_input, "mem")) log_write("Physical canvas: 0x00400000");
    else if (same(console_input, "handles")) {
        char line[72] = "Kernel handles in use: ", n[12];
        number(n, (uint32_t)cm_handle_count()); append(line, n, 72);
        append(line, " / NT: ", 72);
        number(n, (uint32_t)nt_open_handle_count()); append(line, n, 72);
        log_write(line);
    }
    else if (same(console_input, "compat")) {
        log_write("PE32 x86/CUI: limited support via runpe <name>");
        log_write("KERNEL32: GetPID GetTickCount ExitProcess");
        log_write(".NET 10 / GTK4 / Win32 GUI: not supported yet");
    }
    else if (same(console_input, "peinfo")) {
        char line[72] = "PE32 launches: ", n[12];
        number(n, cm_pe_launches); append(line,n,72); log_write(line);
        line[0]=0; append(line,"PE32 completed: ",72);
        number(n,cm_pe_completed); append(line,n,72); log_write(line);
        if (cm_pe_completed) {
            line[0]=0; append(line,"Last exit code: ",72);
            number(n,cm_pe_last_exit); append(line,n,72); log_write(line);
        }
        log_write(cm_pe_error(pe_last_launch_status));
    }
    else if (begins(console_input, "runpe ")) {
        if (!storage_ready) log_write("Attach a writable QuartzieFS disk first");
        else {
            int n=storage_load(console_input+6,(char *)pe_file_cache,sizeof(pe_file_cache));
            if (n<0) log_write("Executable not found on D: or failed checksum");
            else {
                pe_last_launch_status=cm_proc_launch_pe(pe_file_cache,(uint32_t)n);
                log_write(cm_pe_error(pe_last_launch_status));
            }
        }
    }
    else if (same(console_input, "tasklist")) {
        for (int i = 0; i < APPS; i++) if (cm_tasks[i].pid) {
            char line[72] = "PID ", n[12];
            number(n, cm_tasks[i].pid); append(line, n, 72);
            append(line, "  ", 72); append(line, app_name[i], 72);
            append(line, cm_tasks[i].state == CM_TASK_RUNNING ? "  Running" : cm_tasks[i].state == CM_TASK_MINIMIZED ? "  Minimized" : "  Stopped", 72);
            log_write(line);
        }
    }
#ifdef __i386__
    else if (same(console_input, "useron")) {
        if (!cm_userspace_ready) {
            cm_userspace_ready = cm_proc_initialize();
            log_write(cm_userspace_ready ? "Protected workers enabled (experimental)" : "Protected workers unavailable");
        } else log_write("Protected workers already enabled");
    }
    else if (same(console_input, "useroff")) {
        cm_userspace_ready = 0;
        log_write("Protected worker scheduling paused");
    }
    else if (same(console_input, "userstat")) {
        char line[72] = "Protected ring 3 tasks: ", n[12];
        number(n, cm_proc_active_tasks()); append(line, n, 72);
        append(line, " / slices ", 72);
        number(n, cm_user_slices); append(line, n, 72);
        log_write(line);
        line[0] = 0; append(line, "Preemptions ", 72);
        number(n, cm_preemptions); append(line, n, 72);
        append(line, " / syscalls ", 72);
        number(n, cm_user_syscalls); append(line, n, 72);
        log_write(line);
        for (int i = 0; i < CM_USER_COUNT; i++) {
            line[0] = 0;
            append(line, "PID ", 72);
            number(n, cm_user_tasks[i].pid); append(line, n, 72);
            append(line, "  counter ", 72);
            number(n, cm_proc_counter(i)); append(line, n, 72);
            append(line, cm_user_tasks[i].state == CM_THREAD_READY ? " running" : " faulted", 72);
            log_write(line);
        }
    }
    else if (same(console_input, "faulttest")) {
        log_write(cm_proc_fault_demo() ? "Ring 3 privilege fault queued" : "Privilege fault test unavailable");
    }
#endif
    else if (same(console_input, "renderstat")) {
        char line[72] = "Compositor frames (full / partial / cursor): ", n[12];
        number(n, full_frames); append(line, n, 72); append(line, " / ", 72);
        number(n, damage_frames); append(line, n, 72); append(line, " / ", 72);
        number(n, pointer_frames); append(line, n, 72); log_write(line);
        line[0] = 0; append(line, "Pixels sent to display: ", 72);
        number(n, presented_pixels); append(line, n, 72); log_write(line);
        line[0] = 0; append(line, "Rounded-layer errors: ", 72);
        number(n, round_layer_errors); append(line, n, 72); log_write(line);
    }
    else if (same(console_input, "cpu")) log_write(cpu_vendor);
    else if (same(console_input, "date")) { log_write(clock_date); log_write(clock_time); }
    else if (same(console_input, "screen")) {
        char mode[72] = "Display: ";
        char n[12];
        number(n, (uint32_t)sw); append(mode, n, 72);
        append(mode, "x", 72);
        number(n, (uint32_t)sh); append(mode, n, 72);
        append(mode, boot_bpp == 32 ? " 32-bit" : " 8-bit", 72);
        log_write(mode);
    }
    else if (same(console_input, "snapleft") || same(console_input, "snapright")) {
        int a = active_window();
        if (a == CONSOLE) {
            int best = -1;
            for (int i = 0; i < APPS; i++) {
                if (i != CONSOLE && windows[i].opened && !windows[i].minimized && (best < 0 || windows[i].order > windows[best].order)) best = i;
            }
            a = best;
        }
        if (a >= 0) snap_window(a, same(console_input, "snapleft") ? -1 : 1);
        else log_write("No window to snap");
    }
    else if (same(console_input, "clock")) open_window(CLOCK);
    else if (same(console_input, "calc")) open_window(CALC);
    else if (same(console_input, "sketch") || same(console_input, "pinta")) open_window(SKETCH);
    else if (same(console_input, "theme")) open_window(APPEAR);
    else if (same(console_input, "calendar")) open_window(CALENDAR);
    else if (same(console_input, "windows")) open_window(WINDOWS_APP);
    else if (same(console_input, "notes")) open_window(NOTES);
    else if (same(console_input, "files")) open_window(FILES);
    else if (same(console_input, "reboot")) reboot_machine();
    else if (same(console_input, "halt")) halt_machine();
    else if (begins(console_input, "echo ")) log_write(console_input + 5);
    else log_write("Unknown command. Try help.");
    console_input[0] = 0;
    console_len = 0;
}

static int active_window(void) {
    int best = -1;
    int order = -1;
    for (int i = 0; i < APPS; i++) {
        if (windows[i].opened && !windows[i].minimized && windows[i].order > order) {
            best = i;
            order = windows[i].order;
        }
    }
    return best;
}

static void raise_window(int app) {
    if (app < 0 || app >= APPS) return;
    if (next_order >= 48) {
        int ranks[APPS];
        for (int i = 0; i < APPS; i++) {
            int rank = 1;
            for (int j = 0; j < APPS; j++) {
                if (j == i) continue;
                if (windows[j].order < windows[i].order ||
                    (windows[j].order == windows[i].order && j < i)) rank++;
            }
            ranks[i] = rank;
        }
        for (int i = 0; i < APPS; i++) windows[i].order = ranks[i];
        next_order = APPS;
    }
    windows[app].order = ++next_order;
}

static void snap_window(int app, int side) {
    Window *w = &windows[app];
    if (!w->opened) return;
    int from_x = w->x, from_y = w->y, from_w = w->w, from_h = w->h;
    if (!w->maximized && snap_side[app] == side) {
        w->x = w->restore_x;
        w->y = w->restore_y;
        w->w = w->restore_w;
        w->h = w->restore_h;
        snap_side[app] = 0;
    } else {
        if (!w->maximized && !snap_side[app]) {
            w->restore_x = w->x;
            w->restore_y = w->y;
            w->restore_w = w->w;
            w->restore_h = w->h;
        }
        w->maximized = 0;
        snap_side[app] = side;
        w->x = side < 0 ? 4 : sw / 2 + 2;
        w->y = 4;
        w->w = sw / 2 - 6;
        w->h = sh - 59;
    }
    w->minimized = 0;
    raise_window(app);
    begin_window_animation(app, from_x, from_y, from_w, from_h, WINDOW_RESIZE_STEPS, 0);
    dirty = 1;
}

static void toggle_maximize(int app) {
    Window *w = &windows[app];
    int from_x = w->x, from_y = w->y, from_w = w->w, from_h = w->h;
    if (!w->maximized) {
        if (!snap_side[app]) {
            w->restore_x = w->x; w->restore_y = w->y;
            w->restore_w = w->w; w->restore_h = w->h;
        }
        snap_side[app] = 0;
        w->x = 4; w->y = 4; w->w = sw - 8; w->h = sh - 59;
        w->maximized = 1;
    } else {
        w->x = w->restore_x; w->y = w->restore_y;
        w->w = w->restore_w; w->h = w->restore_h;
        w->maximized = 0;
        snap_side[app] = 0;
    }
    begin_window_animation(app, from_x, from_y, from_w, from_h, WINDOW_RESIZE_STEPS, 0);
    dirty = 1;
}

static int app_from_command(const char *name) {
    if (same_fold(name, "files") || same_fold(name, "explorer")) return FILES;
    if (same_fold(name, "notes") || same_fold(name, "notepad")) return NOTES;
    if (same_fold(name, "console") || same_fold(name, "cmd") || same_fold(name, "terminal")) return CONSOLE;
    if (same_fold(name, "system")) return SYSTEM;
    if (same_fold(name, "clock")) return CLOCK;
    if (same_fold(name, "calculator") || same_fold(name, "calc")) return CALC;
    if (same_fold(name, "appearance") || same_fold(name, "settings")) return APPEAR;
    if (same_fold(name, "sketch") || same_fold(name, "paint") || same_fold(name, "pinta")) return SKETCH;
    if (same_fold(name, "calendar")) return CALENDAR;
    if (same_fold(name, "windows")) return WINDOWS_APP;
    return -1;
}

static void run_command(void) {
    if (!run_len) { run_open = 0; dirty = 1; return; }
    if (same_fold(run_input, "desktop")) { run_open = 0; toggle_desktop(); return; }
    if (same_fold(run_input, "dark")) { select_theme(3); run_open = 0; return; }
    if (same_fold(run_input, "light")) { select_theme(previous_theme); run_open = 0; return; }
    if (same_fold(run_input, "quiet")) { toggle_quiet(); run_open = 0; return; }
    if (same_fold(run_input, "reboot")) reboot_machine();
    if (same_fold(run_input, "shutdown") || same_fold(run_input, "halt")) halt_machine();
    int app = app_from_command(run_input);
    if (app >= 0) { run_open = 0; open_window(app); return; }
    notify("Run command not found");
    run_open = 0;
    dirty = 1;
}

static void toggle_desktop(void) {
    if (!desktop_hidden) {
        int found = 0;
        for (int i = 0; i < APPS; i++) {
            desktop_saved[i] = windows[i].opened && !windows[i].minimized;
            if (desktop_saved[i]) { close_window(i, 1); found = 1; }
        }
        desktop_hidden = found;
    } else {
        for (int i = 0; i < APPS; i++) if (desktop_saved[i] && windows[i].opened) open_window(i);
        desktop_hidden = 0;
    }
    start_open = 0;
    context_open = 0;
    quick_open = 0;
    notification_open = 0;
    dirty = 1;
}

static void dismiss_popup_layers(void) {
    start_open = 0;
    context_open = 0;
    quick_open = 0;
    notification_open = 0;
    run_open = 0;
    for (int i = PANEL_START; i <= PANEL_NOTIFICATIONS; i++) {
        panel_step[i] = 0;
        panel_goal[i] = 0;
    }
    panel_step[PANEL_RUN] = 0;
    panel_goal[PANEL_RUN] = 0;
    hover_visible = 0;
    dirty = 1;
}

static void open_window(int app) {
    if (app < 0 || app >= APPS) return;
    if (app != FILES) file_search_focus=0;
    if (app != APPEAR) settings_search_focus=0;
    desktop_focus = -1;
    desktop_hidden = 0;
    int was_active = active_window() == app;
    int was_minimized = windows[app].minimized;
    int was_closed = !windows[app].opened;
    int first = was_closed || was_minimized;
    if (was_active && !first && !anim_exit[app]) {
        dismiss_popup_layers();
        return;
    }
    int new_object = !cm_window_handle(app);
    uint32_t handle = cm_window_open(app);
    if (handle && new_object) cm_post(handle, CM_MSG_CREATE, app, 0);
    if (handle) cm_post(handle, CM_MSG_ACTIVATE, app, 0);
    windows[app].opened = 1;
    windows[app].minimized = 0;
    raise_window(app);
    dismiss_popup_layers();
    if (first) {
        anim_ticks[app] = 0;
        anim_visible[app] = 0;
        anim_exit[app] = 0;
    }
    if (was_closed) icon_kick(app, 1);
    else if (was_minimized || !was_active) icon_kick(app, 3);
    dirty = 1;
}

static int date_number(const char *s) { return (s[0]-'0') * 10 + s[1]-'0'; }

static int month_days(int year, int month) {
    static const int lengths[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2 && (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0)) return 29;
    return lengths[month - 1];
}

static void calendar_month(int *year, int *month) {
    int y = 2000 + date_number(clock_date + 2);
    int m = date_number(clock_date + 5) + calendar_shift;
    while (m < 1) { m += 12; y--; }
    while (m > 12) { m -= 12; y++; }
    *year = y;
    *month = m;
}

static int calendar_weekday(int year, int month) {
    static const int offsets[12] = {0,3,2,5,0,3,5,1,4,6,2,4};
    if (month < 3) year--;
    int d = (year + year/4 - year/100 + year/400 + offsets[month-1] + 1) % 7;
    return (d + 6) % 7;
}

static void snapshot_sketch(void) {
    for (int yy = 0; yy < SKETCH_H; yy++)
        for (int xx = 0; xx < SKETCH_W; xx++) sketch_undo[yy][xx] = sketch_pixels[yy][xx];
    sketch_has_undo = 1;
    sketch_has_redo = 0;
}

static void undo_sketch(void) {
    if (!sketch_has_undo) return;
    pinta_copy(&sketch_redo[0][0], &sketch_pixels[0][0], SKETCH_W * SKETCH_H);
    pinta_copy(&sketch_pixels[0][0], &sketch_undo[0][0], SKETCH_W * SKETCH_H);
    sketch_has_undo = 0;
    sketch_has_redo = 1;
    sketch_dirty = 1;
    dirty = 1;
}

static void redo_sketch(void) {
    if (!sketch_has_redo) return;
    pinta_copy(&sketch_undo[0][0], &sketch_pixels[0][0], SKETCH_W * SKETCH_H);
    pinta_copy(&sketch_pixels[0][0], &sketch_redo[0][0], SKETCH_W * SKETCH_H);
    sketch_has_undo = 1;
    sketch_has_redo = 0;
    sketch_dirty = 1;
    dirty = 1;
}

static int pinta_save_document(void) {
    if (!storage_ready) { sketch_saved = 0; return 0; }
    const uint8_t *bytes = &sketch_pixels[0][0];
    for (int k = 0; k < 7; k++) {
        char name[15] = "painting0.cmx";
        name[8] = (char)('0' + k);
        char path[48] = "\\??\\D:\\";
        append(path, name, sizeof(path));
        uint32_t offset = (uint32_t)k * 4096u;
        uint32_t count = SKETCH_W * SKETCH_H - offset;
        if (count > 4096u) count = 4096u;
        IO_STATUS_BLOCK io = {0};
        HANDLE file = 0;
        NTSTATUS status = nt_open_ascii(&file, path, FILE_WRITE_DATA, FILE_OVERWRITE_IF);
        if (NT_SUCCESS(status)) status = NtWriteFile(file, 0, NULL, NULL, &io, bytes + offset, count, NULL, NULL);
        if (NT_SUCCESS(status)) status = NtFlushBuffersFile(file, &io);
        if (file) {
            if (NT_SUCCESS(status)) status = NtClose(file);
            else nt_file_abort(file);
        }
        if (!NT_SUCCESS(status)) { sketch_saved = 0; return 0; }
    }
    sketch_dirty = 0;
    sketch_saved = 1;
    dirty = 1;
    return 1;
}

static int pinta_load_document(void) {
    if (!storage_ready) return 0;
    static uint8_t loaded[SKETCH_W * SKETCH_H];
    for (int k = 0; k < 7; k++) {
        char name[15] = "painting0.cmx";
        name[8] = (char)('0' + k);
        char path[48] = "\\??\\D:\\";
        append(path, name, sizeof(path));
        IO_STATUS_BLOCK io = {0};
        HANDLE file = 0;
        uint32_t offset = (uint32_t)k * 4096u;
        uint32_t count = SKETCH_W * SKETCH_H - offset;
        if (count > 4096u) count = 4096u;
        NTSTATUS status = nt_open_ascii(&file, path, FILE_READ_DATA, FILE_OPEN);
        if (NT_SUCCESS(status)) status = NtReadFile(file, 0, NULL, NULL, &io, loaded + offset, count, NULL, NULL);
        if (file) NtClose(file);
        if (!NT_SUCCESS(status) || io.Information != count) return 0;
    }
    pinta_copy(&sketch_pixels[0][0], loaded, SKETCH_W * SKETCH_H);
    sketch_has_undo = 0;
    sketch_has_redo = 0;
    sketch_dirty = 0;
    sketch_saved = 1;
    dirty = 1;
    return 1;
}

static int files_nav_width(int cw) { return cw >= 900 ? 188 : 156; }
static int files_preview_width(int cw) { return file_details ? (cw >= 920 ? 222 : cw >= 760 ? 178 : 0) : 0; }
static int files_list_left(int x, int cw) { return x + files_nav_width(cw) + 28; }
static int files_list_width(int cw) { return cw - files_nav_width(cw) - files_preview_width(cw) - 48; }
static int files_row_top(int y) { return y + 134; }
static int files_row_height(void) { return file_view ? 66 : 37; }
static int pe_file_suffix(const char *name) {
    int len=size(name);
    if (len<5) return 0;
    const char *extension=name+len-4;
    return (extension[0]=='.' && (extension[1]=='e'||extension[1]=='E') &&
            (extension[2]=='x'||extension[2]=='X') && (extension[3]=='e'||extension[3]=='E'));
}
static void files_restore(void) {
    file_created = 0;
    if (!storage_ready) return;
    char name[24];
    for (int pass=0;pass<2;pass++) {
        for (int k=0;k<STORAGE_SLOTS && file_created<3;k++) {
            if (!cm_file_list(k,name)) break;
            if (same(name,"notes.txt") || same(name,"system.reg") || pe_file_suffix(name)!=(!pass)) continue;
            copy(file_extra_names[file_created++],name,24);
        }
    }
}

static int files_count(void) { return FILE_COUNT + file_created; }
static const char *files_name(int id) { return id < FILE_COUNT ? file_names[id] : file_extra_names[id - FILE_COUNT]; }
static const char *files_type(int id) { return id < FILE_COUNT ? file_types[id] : pe_file_suffix(files_name(id)) ? "Windows executable" : "Text Document"; }
static const char *files_size(int id) { return id < FILE_COUNT ? file_sizes[id] : pe_file_suffix(files_name(id)) ? "2 KB" : "0 KB"; }
static const char *files_date(int id) { return id < FILE_COUNT ? file_dates[id] : "2026-10-14 21:00"; }
static int files_filter(int id) {
    if (file_location == 1 && id > 2) return 0;
    if (file_location == 2 && id != 3) return 0;
    if (file_location == 3 && id != 0 && id != 2 && id < FILE_COUNT) return 0;
    if (file_location == 4 && id != 1) return 0;
    if (file_location == 5 && id != 3) return 0;
    if (file_search_len) {
        const char *name = files_name(id);
        for (int start = 0; name[start]; start++) {
            int pos = 0;
            while (pos < file_search_len && name[start + pos]) {
                char a = name[start + pos], b = file_search[pos];
                if (a >= 'A' && a <= 'Z') a += 32;
                if (b >= 'A' && b <= 'Z') b += 32;
                if (a != b) break;
                pos++;
            }
            if (pos == file_search_len) return 1;
        }
        return 0;
    }
    return 1;
}
static int files_item(int row) {
    int count = files_count(), n = 0;
    for (int k = 0; k < count; k++) {
        int id = file_sort == 1 ? count - k - 1 : k;
        if (files_filter(id)) {
            if (n++ == row) return id;
        }
    }
    return -1;
}
static int files_visible_count(void) {
    int n = 0;
    for (int i = 0; i < files_count(); i++) if (files_filter(i)) n++;
    return n;
}
static int files_row_hit(const Window *w, int px, int py) {
    int x = w->x + 2, y = w->y + 34, cw = w->w - 4;
    int row_x = files_list_left(x, cw);
    int row_y = files_row_top(y), row_h = files_row_height();
    if (!inside(px, py, row_x, row_y, files_list_width(cw), row_h * files_count())) return -1;
    return files_item((py - row_y) / row_h);
}
static void files_open_selected(void) {
    if (selected_file >= FILE_COUNT && pe_file_suffix(files_name(selected_file))) {
        int n=storage_ready ? storage_load(files_name(selected_file),(char *)pe_file_cache,sizeof(pe_file_cache)) : -1;
        if (n<0) log_write("PE32 executable failed to load from D:");
        else {
            pe_last_launch_status=cm_proc_launch_pe(pe_file_cache,(uint32_t)n);
            log_write(cm_pe_error(pe_last_launch_status));
            if (pe_last_launch_status!=CM_PE_OK) notify("Windows app requires unsupported API/runtime");
        }
    }
    else if (selected_file == 2 || selected_file >= FILE_COUNT) {
        if (notes_select(files_name(selected_file))) open_window(NOTES);
        else notify("Save current document before switching");
    }
    else if (selected_file == 3) open_window(APPEAR);
    else if (selected_file == 4) open_window(CALENDAR);
    else dirty = 1;
}
static int files_action(const Window *w, int px, int py) {
    int x = w->x + 2, y = w->y + 34, cw = w->w - 4;
    int nw = files_nav_width(cw);
    if (inside(px, py, x + 24, y + 15, 60, 24)) {
        if (file_created < 3) {
            const char *name = file_extra_names[file_created];
            HANDLE handle = 0;
            char path[48] = "\\??\\D:\\";
            append(path, name, sizeof(path));
            NTSTATUS status = storage_ready ? nt_open_ascii(&handle, path, FILE_WRITE_DATA, FILE_OVERWRITE_IF) : STATUS_SUCCESS;
            if (handle && NT_SUCCESS(status)) status = NtClose(handle);
            if (NT_SUCCESS(status)) {
                selected_file = FILE_COUNT + file_created++;
                file_location = 6; file_search_len = 0; file_search[0] = 0;
            } else { if (handle) nt_file_abort(handle); notify("Disk write failed"); }
        }
        file_search_focus = 0; return 1;
    }
    if (inside(px, py, x + 92, y + 15, 60, 24)) { file_sort = !file_sort; file_search_focus = 0; return 1; }
    if (inside(px, py, x + 160, y + 15, 60, 24)) { file_view = !file_view; file_search_focus = 0; return 1; }
    if (inside(px, py, x + 218, y + 15, 38, 24)) { file_details = !file_details; file_search_focus = 0; return 1; }
    if (inside(px, py, x + cw - 168, y + 52, 154, 30)) { file_search_focus = 1; return 1; }
    if (inside(px, py, x + 14, y + 52, cw - 212, 30)) { file_location = 6; file_search_focus = 0; return 1; }
    for (int k = 0; k < 7; k++) {
        if (inside(px, py, x + 19, y + 128 + k * 36, nw - 14, 30)) {
            file_location = k; file_search_focus = 0; return 1;
        }
    }
    int id = files_row_hit(w, px, py);
    if (id >= 0) { selected_file = id; file_search_focus = 0; return 1; }
    file_search_focus = 0;
    return 0;
}
static int settings_action(const Window *w, int px, int py) {
    int x = w->x + 2, y = w->y + 34, cw = w->w - 4;
    int nw = cw >= 850 ? 210 : 164;
    int mx = x + nw + 34, mw = cw - nw - 55;
    if (inside(px,py,x+16,y+56,nw-8,34)) { settings_search_focus = 1; return 1; }
    settings_search_focus = 0;
    for (int k = 0; k < 5; k++) {
        if (inside(px,py,x+18,y+119+k*47,nw-10,39)) { settings_page = k; return 1; }
    }
    if (settings_page == 1) {
        int compact = w->h - 36 < 590;
        int mode_y = compact ? 70 : 93;
        int theme_y = compact ? 165 : 217;
        int wall_y = compact ? 250 : 328;
        int glass_y = compact ? 310 : 407;
        int accent_y = compact ? 370 : 461;
        if (inside(px,py,mx+mw-151,y+mode_y+18,128,36)) { toggle_dark_aero(); return 1; }
        for (int k = 0; k < 3; k++) {
            if (inside(px,py,mx+k*145,y+theme_y,132,compact ? 52 : 66)) { select_theme(k); return 1; }
            if (inside(px,py,mx+k*145,y+wall_y,132,compact ? 52 : 64)) { wallpaper=k; return 1; }
            if (inside(px,py,mx+155+k*80,y+glass_y,71,32)) { glass_level=k; return 1; }
        }
        for (int k = 0; k < 4; k++) if (inside(px,py,mx+155+k*65,y+accent_y,45,36)) { accent_index=k; return 1; }
    } else if (settings_page == 0) {
        if (inside(px,py,mx+22,y+292,58,34)) { volume_level = volume_level >= 10 ? volume_level-10 : 0; volume_muted=0; return 1; }
        if (inside(px,py,mx+92,y+292,58,34)) { volume_level = volume_level <= 90 ? volume_level+10 : 100; volume_muted=0; return 1; }
        if (inside(px,py,mx+163,y+292,112,34)) { volume_muted=!volume_muted; return 1; }
    } else if (settings_page == 2) {
        for (int k = 0; k < APPS; k++) {
            int yy=y+95+k*45;
            if (yy+40 >= y+w->h-36-14) break;
            if (inside(px,py,mx+mw-89,yy+6,73,28)) { open_window(k); return 1; }
        }
    } else if (settings_page == 3) {
        if (inside(px,py,mx+mw-103,y+117,80,33)) { toggle_quiet(); return 1; }
        if (inside(px,py,mx+mw-142,y+232,121,33)) { notification_count=0; notification_unread=0; return 1; }
    }
    return 0;
}
static void settings_search_apply(void) {
    if (same_fold(settings_search,"system") || same_fold(settings_search,"display") || same_fold(settings_search,"volume") || same_fold(settings_search,"sound")) settings_page=0;
    else if (same_fold(settings_search,"theme") || same_fold(settings_search,"dark") || same_fold(settings_search,"wallpaper") || same_fold(settings_search,"personalization") || same_fold(settings_search,"accent")) settings_page=1;
    else if (same_fold(settings_search,"apps")) settings_page=2;
    else if (same_fold(settings_search,"notifications") || same_fold(settings_search,"quiet")) settings_page=3;
    else if (same_fold(settings_search,"about")) settings_page=4;
    settings_search_focus=0;
    dirty=1;
}

static int menu_row_app(int row) {
    int visible = 0;
    for (int i = 0; i < APPS; i++) {
        int match = 1;
        if (menu_query_len) {
            match = 0;
            for (int j = 0; app_name[i][j]; j++) {
                int k = 0;
                while (k < menu_query_len && app_name[i][j+k]) {
                    char a = app_name[i][j+k];
                    char b = menu_query[k];
                    if (a >= 'A' && a <= 'Z') a += 32;
                    if (b >= 'A' && b <= 'Z') b += 32;
                    if (a != b) break;
                    k++;
                }
                if (k == menu_query_len) { match = 1; break; }
            }
        }
        if (match) {
            if (visible == row) return i;
            visible++;
        }
    }
    return -1;
}

static void window_content(int app, const Window *w) {
    int x = w->x + 2;
    int y = w->y + 34;
    int cw = w->w - 4;
    int ch = w->h - 36;
    if (cw < 1 || ch < 1) return;
    int old_l = clip_left, old_t = clip_top, old_r = clip_right, old_b = clip_bottom;
    if (clip_left < x) clip_left = x;
    if (clip_top < y) clip_top = y;
    if (clip_right > x + cw) clip_right = x + cw;
    if (clip_bottom > y + ch) clip_bottom = y + ch;
    if (clip_left >= clip_right || clip_top >= clip_bottom) {
        clip_left = old_l; clip_top = old_t; clip_right = old_r; clip_bottom = old_b;
        return;
    }
    rect(x, y, cw, ch, PAPER);
    if (app == FILES) {
        int nw = files_nav_width(cw), pw = files_preview_width(cw);
        int lx = files_list_left(x, cw), lw = files_list_width(cw);
        int py = files_row_top(y), rh = files_row_height();
        int px = x + cw - pw - 14;
        rounded(x + 14, y + 10, cw - 28, 34, 10, UI_FIELD);
        outline(x + 14, y + 10, cw - 28, 34, UI_EDGE);
        for (int k = 0; k < 4; k++) {
            int bx = x + (k == 3 ? 218 : 24 + k * 68), bw = k == 3 ? 38 : 60;
            rounded(bx, y + 15, bw, 24, 7, k == 0 ? UI_HOVER : UI_CARD);
            text(bx + 12, y + 21, k == 0 ? "New" : k == 1 ? (file_sort ? "Z-A" : "Sort") : k == 2 ? (file_view ? "List" : "View") : "...", INK);
        }
        rounded(x + 14, y + 52, cw - 212, 30, 9, UI_CARD);
        outline(x + 14, y + 52, cw - 212, 30, HAIR);
        text(x + 27, y + 59, storage_ready ? (file_location == 6 ? "This PC  >  Disk (D:)" : "Home  >  Disk (D:)") : (file_location == 6 ? "This PC  >  RAM (R:)" : "Home  >  RAM (R:)"), INK);
        rounded(x + cw - 168, y + 52, 154, 30, 9, UI_FIELD);
        outline(x + cw - 168, y + 52, 154, 30, file_search_focus ? BLUE : UI_EDGE);
        text(x + cw - 158, y + 59, file_search_len ? file_search : (storage_ready ? "Search Disk" : "Search RAM"), file_search_len ? INK : STEEL);
        if (file_search_focus) rect(x + cw - 158 + file_search_len * 8, y + 75, 6, 2, BLUE);
        rounded(x + 12, y + 92, nw, ch - 104, 12, LIGHT);
        text(x + 26, y + 103, "This PC", INK);
        const char *nav[7] = {"Home", "Quick access", "Desktop", "Documents", "Downloads", "Pictures", "RAM (R:)"};
        for (int k = 0; k < 7; k++) {
            int yy = y + 132 + k * 36;
            if (file_location == k) rounded(x + 19, yy - 4, nw - 14, 30, 8, UI_HOVER);
            rounded(x + 27, yy + 5, 13, 13, 3, k == 6 ? 0xe8bd79 : (NIGHT ? 0x42617f : 0x8bb6d6));
            text(x + 47, yy + 2, k == 6 && storage_ready ? "Disk (D:)" : nav[k], file_location == k ? BLUE : INK2);
        }
        if (pw) {
            rounded(px, y + 92, pw, ch - 104, 12, LIGHT);
            outline(px, y + 92, pw, ch - 104, HAIR);
        }
        text(lx + 5, y + 103, "Name", STEEL);
        if (!file_view) {
            text(lx + lw * 42 / 100, y + 103, "Date modified", STEEL);
            text(lx + lw * 72 / 100, y + 103, "Type", STEEL);
            text(lx + lw - 46, y + 103, "Size", STEEL);
        }
        rect(lx, y + 121, lw, 1, HAIR);
        int shown = files_visible_count();
        for (int row = 0; row < shown; row++) {
            int id = files_item(row), yy = py + row * rh;
            if (yy + rh >= y + ch - 49) break;
            if (selected_file == id) rounded(lx, yy, lw, rh - 2, 9, UI_HOVER);
            else if (row & 1) rounded(lx, yy, lw, rh - 2, 9, NIGHT ? 0x1d2c3c : 0xf7fbff);
            rounded(lx + 12, yy + 9, 17, 20, 4, WHITE);
            outline(lx + 12, yy + 9, 17, 20, STEEL);
            rect(lx + 16, yy + 15, 9, 2, HAIR);
            text(lx + 37, yy + (file_view ? 11 : 10), files_name(id), INK);
            if (!file_view) {
                text(lx + lw * 42 / 100, yy + 10, files_date(id), INK2);
                text(lx + lw * 72 / 100, yy + 10, id >= FILE_COUNT && pe_file_suffix(files_name(id)) ? "EXE" : id == 3 ? "Bitmap" : id == 1 ? "Log" : id == 4 ? "Data" : "Text", INK2);
                text(lx + lw - 46, yy + 10, files_size(id), INK2);
            } else {
                text(lx + 37, yy + 34, files_type(id), STEEL);
                text(lx + lw - 64, yy + 11, files_size(id), STEEL);
            }
        }
        if (!shown) text(lx + 20, py + 26, "No files match your search", STEEL);
        rect(lx, y + ch - 43, lw, 1, HAIR);
        char count[12]; number(count, (uint32_t)shown);
        text(lx + 5, y + ch - 32, count, INK2);
        text(lx + 22, y + ch - 32, storage_ready ? "items on D: / persistent" : "items in RAM / temporary", STEEL);
        if (pw) {
            text(px + 17, y + 106, "Details", INK);
            rounded(px + 17, y + 139, pw - 34, 91, 10, UI_CARD);
            rounded(px + 32, y + 157, 35, 43, 5, WHITE);
            outline(px + 32, y + 157, 35, 43, STEEL);
            text(px + 78, y + 151, files_name(selected_file), INK);
            text(px + 78, y + 174, files_type(selected_file), STEEL);
            text(px + 17, y + 251, "Location", STEEL);
            text(px + 17, y + 274, storage_ready ? "Disk (D:)" : "RAM (R:)", INK);
            text(px + 17, y + 309, "Modified", STEEL);
            text(px + 17, y + 333, files_date(selected_file), INK);
            text(px + 17, y + 365, "Size", STEEL);
            text(px + 17, y + 389, files_size(selected_file), INK);
            rect(px + 17, y + 415, pw - 34, 1, HAIR);
            text(px + 17, y + 435, selected_file >= FILE_COUNT && pe_file_suffix(files_name(selected_file)) ? "PE32 console executable" : selected_file == 2 ? "Editable in Notes" : selected_file == 3 ? "Desktop wallpaper" : selected_file == 4 ? "Calendar app data" : (storage_ready ? "Saved document on D:" : "Temporary RAM document"), INK2);
            text(px + 17, y + 458, storage_ready ? "Saved on virtual hard disk" : "Temporary session storage", STEEL);
        }
    } else if (app == NOTES) {
        rect(x, y, cw, 27, LIGHT);
        text(x + 12, y + 6, notes_filename, INK);
        text(x + cw - 168, y + 6, storage_ready ? (notes_save_failed ? "WRITE ERROR" : notes_dirty ? "Not saved  Ctrl+S" : "Disk saved  Ctrl+S") : "RAM only  Ctrl+S", STEEL);
        rect(x, y + 27, cw, 1, HAIR);
        int max_columns = (cw - 30) / 8;
        if (max_columns < 1) max_columns = 1;
        int max_lines = (ch - 44) / 19;
        if (max_lines < 1) max_lines = 1;
        int total_line = 0;
        int col = 0;
        for (int i = 0; i < notes_len; i++) {
            if (notes[i] == '\n' || col >= max_columns) { total_line++; col = 0; }
            if (notes[i] != '\n') col++;
        }
        int skip = total_line >= max_lines ? total_line - max_lines + 1 : 0;
        int line_index = 0;
        col = 0;
        for (int i = 0; i <= notes_len; i++) {
            char c = i == notes_len ? 0 : notes[i];
            if (c == '\n' || col >= max_columns) { line_index++; col = 0; }
            if (c && c != '\n') {
                if (line_index >= skip && line_index - skip < max_lines) {
                    int gx = x + 13 + col * 8, gy = y + 36 + (line_index - skip) * 19;
                    if (notes_select_all) rounded(gx, gy - 1, 8, 17, 1, UI_HOVER);
                    glyph(gx, gy, c, INK, 1);
                }
                col++;
            }
            if (!c && !notes_select_all && line_index >= skip && line_index - skip < max_lines) {
                rect(x + 13 + col * 8, y + 36 + (line_index - skip) * 19 + 14, 7, 2, BLUE);
            }
        }
    } else if (app == CONSOLE) {
        rect(x, y, cw, ch, BLACK);
        rect(x, y, cw, 2, ACTIVE);
        int visible = (ch - 45) / 19;
        if (visible < 1) visible = 1;
        int begin = log_count > visible ? log_count - visible : 0;
        int yy = y + 14;
        for (int i = begin; i < log_count; i++) {
            text(x + 12, yy, log_lines[i], 0xcad7de);
            yy += 19;
        }
        text(x + 12, y + ch - 29, ">", ACTIVE);
        text(x + 28, y + ch - 29, console_input, WHITE);
        rect(x + 28 + console_len * 8, y + ch - 13, 7, 2, WHITE);
    } else if (app == SYSTEM) {
        rect(x, y, cw, 4, BLUE);
        big_text(x + 21, y + 22, "QUARTZIE", INK);
        text(x + 24, y + 62, "RELEASE 0.28", STEEL);
        rect(x + 21, y + 91, cw - 42, 1, HAIR);
        text(x + 24, y + 106, "Kernel", STEEL);
        text(x + 166, y + 106, "Quartzie 0.28", INK);
        text(x + 24, y + 134, "Target", STEEL);
        text(x + 166, y + 134, "i386 protected", INK);
        text(x + 24, y + 162, "Firmware", STEEL);
        text(x + 166, y + 162, "BIOS / CD-ROM", INK);
        text(x + 24, y + 190, "Processor", STEEL);
        text(x + 166, y + 190, cpu_vendor, INK);
        text(x + 24, y + 218, "Display", STEEL);
        char display[48] = "";
        char tmp[12];
        number(tmp, (uint32_t)sw); append(display, tmp, 48);
        append(display, " x ", 48);
        number(tmp, (uint32_t)sh); append(display, tmp, 48);
        append(display, boot_bpp == 32 ? " / VBE" : " / VGA", 48);
        text(x + 166, y + 218, display, INK);
        text(x + 24, y + 246, "Storage", STEEL);
        text(x + 166, y + 246, storage_ready ? "Disk D: / QuartzieFS" : "RAM-only session", INK);
        text(x + 24, y + 274, "Window manager", STEEL);
        text(x + 166, y + 274, "Message-based dispatch", INK);
        text(x + 24, y + 302, "Kernel objects", STEEL);
        char objects[12]; number(objects, (uint32_t)cm_handle_count());
        text(x + 166, y + 302, objects, INK);
        text(x + 24, y + 330, "App tasks", STEEL);
        int tasks = 0; for (int i = 0; i < APPS; i++) if (cm_tasks[i].state) tasks++;
        number(objects, (uint32_t)tasks);
        text(x + 166, y + 330, objects, INK);
        text(x + 24, y + 358, "NT I/O Manager", STEEL);
        text(x + 166, y + 358, nt_driver_ready ? "IRP / QuartzieFS" : "Not initialized", INK);
        text(x + 24, y + 386, "Device namespace", STEEL);
        text(x + 166, y + 386, "\\Device\\QuartzieDisk", INK);
        text(x + 24, y + 414, "Registry", STEEL);
        text(x + 166, y + 414, nt_registry_loaded ? "HKLM / Software / Quartzie" : "Available on D:", INK);
#ifdef __i386__
        if (ch >= 472) {
            text(x + 24, y + 443, "User processes", STEEL);
            text(x + 166, y + 443, cm_proc_enabled() ? "Ring 3 / paged / PIT scheduled" : "PSE unavailable", INK);
        }
#endif
    } else if (app == CLOCK) {
        rect(x, y, cw, 4, BLUE);
        text(x + 22, y + 30, "SYSTEM RTC", STEEL);
        big_text(x + 22, y + 73, clock_time, INK);
        rect(x + 22, y + 127, cw - 44, 1, HAIR);
        big_text(x + 22, y + 156, clock_date, BLUE);
        text(x + 22, y + 230, "CMOS clock", STEEL);
    } else if (app == APPEAR) {
        int nw = cw >= 850 ? 210 : 164;
        int mx = x + nw + 34, mw = cw - nw - 55;
        rect(x + nw + 14, y + 13, 1, ch - 26, HAIR);
        text(x + 25, y + 21, "Settings", INK);
        rounded(x + 16, y + 56, nw - 8, 34, 9, UI_FIELD);
        outline(x + 16, y + 56, nw - 8, 34, settings_search_focus ? BLUE : UI_EDGE);
        text(x + 27, y + 65, settings_search_len ? settings_search : "Find a setting", settings_search_len ? INK : STEEL);
        static const char *pages[5] = {"System", "Personalization", "Apps", "Notifications", "About"};
        for (int k = 0; k < 5; k++) {
            int yy = y + 122 + k * 47;
            if (settings_page == k) rounded(x + 18, yy - 3, nw - 10, 39, 10, UI_HOVER);
            rounded(x + 26, yy + 8, 18, 18, 5, settings_page == k ? BLUE : STEEL);
            text(x + 55, yy + 9, pages[k], settings_page == k ? INK : INK2);
        }
        rect(x + 22, y + ch - 75, nw - 20, 1, HAIR);
        text(x + 27, y + ch - 57, "Quartzie 0.28", STEEL);
        text(x + 27, y + ch - 37, "1920 x 1080", STEEL);
        big_text(mx, y + 18, pages[settings_page], INK);
        if (settings_page == 1) {
            int compact = ch < 590;
            int mode_y = compact ? 70 : 93;
            int theme_y = compact ? 165 : 217;
            int wall_y = compact ? 250 : 328;
            int glass_y = compact ? 310 : 407;
            int accent_y = compact ? 370 : 461;
            text(mx + 2, y + 59, "Theme, backgrounds, colors and transparency", STEEL);
            rounded(mx, y + mode_y, mw, compact ? 61 : 79, 12, UI_CARD);
            text(mx + 19, y + mode_y + 14, "Choose your mode", INK);
            text(mx + 19, y + mode_y + 39, "Modular Glassmorphism", STEEL);
            rounded(mx + mw - 151, y + mode_y + 18, 128, 36, 10, NIGHT ? 0x416586 : 0xd0eafa);
            text(mx + mw - 133, y + mode_y + 27, NIGHT ? "Dark Glass" : "Light Glass", INK);
            text(mx + 2, y + theme_y - 27, "Color theme", INK);
            for (int k = 0; k < 3; k++) {
                int bx = mx + k * 145;
                rounded(bx, y + theme_y, 132, compact ? 52 : 66, 10, (NIGHT ? previous_theme : theme) == k ? UI_HOVER : UI_CARD);
                outline(bx, y + theme_y, 132, compact ? 52 : 66, (NIGHT ? previous_theme : theme) == k ? BLUE : HAIR);
                rounded(bx + 12, y + theme_y + 12, 36, 21, 5, k == 0 ? 0x3774b8 : k == 1 ? 0x8eb4d5 : 0x1b325b);
                text(bx + 56, y + theme_y + 16, k == 0 ? "Crystal" : k == 1 ? "Frost" : "Midnight", INK);
            }
            text(mx + 2, y + wall_y - 27, "Wallpaper", INK);
            for (int k = 0; k < 3; k++) {
                int bx = mx + k * 145;
                rounded(bx, y + wall_y, 132, compact ? 52 : 64, 10, wallpaper == k ? UI_HOVER : UI_CARD);
                outline(bx, y + wall_y, 132, compact ? 52 : 64, wallpaper == k ? BLUE : HAIR);
                rounded(bx + 9, y + wall_y + 8, 48, 44, 6, k == 0 ? 0x236bb0 : k == 1 ? 0x329bac : 0x455b99);
                text(bx + 61, y + wall_y + 19, k == 0 ? "Aurora" : k == 1 ? "Lagoon" : "Twilight", INK);
            }
            text(mx + 2, y + glass_y + 7, "Glass strength", INK);
            for (int k = 0; k < 3; k++) {
                int bx = mx + 155 + k * 80;
                rounded(bx, y + glass_y, 71, 32, 8, glass_level == k ? UI_HOVER : UI_CARD);
                outline(bx, y + glass_y, 71, 32, glass_level == k ? BLUE : HAIR);
                text(bx + 18, y + glass_y + 9, k == 0 ? "Low" : k == 1 ? "Med" : "High", INK);
            }
            text(mx + 2, y + accent_y + 7, "Accent color", INK);
            for (int k = 0; k < 4; k++) {
                int bx = mx + 155 + k * 65;
                rounded(bx, y + accent_y, 45, 36, 9, accent_colors[k]);
                if (accent_index == k) rounded(bx + 15, y + accent_y + 13, 16, 9, 4, WHITE);
            }
        } else if (settings_page == 0) {
            text(mx + 2, y + 61, "Display, sound and device information", STEEL);
            rounded(mx, y + 92, mw, 96, 12, UI_CARD);
            text(mx + 22, y + 108, "Display", INK);
            char dims[40] = "Resolution: ", tmp[12];
            number(tmp, (uint32_t)sw); append(dims, tmp, 40); append(dims, " x ", 40);
            number(tmp, (uint32_t)sh); append(dims, tmp, 40);
            text(mx + 22, y + 136, dims, STEEL);
            text(mx + 22, y + 161, "32-bit framebuffer / BIOS graphics", STEEL);
            rounded(mx, y + 204, mw, 166, 12, UI_CARD);
            text(mx + 22, y + 223, "Sound (software control)", INK);
            text(mx + 22, y + 254, "Output requires an audio device driver", STEEL);
            rounded(mx + 22, y + 292, 58, 34, 9, UI_HOVER);
            rounded(mx + 92, y + 292, 58, 34, 9, UI_HOVER);
            rounded(mx + 163, y + 292, 112, 34, 9, UI_HOVER);
            text(mx + 47, y + 301, "-", INK);
            text(mx + 117, y + 301, "+", INK);
            text(mx + 187, y + 301, volume_muted ? "Unmute" : "Mute", INK);
            number(tmp, (uint32_t)volume_level); text(mx + 295, y + 301, tmp, INK);
            rounded(mx, y + 390, mw, 109, 12, UI_CARD);
            text(mx + 22, y + 409, "Storage", INK);
            text(mx + 22, y + 440, storage_ready ? "Disk D: / QuartzieFS / ATA IDE" : "RAM R: / temporary session", STEEL);
            text(mx + 22, y + 466, storage_ready ? "Notes and documents survive reboot" : "Attach Quartzie disk image for persistence", STEEL);
        } else if (settings_page == 2) {
            text(mx + 2, y + 61, "Installed applications", STEEL);
            for (int k = 0; k < APPS; k++) {
                int yy = y + 95 + k * 45;
                if (yy + 40 >= y + ch - 14) break;
                rounded(mx, yy, mw, 41, 9, UI_CARD);
                text(mx + 22, yy + 11, app_name[k], INK);
                rounded(mx + mw - 89, yy + 6, 73, 28, 8, UI_HOVER);
                text(mx + mw - 74, yy + 13, "Open", BLUE);
            }
        } else if (settings_page == 3) {
            text(mx + 2, y + 61, "Notification preferences", STEEL);
            rounded(mx, y + 93, mw, 93, 12, UI_CARD);
            text(mx + 20, y + 110, "Quiet mode", INK);
            text(mx + 20, y + 138, "Hide unread badge; keep notification history", STEEL);
            rounded(mx + mw - 103, y + 117, 80, 33, 9, UI_HOVER);
            text(mx + mw - 86, y + 125, do_not_disturb ? "On" : "Off", INK);
            rounded(mx, y + 206, mw, 89, 12, UI_CARD);
            text(mx + 20, y + 226, "Notification history", INK);
            char count[12]; number(count, (uint32_t)notification_count);
            text(mx + 20, y + 254, count, STEEL);
            rounded(mx + mw - 142, y + 232, 121, 33, 9, UI_HOVER);
            text(mx + mw - 127, y + 240, "Clear all", INK);
        } else {
            text(mx + 2, y + 61, "About Quartzie", STEEL);
            rounded(mx, y + 93, mw, 222, 12, UI_CARD);
            big_text(mx + 20, y + 116, "Quartzie 0.28", INK);
            text(mx + 23, y + 162, "32-bit i386 operating system", STEEL);
            text(mx + 23, y + 193, "Legacy BIOS / El Torito boot", STEEL);
            text(mx + 23, y + 224, "Fluid Layers desktop with Dark Glass", STEEL);
            text(mx + 23, y + 255, "Session data is volatile RAM storage", STEEL);
        }
    } else if (app == SKETCH) {
        int scale = cw >= 900 ? 3 : 2;
        int paint_x = x + 18, paint_y = y + 44;
        int tools_x = x + cw - 247;
        text(x + 19, y + 14, "PINTA  -  Untitled", INK);
        text(x + cw - 195, y + 14, "native port / MIT", STEEL);
        rounded(paint_x - 5, paint_y - 5, SKETCH_W * scale + 10, SKETCH_H * scale + 10, 9, UI_FIELD);
        rect(paint_x, paint_y, SKETCH_W * scale, SKETCH_H * scale, WHITE);
        for (int row = 0; row < SKETCH_H; row++) for (int col = 0; col < SKETCH_W; col++) {
            uint8_t color = sketch_pixels[row][col];
            if (color) rect(paint_x + scale * col, paint_y + scale * row, scale, scale, ink_colors[color]);
        }
        outline(paint_x - 1, paint_y - 1, SKETCH_W * scale + 2, SKETCH_H * scale + 2, HAIR);
        const char *tools[PINTA_TOOLS] = {"Pencil", "Brush", "Eraser", "Line", "Rect", "Fill"};
        text(tools_x + 7, y + 48, "TOOLS", STEEL);
        for (int k = 0; k < PINTA_TOOLS; k++) {
            int tx = tools_x + (k % 2) * 118, ty = y + 68 + (k / 2) * 44;
            rounded(tx, ty, 107, 36, 8, sketch_tool == k ? UI_HOVER : UI_CARD);
            if (sketch_tool == k) outline(tx, ty, 107, 36, BLUE);
            text(tx + 11, ty + 10, tools[k], sketch_tool == k ? BLUE : INK);
        }
        text(tools_x + 7, y + 224, "Brush size", STEEL);
        rounded(tools_x + 8, y + 249, 34, 29, 7, UI_CARD);
        text(tools_x + 21, y + 255, "-", INK);
        rounded(tools_x + 52, y + 249, 34, 29, 7, UI_CARD);
        text(tools_x + 64, y + 255, "+", INK);
        char brush_size[12]; number(brush_size, (uint32_t)sketch_brush);
        text(tools_x + 103, y + 256, brush_size, INK);
        text(tools_x + 7, y + 297, "HISTORY", STEEL);
        rounded(tools_x + 8, y + 317, 105, 33, 8, UI_CARD);
        text(tools_x + 30, y + 326, "Undo", sketch_has_undo ? INK : STEEL);
        rounded(tools_x + 122, y + 317, 105, 33, 8, UI_CARD);
        text(tools_x + 144, y + 326, "Redo", sketch_has_redo ? INK : STEEL);
        rounded(tools_x + 8, y + 359, 105, 33, 8, UI_CARD);
        text(tools_x + 30, y + 368, "Clear", INK);
        rounded(tools_x + 122, y + 359, 105, 33, 8, UI_CARD);
        text(tools_x + 144, y + 368, "Save", storage_ready ? BLUE : STEEL);
        rect(x + 18, y + ch - 61, cw - 36, 1, HAIR);
        text(x + 20, y + ch - 83, sketch_dirty ? "Unsaved changes" : sketch_saved ? "Saved to Disk D:" : "Image / RGB palette", STEEL);
        for (int k = 0; k < 8; k++) {
            int px = x + 18 + 41 * k, py = y + ch - 44;
            rounded(px, py, 29, 29, 5, ink_colors[k + 1]);
            if (sketch_color == k + 1) outline(px - 3, py - 3, 35, 35, BLUE);
        }
        if (cw < 900) {
            rounded(x + cw - 199, y + ch - 43, 85, 29, 6, LIGHT);
            text(x + cw - 177, y + ch - 37, "Undo", sketch_has_undo ? INK : STEEL);
            rounded(x + cw - 102, y + ch - 43, 84, 29, 6, LIGHT);
            text(x + cw - 86, y + ch - 37, "Clear", INK);
        }
    } else if (app == CALENDAR) {
        int year, month;
        calendar_month(&year, &month);
        static const char *months[12] = {"January","February","March","April","May","June","July","August","September","October","November","December"};
        char year_text[12];
        number(year_text, (uint32_t)year);
        big_text(x + 23, y + 21, months[month - 1], INK);
        text(x + 27, y + 66, year_text, STEEL);
        rounded(x + cw - 109, y + 29, 35, 32, 6, LIGHT);
        rounded(x + cw - 65, y + 29, 35, 32, 6, LIGHT);
        text(x + cw - 98, y + 37, "<", INK);
        text(x + cw - 52, y + 37, ">", INK);
        rect(x + 22, y + 94, cw - 44, 1, HAIR);
        static const char *week[7] = {"Mo","Tu","We","Th","Fr","Sa","Su"};
        int cell = (cw - 48) / 7;
        if (cell < 25) cell = 25;
        for (int k = 0; k < 7; k++) text(x + 25 + k * cell, y + 112, week[k], STEEL);
        int start = calendar_weekday(year, month);
        int days = month_days(year, month);
        int today = date_number(clock_date + 8);
        for (int day = 1; day <= days; day++) {
            int k = start + day - 1;
            int cx = x + 21 + (k % 7) * cell;
            int cy = y + 145 + (k / 7) * 34;
            if (cy + 29 > y + ch - 5) continue;
            if (!calendar_shift && day == today) rounded(cx, cy, cell - 5, 29, 5, 0xb4d8f3);
            if (calendar_day == day) outline(cx, cy, cell - 5, 29, BLUE);
            char num[12];
            number(num, (uint32_t)day);
            text(cx + 8, cy + 7, num, INK);
        }
    } else if (app == WINDOWS_APP) {
        text(x + 23, y + 21, "OPEN WINDOWS", STEEL);
        rect(x + 22, y + 51, cw - 44, 1, HAIR);
        int row = 0;
        for (int i = 0; i < APPS; i++) {
            if (!windows[i].opened || i == WINDOWS_APP) continue;
            int cy = y + 63 + 36 * row++;
            if (cy + 33 > y + ch) break;
            rounded(x + 20, cy, cw - 40, 32, 5, LIGHT);
            text(x + 32, cy + 8, app_name[i], INK);
            char pid[32] = "PID ", number_text[12];
            number(number_text, cm_tasks[i].pid); append(pid, number_text, 32);
            text(x + 170, cy + 8, pid, STEEL);
            text(x + 280, cy + 8, windows[i].minimized ? "Minimized" : "Running", STEEL);
            rounded(x + cw - 163, cy + 3, 78, 26, 4, 0xd6e8f7);
            rounded(x + cw - 80, cy + 3, 61, 26, 4, 0xe7e9f0);
            text(x + cw - 154, cy + 7, "Switch", BLUE);
            text(x + cw - 72, cy + 7, "Close", INK);
        }
        if (!row) text(x + 24, y + 73, "No open windows", STEEL);
    } else if (app == CALC) {
        rounded(x + 20, y + 18, 264, 77, 12, LIGHT);
        outline(x + 20, y + 18, 264, 77, HAIR);
        text(x + 30, y + 25, calc_input, STEEL);
        big_text(x + 30, y + 56, calc_result, INK);
        const char *keys = "789/456*123-C0=+";
        for (int k = 0; k < 16; k++) {
            int px = x + 20 + (k % 4) * 66;
            int py = y + 113 + (k / 4) * 47;
            rounded(px, py, 57, 39, 10, k % 4 == 3 ? STEEL : LIGHT);
            outline(px, py, 57, 39, HAIR);
            glyph(px + 24, py + 11, keys[k], k % 4 == 3 ? WHITE : INK, 1);
        }
    }
    clip_left = old_l; clip_top = old_t; clip_right = old_r; clip_bottom = old_b;
}static void draw_window(int i) {
    const Window *w = &windows[i];
    if ((!w->opened || w->minimized) && !anim_visible[i]) return;
    int x, y, ww, hh;
    animated_window_rect(i, &x, &y, &ww, &hh);
    if (x >= clip_right + 40 || y >= clip_bottom + 40 || x + ww + 40 <= clip_left || y + hh + 40 <= clip_top) return;
    int focused = active_window() == i;
    uint32_t shell = NIGHT ? 0x0f1928 : 0xf7fbff;
    uint32_t body = NIGHT ? 0x172637 : 0xfdfefe;
    uint32_t border = focused ? blend(BLUE, WHITE, 118) : (NIGHT ? 0x6284a5 : 0xb6d2eb);
    uint32_t title = focused ? blend(BLUE, NIGHT ? 0x0f1a2a : 0xffffff, NIGHT ? 76 : 112) : (NIGHT ? 0x20324a : 0xe7f1fb);
    soft_shadow(x, y, ww, hh, focused ? 18 : 16, focused ? 10 : 7);
    round_capture(x, y, ww, hh, 18);
    frosted(x, y, ww, hh, shell, NIGHT ? 106 : 128, NIGHT ? 86 : 70);
    frosted(x + 1, y + 1, ww - 2, 35, title, focused ? 110 : 84, NIGHT ? 72 : 58);
    frosted(x + 2, y + 36, ww - 4, hh - 38, body, NIGHT ? 134 : 158, NIGHT ? 56 : 44);
    glass(x + 1, y + 1, ww - 2, 1, 0xffffff, focused ? 118 : 78);
    glass(x + 1, y + 1, 1, hh - 2, 0xffffff, NIGHT ? 42 : 56);
    glass(x + ww - 2, y + 1, 1, hh - 2, border, 142);
    glass(x + 1, y + hh - 2, ww - 2, 1, border, 142);
    rect(x + 1, y + 34, ww - 2, 1, blend(border, shell, 90));
    rounded(x + 11, y + 9, 18, 18, 6, focused ? blend(BLUE, 0xffffff, 42) : 0x7bb7df);
    draw_mark(x + 14, y + 10, WHITE);
    text(x + 39, y + 11, app_name[i], focused ? WHITE : (NIGHT ? 0xeaf5ff : 0x355170));
    int by = y + 5;
    round_capture(x + ww - 91, by, 27, 24, 8);
    frosted(x + ww - 91, by, 27, 24, NIGHT ? 0x1d2c41 : 0xf2f8fd, 128, 44);
    outline(x + ww - 91, by, 27, 24, border);
    round_finish(x + ww - 91, by, 27, 24, 8);
    round_capture(x + ww - 61, by, 27, 24, 8);
    frosted(x + ww - 61, by, 27, 24, NIGHT ? 0x1d2c41 : 0xf2f8fd, 128, 44);
    outline(x + ww - 61, by, 27, 24, border);
    round_finish(x + ww - 61, by, 27, 24, 8);
    round_capture(x + ww - 31, by, 27, 24, 8);
    frosted(x + ww - 31, by, 27, 24, 0xb84556, 176, 36);
    outline(x + ww - 31, by, 27, 24, blend(0xb84556, 0xffffff, 74));
    round_finish(x + ww - 31, by, 27, 24, 8);
    rounded(x + ww - 83, by + 12, 11, 3, 1, focused ? WHITE : border);
    round_capture(x + ww - 54, by + 7, 12, 10, 3);
    rounded(x + ww - 54, by + 7, 10, 9, 2, blend(focused ? WHITE : border, shell, 34));
    round_finish(x + ww - 54, by + 7, 12, 10, 3);
    line(x + ww - 24, by + 7, x + ww - 13, by + 18, WHITE);
    line(x + ww - 13, by + 7, x + ww - 24, by + 18, WHITE);
    Window view = *w;
    view.x = x; view.y = y; view.w = ww; view.h = hh;
    window_content(i, &view);
    if (!w->maximized && !snap_side[i]) {
        line(x + ww - 19, y + hh - 6, x + ww - 6, y + hh - 19, 0x799eb9);
        line(x + ww - 13, y + hh - 6, x + ww - 6, y + hh - 13, 0xa7c4d9);
    }
    round_finish(x, y, ww, hh, 18);
}

static int menu_height(void) { return sh < 540 ? sh - 54 : 442; }
static int menu_top(void) { return sh - 70 - menu_height(); }
static int menu_width(void) { return sw < 510 ? sw - 20 : 490; }
static int menu_left(void) { return sw >= 660 ? (sw - menu_width()) / 2 : 12; }

static void draw_menu(void) {
    if (!panel_visible(PANEL_START)) return;
    int mh = menu_height();
    int my = menu_top();
    int mw = menu_width();
    int mx = menu_left();
    int phase = ease(panel_step[PANEL_START], PANEL_STEPS);
    my += ((256 - phase) * 18) >> 8;
    soft_shadow(mx, my, mw, mh, 20, 9);
    blur_region(mx, my, mw, 68);
    round_capture(mx, my, mw, mh, 20);
    frosted(mx, my, mw, mh, NIGHT ? 0x101827 : 0xf3f8ff, NIGHT ? 128 : 158, 48);
    frosted(mx + 2, my + 2, mw - 4, 52, NIGHT ? 0x222d48 : 0x353b8d, 176, 38);
    glass(mx + 2, my + 2, mw - 4, 1, 0xffffff, 148);
    rounded(mx + 16, my + 10, 32, 32, 10, 0x6c78ff);
    rounded(mx + 26, my + 19, 12, 12, 4, 0xf1f6ff);
    text(mx + 60, my + 11, "Quartzie", 0xffffff);
    text(mx + 60, my + 30, "Fluid Layers", 0xd9e5ff);
    if (mh >= 420) {
        rounded(mx + 18, my + 65, mw - 36, 36, 13, NIGHT ? 0x23374d : 0xe8f1ff);
        glass(mx + 19, my + 66, mw - 38, 1, 0xffffff, 122);
        outline(mx + 18, my + 65, mw - 36, 36, NIGHT ? 0x526c9f : 0xb5ccef);
        circle(mx + 36, my + 82, 6, BLUE);
        line(mx + 40, my + 87, mx + 45, my + 92, BLUE);
        text(mx + 56, my + 75, menu_query_len ? menu_query : "Search apps and settings", menu_query_len ? INK : STEEL);
        text(mx + 22, my + 113, menu_query_len ? "Search results" : "Pinned apps", INK);
        text(mx + mw - 84, my + 113, "All apps", STEEL);
        for (int k = 0; k < APPS; k++) {
            int app = menu_row_app(k);
            if (app < 0) break;
            int col = k % 5, row = k / 5;
            int cell_x = mx + 18 + col * 92;
            int cell_y = my + 137 + row * 93;
            rounded(cell_x + 5, cell_y + 2, 83, 78, 13,
                (menu_query_len && k == 0) ? UI_HOVER : (NIGHT ? 0x1d2c42 : 0xe5f0ff));
            if (menu_query_len && k == 0) outline(cell_x + 5, cell_y + 2, 83, 78, BLUE);
            draw_icon(cell_x + 26, cell_y + 9, app);
            int label_width = size(app_name[app]) * 8;
            if (label_width > 80) label_width = 80;
            text(cell_x + 46 - label_width / 2, cell_y + 58, app_name[app], INK);
        }
        if (menu_row_app(0) < 0) text(mx + 31, my + 170, "No matching apps", INK);
        rect(mx + 21, my + 326, mw - 42, 1, NIGHT ? 0x4a6383 : 0xc4d5ec);
        text(mx + 22, my + 338, "Recommended", INK);
        rounded(mx + 21, my + 362, 216, 38, 11, NIGHT ? 0x22344c : 0xe5effb);
        rounded(mx + 252, my + 362, 216, 38, 11, NIGHT ? 0x22344c : 0xe5effb);
        text(mx + 36, my + 372, "File Explorer", INK);
        text(mx + 268, my + 372, "Notes", INK);
        frosted(mx + 2, my + mh - 34, mw - 4, 32, NIGHT ? 0x19273c : 0xdceaff, NIGHT ? 152 : 168, 42);
        text(mx + 24, my + mh - 27, "Quartzie desktop", INK);
        text(mx + mw - 138, my + mh - 27, "Restart", INK);
        text(mx + mw - 66, my + mh - 27, "Power", INK);
    } else {
        int row_h = 24;
        int max_rows = (mh - 116) / row_h;
        if (max_rows > APPS) max_rows = APPS;
        for (int k = 0; k < max_rows; k++) {
            int app = menu_row_app(k);
            if (app < 0) break;
            int py = my + 58 + k * row_h;
            rounded(mx + 13, py, mw - 26, row_h - 2, 7, (menu_query_len && k == 0) ? UI_HOVER : UI_CARD);
            text(mx + 31, py + 4, app_name[app], INK);
        }
        rounded(mx + 12, my + mh - 42, mw - 24, 32, 8, UI_FIELD);
        text(mx + 25, my + mh - 34, menu_query_len ? menu_query : "Search programs", INK);
    }
    glass(mx + 1, my + mh - 2, mw - 2, 1, NIGHT ? 0x7595c5 : 0xc6d8f2, 110);
    round_finish(mx, my, mw, mh, 20);
}

static int context_height(void) { return 208; }
static int context_width(void) { return 208; }

static int quick_width(void) { return 262; }
static int quick_height(void) { return 422; }

static void draw_context(void) {
    if (!panel_visible(PANEL_CONTEXT)) return;
    int x = context_x, y = context_y;
    int w = context_width(), h = context_height();
    y += panel_shift(PANEL_CONTEXT, 12);
    soft_shadow(x, y, w, h, 14, 6);
    blur_region(x, y, w, 44);
    round_capture(x, y, w, h, 16);
    frosted(x, y, w, h, NIGHT ? 0x111d2d : 0xf7fbff, NIGHT ? 118 : 132, NIGHT ? 82 : 64);
    glass(x + 1, y + 1, w - 2, 1, 0xffffff, 84);
    glass(x + 1, y + h - 2, w - 2, 1, NIGHT ? 0x6e89a7 : 0xa6c4de, 110);
    frosted(x + 6, y + 6, w - 12, h - 12, PAPER, NIGHT ? 112 : 152, NIGHT ? 36 : 22);
    const char *items[5] = {"Refresh", "Appearance", "Windows", "Show desktop", "Toggle Dark Glass"};
    for (int i = 0; i < 5; i++) {
        int py = y + 13 + i * 38;
        if (mouse_x >= x + 7 && mouse_x < x + w - 7 && mouse_y >= py && mouse_y < py + 34)
            rounded(x + 8, py, w - 16, 33, 10, UI_HOVER);
        text(x + 23, py + 9, items[i], INK);
    }
    round_finish(x, y, w, h, 16);
}

static void draw_quick(void) {
    if (!panel_visible(PANEL_QUICK)) return;
    int w = quick_width(), h = quick_height();
    int x = sw - w - 16;
    int y = sh - 60 - h;
    y += panel_shift(PANEL_QUICK, 14);
    soft_shadow(x, y, w, h, 16, 8);
    blur_region(x, y, w, 48);
    round_capture(x, y, w, h, 18);
    frosted(x, y, w, h, NIGHT ? 0x101a29 : 0xf8fbff, NIGHT ? 116 : 132, NIGHT ? 80 : 64);
    frosted(x + 2, y + 2, w - 4, 46, UI_HEAD, 116, 64);
    glass(x + 2, y + 2, w - 4, 14, 0xffffff, 64);
    glass(x + 1, y + 1, w - 2, 1, 0xffffff, 84);
    glass(x + 1, y + h - 2, w - 2, 1, NIGHT ? 0x6e89a7 : 0xa6c4de, 110);
    text(x + 18, y + 12, clock_time, WHITE);
    text(x + 18, y + 29, clock_date, 0xd9edfb);
    rounded(x + 16, y + 60, 230, 34, 10, UI_CARD);
    rounded(x + 16, y + 102, 110, 34, 10, UI_CARD);
    rounded(x + 136, y + 102, 110, 34, 10, UI_CARD);
    rounded(x + 16, y + 144, 230, 28, 10, UI_CARD);
    text(x + 29, y + 70, "Open Calendar", INK);
    text(x + 29, y + 112, "Appearance", INK);
    text(x + 165, y + 112, "Desktop", INK);
    text(x + 29, y + 151, glass_level == 0 ? "Glass: Light" : glass_level == 1 ? "Glass: Medium" : "Glass: Strong", INK);
    text(x + 16, y + 187, "VOLUME (UI ONLY)", WHITE);
    rounded(x + 16, y + 211, 46, 31, 8, UI_CARD);
    rounded(x + 72, y + 211, 46, 31, 8, UI_CARD);
    rounded(x + 129, y + 211, 117, 31, 8, UI_CARD);
    text(x + 35, y + 217, "-", INK);
    text(x + 91, y + 217, "+", INK);
    text(x + 146, y + 217, volume_muted ? "Unmute" : "Mute", INK);
    char level[8]; number(level, (uint32_t)(volume_muted ? 0 : volume_level));
    text(x + 190, y + 187, level, WHITE);
    text(x + 16, y + 255, "TOOLS", WHITE);
    rounded(x + 16, y + 271, 230, 31, 8, UI_CARD);
    text(x + 29, y + 278, "Run command", INK);
    text(x + 16, y + 319, "DISPLAY AND ALERTS", WHITE);
    rounded(x + 16, y + 338, 230, 34, 10, UI_CARD);
    text(x + 29, y + 347, NIGHT ? "Dark Glass: On" : "Dark Glass: Off", INK);
    rounded(x + 164, y + 345, 69, 20, 9, NIGHT ? BLUE : HAIR);
    circle(NIGHT ? x + 223 : x + 174, y + 355, 7, WHITE);
    rounded(x + 16, y + 375, 230, 34, 10, UI_CARD);
    text(x + 29, y + 384, do_not_disturb ? "Quiet: On" : "Quiet: Off", INK);
    round_finish(x, y, w, h, 18);
}

static int notification_width(void) { return 336; }
static int notification_height(void) { return 354; }
static int notification_left(void) { return sw - notification_width() - 14; }
static int notification_top(void) { return sh - 60 - notification_height(); }

static void draw_notifications(void) {
    if (!panel_visible(PANEL_NOTIFICATIONS)) return;
    int w = notification_width(), h = notification_height();
    int x = notification_left();
    int y = notification_top() + panel_shift(PANEL_NOTIFICATIONS, 20);
    soft_shadow(x, y, w, h, 17, 8);
    blur_region(x, y, w, 48);
    round_capture(x, y, w, h, 18);
    frosted(x, y, w, h, NIGHT ? 0x101a29 : 0xf8fbff, NIGHT ? 116 : 132, NIGHT ? 80 : 64);
    frosted(x + 2, y + 2, w - 4, 52, UI_HEAD, 118, 64);
    glass(x + 1, y + 1, w - 2, 1, 0xffffff, 84);
    glass(x + 1, y + h - 2, w - 2, 1, NIGHT ? 0x6e89a7 : 0xa6c4de, 110);
    text(x + 18, y + 17, "NOTIFICATIONS", WHITE);
    text(x + w - 93, y + 18, "Clear all", WHITE);
    for (int i = 0; i < notification_count && i < 5; i++) {
        int py = y + 61 + i * 55;
        rounded(x + 12, py, w - 24, 49, 10, UI_CARD);
        rounded(x + 13, py + 1, 5, 47, 2, BLUE);
        text(x + 28, py + 9, notification_text[notification_count - 1 - i], INK);
        text(x + 28, py + 28, notification_time[notification_count - 1 - i], STEEL);
        text(x + w - 31, py + 12, "x", STEEL);
    }
    if (do_not_disturb) text(x + 18, y + 37, "Quiet mode is enabled", WHITE);
    if (!notification_count) {
        text(x + 60, y + 156, "You're all caught up", INK);
        text(x + 60, y + 182, "No new notifications", STEEL);
    }
    round_finish(x, y, w, h, 18);
}

static void draw_snap_preview(void) {
    if (drag_app < 0 || sw < 1000 || (mouse_x > 14 && mouse_x < sw - 15)) return;
    int x = mouse_x <= 14 ? 5 : sw / 2 + 3;
    int y = 5;
    int w = sw / 2 - 9;
    int h = sh - 65;
    round_capture(x, y, w, h, 14);
    glass(x, y, w, h, 0x8fc7f0, 90);
    glass(x + 6, y + 6, w - 12, h - 12, 0xa4dcff, 46);
    glass(x + 10, y + 10, w - 20, h - 20, 0x5ba4df, 19);
    glass(x + 4, y + 3, w - 8, 2, 0xe1f7ff, 120);
    glass(x + 4, y + h - 5, w - 8, 2, 0xe1f7ff, 88);
    round_finish(x, y, w, h, 14);
}

static int run_width(void) { return 366; }
static int run_height(void) { return 146; }
static int run_left(void) { return (sw - run_width()) / 2; }
static int run_top(void) { return sh / 2 - 126; }

static void draw_run(void) {
    if (!panel_visible(PANEL_RUN)) return;
    int w = run_width(), h = run_height();
    int x = run_left();
    int y = run_top() + panel_shift(PANEL_RUN, 15);
    soft_shadow(x, y, w, h, 16, 8);
    round_capture(x, y, w, h, 15);
    frosted(x, y, w, h, NIGHT ? 0x101a29 : 0xf8fbff, NIGHT ? 118 : 132, NIGHT ? 80 : 64);
    glass(x + 1, y + 1, w - 2, 1, 0xffffff, 84);
    glass(x + 1, y + h - 2, w - 2, 1, NIGHT ? 0x6e89a7 : 0xa6c4de, 110);
    frosted(x + 2, y + 2, w - 4, h - 4, PAPER, NIGHT ? 112 : 154, NIGHT ? 36 : 22);
    text(x + 22, y + 18, "Run", INK);
    text(x + 22, y + 40, "Open a program, folder or command.", STEEL);
    rounded(x + 21, y + 67, w - 42, 32, 9, LIGHT);
    outline(x + 21, y + 67, w - 42, 32, UI_EDGE);
    text(x + 31, y + 76, run_len ? run_input : "files", run_len ? INK : 0x6c87a3);
    rounded(x + w - 103, y + 108, 80, 25, 7, UI_HOVER);
    text(x + w - 78, y + 115, "Launch", INK);
    round_finish(x, y, w, h, 15);
}

static void tooltip_size(int hover, int shown, int *w, int *h) {
    *w = shown ? 202 : size(app_name[hover]) * 8 + 30;
    if (*w < 92) *w = 92;
    *h = shown ? 152 : 36;
}

static int tooltip_left(int hover, int w) {
    int items[APPS];
    int count = taskbar_items(items);
    int left = taskbar_left(count);
    int center = mouse_x;
    for (int i = 0; i < count; i++) {
        if (items[i] == hover) { center = left + i * taskbar_step() + taskbar_width() / 2; break; }
    }
    int x = center - w / 2;
    if (x < 8) x = 8;
    if (x + w > sw - 8) x = sw - 8 - w;
    return x;
}

static int tooltip_top(int h) { return sh - 62 - h; }

static void draw_tooltip(void) {
    if (!panel_visible(PANEL_TOOLTIP) || tooltip_render_app < 0) return;
    int hover_app = tooltip_render_app;
    int shown = tooltip_render_running;
    int w, h;
    tooltip_size(hover_app, shown, &w, &h);
    int x = tooltip_left(hover_app, w);
    int y = tooltip_top(h) + panel_shift(PANEL_TOOLTIP, 8);
    if (shown) {
        Window *a = &windows[hover_app];
        for (int py = 0; py < 92; py++) for (int px = 0; px < 172; px++) {
            int sx = a->x + (px * a->w / 172);
            int sy = a->y + (py * a->h / 92);
            preview_buffer[py * 172 + px] = (unsigned)sx < (unsigned)sw && (unsigned)sy < (unsigned)sh ? canvas[(size_t)sy * sw + sx] : BLACK;
        }
    }
    soft_shadow(x, y, w, h, 11, 4);
    round_capture(x, y, w, h, 12);
    frosted(x, y, w, h, NIGHT ? 0x111c2b : 0xf9fcff, NIGHT ? 118 : 132, NIGHT ? 80 : 64);
    frosted(x + 2, y + 2, w - 4, h - 4, UI_CARD, NIGHT ? 122 : 156, NIGHT ? 34 : 22);
    glass(x + 1, y + 1, w - 2, 1, 0xffffff, 84);
    glass(x + 3, y + 3, w - 6, shown ? 30 : 13, 0xffffff, 66);
    if (shown) {
        draw_task_icon(x + 7, y + 4, hover_app);
        text(x + 48, y + 12, app_name[hover_app], INK);
        rounded(x + 11, y + 40, 180, 101, 7, NIGHT ? 0x38516a : 0xb7cfe2);
        for (int py = 0; py < 92; py++) {
            int yy = y + 44 + py;
            if (yy < clip_top || yy >= clip_bottom || (unsigned)yy >= (unsigned)sh) continue;
            for (int px = 0; px < 172; px++) {
                int xx = x + 15 + px;
                if (xx >= clip_left && xx < clip_right && (unsigned)xx < (unsigned)sw)
                    canvas[(size_t)yy * sw + xx] = preview_buffer[py * 172 + px];
            }
        }
    } else {
        int tw = size(app_name[hover_app]) * 8;
        text(x + (w - tw) / 2, y + 11, app_name[hover_app], INK);
    }
    round_finish(x, y, w, h, 12);
}

static int switcher_left(void) { return (sw - 450) / 2; }
static int switcher_top(void) { return (sh - (tab_count > 5 ? 238 : 154)) / 2; }
static int switcher_height(void) { return tab_count > 5 ? 238 : 154; }

static void draw_switcher(void) {
    if (!panel_visible(PANEL_SWITCHER) || tab_count < 2) return;
    int x = switcher_left(), y = switcher_top() + panel_shift(PANEL_SWITCHER, 15);
    int h = switcher_height();
    soft_shadow(x, y, 450, h, 17, 6);
    round_capture(x, y, 450, h, 16);
    frosted(x, y, 450, h, NIGHT ? 0x101a29 : 0xf9fcff, NIGHT ? 118 : 132, NIGHT ? 80 : 64);
    frosted(x + 2, y + 2, 446, h - 4, NIGHT ? 0x182739 : 0xfbfdff, NIGHT ? 124 : 158, NIGHT ? 36 : 22);
    glass(x + 1, y + 1, 448, 1, 0xffffff, 84);
    text(x + 21, y + 15, "Switch windows", WHITE);
    text(x + 239, y + 15, "Release Alt to switch", 0xb7d5e9);
    for (int i = 0; i < tab_count; i++) {
        int cx = x + 22 + (i % 5) * 85;
        int cy = y + 46 + (i / 5) * 83;
        if (i == tab_selected) {
            rounded(cx - 6, cy - 5, 73, 72, 10, 0x376d98);
            outline(cx - 6, cy - 5, 73, 72, 0x8bcef4);
        }
        draw_task_icon(cx + 14, cy + 1, tab_apps[i]);
        int len = size(app_name[tab_apps[i]]) * 8;
        if (len > 77) len = 77;
        text(cx + 30 - len / 2, cy + 41, app_name[tab_apps[i]], WHITE);
        if (windows[tab_apps[i]].minimized) rounded(cx + 25, cy + 60, 10, 2, 1, 0x95b2c7);
    }
    round_finish(x, y, 450, h, 16);
}

static void start_or_cycle_switcher(void) {
    if (!tab_active) {
        tab_count = 0;
        int used[APPS] = {0};
        for (int n = 0; n < APPS; n++) {
            int best = -1;
            for (int i = 0; i < APPS; i++) {
                if (!used[i] && windows[i].opened && (best < 0 || windows[i].order > windows[best].order)) best = i;
            }
            if (best < 0) break;
            used[best] = 1;
            tab_apps[tab_count++] = best;
        }
        if (tab_count < 2) return;
        tab_active = 1;
        tab_selected = shift_down ? tab_count - 1 : 1;
        start_open = 0;
        quick_open = 0;
        context_open = 0;
        notification_open = 0;
    } else {
        tab_selected = (tab_selected + (shift_down ? tab_count - 1 : 1)) % tab_count;
    }
    dirty = 1;
}

static void finish_switcher(int commit) {
    if (!tab_active) return;
    int chosen = tab_apps[tab_selected];
    tab_active = 0;
    dirty = 1;
    if (commit && windows[chosen].opened) open_window(chosen);
}

static void draw_pointer(void) {
    pointer_old_x = mouse_x;
    pointer_old_y = mouse_y;
    for (int y = 0; y < CURSOR_BOX_H; y++) {
        for (int x = 0; x < CURSOR_BOX_W; x++) {
            int px = mouse_x + x, py = mouse_y + y;
            pointer_back[y][x] = (unsigned)px < (unsigned)sw && (unsigned)py < (unsigned)sh ? canvas[(size_t)py * sw + px] : 0;
        }
    }
    for (int y = 0; y < CURSOR_H; y++) {
        for (int x = 0; x < CURSOR_W; x++) {
            int alpha = cursor_alpha[y][x];
            if (!alpha) continue;
            int sx = mouse_x + x + 1, sy = mouse_y + y + 1;
            if ((unsigned)sx < (unsigned)sw && (unsigned)sy < (unsigned)sh) {
                uint32_t *dst = canvas + (size_t)sy * sw + sx;
                *dst = blend(*dst, 0x10223b, alpha * 3 / 7);
            }
        }
    }
    for (int y = 0; y < CURSOR_H; y++) {
        for (int x = 0; x < CURSOR_W; x++) {
            int alpha = cursor_alpha[y][x];
            if (!alpha) continue;
            int px = mouse_x + x, py = mouse_y + y;
            if ((unsigned)px < (unsigned)sw && (unsigned)py < (unsigned)sh) {
                uint32_t *dst = canvas + (size_t)py * sw + px;
                *dst = blend(*dst, WHITE, alpha);
            }
        }
    }
    pointer_saved = 1;
}

static void present_region(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > sw) w = sw - x;
    if (y + h > sh) h = sh - y;
    if (w <= 0 || h <= 0) return;
    presented_pixels += (uint32_t)(w * h);
    uint8_t *base = (uint8_t *)(uintptr_t)boot_fb;
    for (int yy = y; yy < y + h; yy++) {
        if (boot_bpp == 32) {
            uint32_t *dst = (uint32_t *)(base + (uint32_t)yy * boot_pitch) + x;
            const uint32_t *src = canvas + (size_t)yy * sw + x;
            size_t count = (size_t)w;
            __asm__ volatile("cld; rep movsl" : "+D"(dst), "+S"(src), "+c"(count) : : "memory");
        } else {
            volatile uint8_t *dst = base + (uint32_t)yy * boot_pitch;
            for (int xx = x; xx < x + w; xx++) {
                uint32_t c = canvas[(size_t)yy * sw + xx];
                dst[xx] = (uint8_t)((c & 0xe00000) >> 16 | (c & 0x00e000) >> 11 | (c & 0xc0) >> 6);
            }
        }
    }
}

static void move_pointer_only(void) {
    if (!pointer_saved) { dirty = 1; return; }
    for (int y = 0; y < CURSOR_BOX_H; y++) for (int x = 0; x < CURSOR_BOX_W; x++) {
        int px = pointer_old_x + x, py = pointer_old_y + y;
        if ((unsigned)px < (unsigned)sw && (unsigned)py < (unsigned)sh)
            canvas[(size_t)py * sw + px] = pointer_back[y][x];
    }
    int old_x = pointer_old_x;
    int old_y = pointer_old_y;
    clip_full();
    draw_pointer();
    present_region(old_x, old_y, CURSOR_BOX_W, CURSOR_BOX_H);
    present_region(mouse_x, mouse_y, CURSOR_BOX_W, CURSOR_BOX_H);
    pointer_frames++;
    pointer_only = 0;
}

static void program_palette(void) {
    out8(0x3c8, 0);
    for (int i = 0; i < 256; i++) {
        out8(0x3c9, (uint8_t)(((i >> 5) & 7) * 9));
        out8(0x3c9, (uint8_t)(((i >> 2) & 7) * 9));
        out8(0x3c9, (uint8_t)((i & 3) * 21));
    }
}

static void present(void) {
    presented_pixels += (uint32_t)(sw * sh);
    uint8_t *base = (uint8_t *)(uintptr_t)boot_fb;
    if (boot_bpp == 32) {
        for (int y = 0; y < sh; y++) {
            uint32_t *dst = (uint32_t *)(base + (uint32_t)y * boot_pitch);
            const uint32_t *src = canvas + (size_t)y * (size_t)sw;
            size_t count = (size_t)sw;
            __asm__ volatile("cld; rep movsl" : "+D"(dst), "+S"(src), "+c"(count) : : "memory");
        }
    } else {
        for (int y = 0; y < sh; y++) {
            volatile uint8_t *dst = base + (uint32_t)y * boot_pitch;
            const uint32_t *src = canvas + (size_t)y * (size_t)sw;
            for (int x = 0; x < sw; x++) {
                uint32_t c = src[x];
                dst[x] = (uint8_t)((c & 0xe00000) >> 16 | (c & 0x00e000) >> 11 | (c & 0xc0) >> 6);
            }
        }
    }
}

static void add_damage(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > sw) w = sw - x;
    if (y + h > sh) h = sh - y;
    if (w <= 0 || h <= 0) return;
    int x2 = x + w, y2 = y + h;
    for (int k = 0; k < damage_count;) {
        DamageRect *d = &damage[k];
        int ux = x < d->x ? x : d->x, uy = y < d->y ? y : d->y;
        int ux2 = x2 > d->x + d->w ? x2 : d->x + d->w;
        int uy2 = y2 > d->y + d->h ? y2 : d->y + d->h;
        int enclosing = (ux2 - ux) * (uy2 - uy);
        int pieces = (x2 - x) * (y2 - y) + d->w * d->h;
        if (x <= d->x + d->w && x2 >= d->x && y <= d->y + d->h && y2 >= d->y && enclosing * 100 <= pieces * 118) {
            if (d->x < x) x = d->x;
            if (d->y < y) y = d->y;
            if (d->x + d->w > x2) x2 = d->x + d->w;
            if (d->y + d->h > y2) y2 = d->y + d->h;
            damage[k] = damage[--damage_count];
            k = 0;
        } else k++;
    }
    if (damage_count >= 32) { damage_count = 1; damage[0] = (DamageRect){0, 0, sw, sh}; return; }
    damage[damage_count++] = (DamageRect){x, y, x2 - x, y2 - y};
}

static void add_window_damage(DamageRect d) {
    add_damage(d.x - 46, d.y - 46, d.w + 92, d.h + 105);
}

static int snap_preview_side(void) {
    if (drag_app < 0 || sw < 1000) return 0;
    if (mouse_x <= 14) return -1;
    if (mouse_x >= sw - 15) return 1;
    return 0;
}

static void add_preview_damage(int side) {
    if (side) add_damage(side < 0 ? 0 : sw / 2 - 2, 0, sw / 2 + 2, sh - 58);
}

static void add_tooltip_damage(int hover, int shown, int visible) {
    if (hover < 0 || hover >= APPS || !visible) return;
    int w, h;
    tooltip_size(hover, shown, &w, &h);
    int left = tooltip_left(hover, w);
    add_damage(left - 45, tooltip_top(h) - 52, w + 90, h + 112);
}

static void draw_scene(void) {
    draw_desktop();
    int placed[APPS] = {0};
    for (int n = 0; n < APPS; n++) {
        int pick = -1;
        for (int i = 0; i < APPS; i++) {
            if (placed[i] || ((!windows[i].opened || windows[i].minimized) && !anim_visible[i])) continue;
            if (pick == -1 || windows[i].order < windows[pick].order) pick = i;
        }
        if (pick < 0) break;
        placed[pick] = 1;
        draw_window(pick);
    }
    draw_snap_preview();
    draw_menu();
    draw_context();
    draw_quick();
    draw_notifications();
    draw_run();
    if (panel_visible(PANEL_SWITCHER)) draw_switcher();
    else draw_tooltip();
}

static void repaint(void) {
    if ((unsigned)theme > 3u) theme = 0;
    if ((unsigned)accent_index > 3u) accent_index = 0;
    panel_sync();
    int entire = !frame_valid || frame_theme != theme || frame_wallpaper != wallpaper ||
        frame_glass != glass_level || frame_accent != accent_index;
    DamageRect current[APPS];
    int visible[APPS];
    clip_full();
    if (pointer_saved) {
        for (int y = 0; y < CURSOR_BOX_H; y++) for (int x = 0; x < CURSOR_BOX_W; x++) {
            int px = pointer_old_x + x, py = pointer_old_y + y;
            if ((unsigned)px < (unsigned)sw && (unsigned)py < (unsigned)sh)
                canvas[(size_t)py * sw + px] = pointer_back[y][x];
        }
        pointer_saved = 0;
    }
    damage_count = 0;

    int changed_window = 0;
    int order_changed = 0;
    for (int i = 0; i < APPS; i++) {
        visible[i] = (windows[i].opened && !windows[i].minimized) || anim_visible[i];
        current[i] = (DamageRect){0, 0, 0, 0};
        if (visible[i]) {
            animated_window_rect(i, &current[i].x, &current[i].y, &current[i].w, &current[i].h);
        }
        if (!entire) {
            int differs = last_window_visible[i] != visible[i];
            if (visible[i] && last_window_visible[i]) {
                DamageRect d = last_window_rect[i];
                if (d.x != current[i].x || d.y != current[i].y || d.w != current[i].w || d.h != current[i].h) differs = 1;
                if (last_window_order[i] != windows[i].order) order_changed = 1;
            }
            if (differs) {
                changed_window = 1;
                if (last_window_visible[i]) add_window_damage(last_window_rect[i]);
                if (visible[i]) add_window_damage(current[i]);
            }
        }
    }
    if (!entire) {
        if (taskbar_dirty || changed_window || order_changed || last_active != active_window() ||
            last_start != start_open || last_quick != quick_open ||
            last_notification != notification_open || last_context != context_open || last_hover != task_hover || last_hover_visible != hover_visible) {
            add_damage(0, sh - 83, sw, 83);
        }
        if (order_changed) {
            for (int i = 0; i < APPS; i++) {
                if (visible[i]) add_window_damage(current[i]);
                if (last_window_visible[i]) add_window_damage(last_window_rect[i]);
            }
        }
        if (dirty && !changed_window && !order_changed && !start_open && !context_open && !quick_open && !notification_open &&
            !last_start && !last_context && !last_quick && !last_notification && last_hover == task_hover && !taskbar_dirty) {
            int focus = active_window();
            if (focus >= 0 && visible[focus]) add_window_damage(current[focus]);
            if (last_active >= 0 && last_active != focus && last_window_visible[last_active]) add_window_damage(last_window_rect[last_active]);
        }
        if (panel_visible(PANEL_START) || panel_was_visible[PANEL_START] || last_start) add_damage(menu_left() - 46, menu_top() - 52, menu_width() + 92, menu_height() + 112);
        if (panel_visible(PANEL_CONTEXT) || panel_was_visible[PANEL_CONTEXT]) add_damage(context_x - 46, context_y - 52, context_width() + 92, context_height() + 111);
        if (last_context) add_damage(last_context_x - 46, last_context_y - 52, context_width() + 92, context_height() + 111);
        if (panel_visible(PANEL_QUICK) || panel_was_visible[PANEL_QUICK] || last_quick) add_damage(sw - quick_width() - 57, sh - 60 - quick_height() - 52, quick_width() + 93, quick_height() + 113);
        if (panel_visible(PANEL_NOTIFICATIONS) || panel_was_visible[PANEL_NOTIFICATIONS] || last_notification) add_damage(notification_left() - 47, notification_top() - 52, notification_width() + 94, notification_height() + 112);
        if (panel_visible(PANEL_RUN) || panel_was_visible[PANEL_RUN] || run_anim) add_damage(run_left() - 46, run_top() - 52, run_width() + 92, run_height() + 112);
        add_tooltip_damage(last_hover, last_hover_running, last_hover_visible);
        add_tooltip_damage(tooltip_render_app, tooltip_render_running, panel_visible(PANEL_TOOLTIP) || panel_was_visible[PANEL_TOOLTIP]);
        add_preview_damage(last_snap_side);
        add_preview_damage(snap_preview_side());
        if (desktop_focus != last_desktop_focus || desktop_hover != last_desktop_hover) {
            if (last_desktop_focus >= 0 || last_desktop_hover >= 0) {
                int slot = last_desktop_hover >= 0 ? last_desktop_hover : last_desktop_focus;
                int x, y, w, h; desktop_slot_bounds(slot, &x, &y, &w, &h);
                add_damage(x - 4, y - 4, w + 8, h + 8);
            }
            if (desktop_focus >= 0 || desktop_hover >= 0) {
                int slot = desktop_hover >= 0 ? desktop_hover : desktop_focus;
                int x, y, w, h; desktop_slot_bounds(slot, &x, &y, &w, &h);
                add_damage(x - 4, y - 4, w + 8, h + 8);
            }
        }
        if (panel_visible(PANEL_SWITCHER) || panel_was_visible[PANEL_SWITCHER] || last_tab_active) {
            add_damage((sw - 450) / 2 - 48, (sh - 238) / 2 - 52, 546, 351);
        }
        add_damage(pointer_old_x, pointer_old_y, CURSOR_BOX_W + 1, CURSOR_BOX_H + 1);
        add_damage(mouse_x, mouse_y, CURSOR_BOX_W + 1, CURSOR_BOX_H + 1);
        if (damage_count > 8) {
            int min_x = damage[0].x, min_y = damage[0].y;
            int max_x = damage[0].x + damage[0].w, max_y = damage[0].y + damage[0].h;
            for (int i = 1; i < damage_count; i++) {
                if (damage[i].x < min_x) min_x = damage[i].x;
                if (damage[i].y < min_y) min_y = damage[i].y;
                if (damage[i].x + damage[i].w > max_x) max_x = damage[i].x + damage[i].w;
                if (damage[i].y + damage[i].h > max_y) max_y = damage[i].y + damage[i].h;
            }
            damage_count = 1;
            damage[0] = (DamageRect){min_x, min_y, max_x - min_x, max_y - min_y};
        }
    }
    if (changed_window || order_changed) entire = 1;
    if (entire) {
        clip_full();
        draw_scene();
        draw_pointer();
        present();
        full_frames++;
    } else {
        for (int i = 0; i < damage_count; i++) {
            DamageRect d = damage[i];
            clip_left = d.x; clip_top = d.y; clip_right = d.x + d.w; clip_bottom = d.y + d.h;
            draw_scene();
        }
        clip_full();
        draw_pointer();
        for (int i = 0; i < damage_count; i++) {
            DamageRect d = damage[i];
            present_region(d.x, d.y, d.w, d.h);
        }
        damage_frames++;
    }
    for (int i = 0; i < APPS; i++) {
        if (visible[i] && cm_window_handle(i) && (entire || changed_window || dirty))
            cm_post(cm_window_handle(i), CM_MSG_PAINT, current[i].w, current[i].h);
        last_window_rect[i] = current[i];
        last_window_visible[i] = (uint8_t)visible[i];
        last_window_order[i] = windows[i].order;
    }
    for (int i = 0; i < PANEL_COUNT; i++) panel_was_visible[i] = (uint8_t)panel_visible(i);
    last_start = start_open;
    last_quick = quick_open;
    last_notification = notification_open;
    last_context = context_open;
    last_context_x = context_x;
    last_context_y = context_y;
    last_hover = task_hover;
    last_hover_x = mouse_x;
    last_hover_running = task_hover >= 0 && windows[task_hover].opened && !windows[task_hover].minimized;
    last_hover_visible = hover_visible;
    last_active = active_window();
    last_desktop_focus = desktop_focus;
    last_desktop_hover = desktop_hover;
    last_tab_active = tab_active;
    last_tab_selected = tab_selected;
    last_snap_side = snap_preview_side();
    frame_theme = theme;
    frame_accent = accent_index;
    frame_glass = glass_level;
    frame_wallpaper = wallpaper;
    frame_valid = 1;
    pointer_only = 0;
    taskbar_dirty = 0;
    dirty = 0;
    if (animation_step_allowed) {
        int steps = animation_step_allowed;
        panel_advance();
        for (int i = 0; i < APPS; i++) {
            if (anim_ticks[i] > 0) {
                anim_ticks[i] = anim_ticks[i] > steps ? anim_ticks[i] - steps : 0;
                if (!anim_ticks[i]) { anim_visible[i] = 0; dirty = 1; }
            }
        }
        if (start_anim > 0) start_anim = start_anim > steps ? start_anim - steps : 0;
        if (context_anim > 0) context_anim = context_anim > steps ? context_anim - steps : 0;
        if (quick_anim > 0) quick_anim = quick_anim > steps ? quick_anim - steps : 0;
        if (notification_anim > 0) notification_anim = notification_anim > steps ? notification_anim - steps : 0;
        if (run_anim > 0) run_anim = run_anim > steps ? run_anim - steps : 0;
        for (int i = 0; i < APPS; i++) if (icon_motion[i]) {
            icon_frame[i] = (uint8_t)(icon_frame[i] + steps);
            if (icon_frame[i] >= TASK_ICON_FRAMES) { icon_frame[i] = 0; icon_motion[i] = 0; }
            taskbar_dirty = 1;
            dirty = 1;
        }
    }
}

static int inside(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static int accept_double_click(int category, int item, int x, int y) {
    int dx = x - click_x, dy = y - click_y;
    if (click_wait && click_class == category && click_item == item &&
        dx >= -6 && dx <= 6 && dy >= -6 && dy <= 6) {
        click_wait = 0;
        click_class = -1;
        return 1;
    }
    click_class = category;
    click_item = item;
    click_x = x;
    click_y = y;
    click_wait = CLICK_WINDOW_TICKS;
    return 0;
}

static void icon_kick(int app, int mode) {
    if (app < 0 || app >= APPS) return;
    if (icon_motion[app] == mode) return;
    icon_motion[app] = (uint8_t)mode;
    icon_frame[app] = 0;
    taskbar_dirty = 1;
    dirty = 1;
}

static int sketch_coordinates(int px, int py, int *xx, int *yy) {
    Window *w = &windows[SKETCH];
    int scale = w->w >= 900 ? 3 : 2;
    int x = px - (w->x + 20);
    int y = py - (w->y + 34 + 44);
    if (x < 0 || y < 0 || x >= SKETCH_W * scale || y >= SKETCH_H * scale) return 0;
    *xx = x / scale;
    *yy = y / scale;
    return 1;
}

static void sketch_paint(int px, int py) {
    int xx, yy;
    if (!sketch_coordinates(px, py, &xx, &yy)) return;
    int color = sketch_tool == PINTA_ERASER ? 0 : sketch_color;
    int radius = sketch_tool == PINTA_PENCIL || sketch_tool == PINTA_ERASER ? 0 : sketch_brush;
    if (!sketch_stroking) {
        sketch_x0 = xx;
        sketch_y0 = yy;
        sketch_prev_x = xx;
        sketch_prev_y = yy;
        sketch_stroking = 1;
    }
    if (sketch_tool == PINTA_PENCIL || sketch_tool == PINTA_BRUSH || sketch_tool == PINTA_ERASER) {
        pinta_line(&sketch_pixels[0][0], SKETCH_W, SKETCH_H, sketch_prev_x, sketch_prev_y, xx, yy, color, radius);
        cm_paint_submit(xx, yy, (uint32_t)color, (uint32_t)radius);
    } else if (sketch_tool == PINTA_FILL) {
        pinta_fill(&sketch_pixels[0][0], SKETCH_W, SKETCH_H, xx, yy, color, sketch_fill_stack, SKETCH_W * SKETCH_H);
    } else {
        pinta_stamp(&sketch_pixels[0][0], SKETCH_W, SKETCH_H, xx, yy, color, 0);
    }
    sketch_prev_x = xx;
    sketch_prev_y = yy;
    sketch_dirty = 1;
    dirty = 1;
}

static void sketch_finish(int px, int py) {
    if (!sketch_stroking) return;
    int xx, yy;
    if (sketch_coordinates(px, py, &xx, &yy)) {
        if (sketch_tool == PINTA_LINE) pinta_line(&sketch_pixels[0][0], SKETCH_W, SKETCH_H, sketch_x0, sketch_y0, xx, yy, sketch_color, 0);
        if (sketch_tool == PINTA_RECTANGLE) pinta_rectangle(&sketch_pixels[0][0], SKETCH_W, SKETCH_H, sketch_x0, sketch_y0, xx, yy, sketch_color, 0);
    }
    sketch_stroking = 0;
    dirty = 1;
}

static int handle_dock_click(int x, int y) {
    if (y < sh - 53) return 0;
    if (inside(x, y, 12, sh - 52, 52, 52)) {
        int opening = !start_open;
        dismiss_popup_layers();
        if (opening) {
            start_open = 1;
            menu_query_len = 0;
            menu_query[0] = 0;
            start_anim = 8;
        }
        dirty = 1;
        return 1;
    }
    if (x >= sw - 11) {
        dismiss_popup_layers();
        toggle_desktop();
        return 1;
    }
    int app = taskbar_hit(x, y);
    if (app >= 0) {
        if (windows[app].opened && !windows[app].minimized && active_window() == app) {
            dismiss_popup_layers();
            close_window(app, 1);
        } else open_window(app);
        dirty = 1;
        return 1;
    }
    if (sw >= 700 && x >= sw - 177 && x < sw - 125) {
        int opening = !notification_open;
        dismiss_popup_layers();
        if (opening) {
            notification_open = 1;
            notification_anim = 8;
            notification_unread = 0;
        }
        dirty = 1;
        return 1;
    }
    if (sw >= 520 && x >= sw - 124) {
        int opening = !quick_open;
        dismiss_popup_layers();
        if (opening) {
            quick_open = 1;
            quick_anim = 8;
        }
        dirty = 1;
        return 1;
    }
    if (y >= sh - 52) {
        dismiss_popup_layers();
        return 1;
    }
    return 0;
}

static void handle_click(void) {
    if (tab_active) { finish_switcher(0); return; }
    if (handle_dock_click(mouse_x, mouse_y)) return;
    if (desktop_focus >= 0) { desktop_focus = -1; dirty = 1; }
    if (run_open) {
        int rx = run_left(), ry = run_top();
        if (inside(mouse_x, mouse_y, rx, ry, run_width(), run_height())) {
            if (inside(mouse_x, mouse_y, rx + run_width() - 103, ry + 108, 80, 25)) { run_command(); return; }
            dirty = 1;
            return;
        }
        run_open = 0;
        dirty = 1;
    }
    if (notification_open) {
        int nx = notification_left(), ny = notification_top();
        if (inside(mouse_x, mouse_y, nx, ny, notification_width(), notification_height())) {
            if (inside(mouse_x, mouse_y, nx + notification_width() - 105, ny + 7, 102, 45)) {
                notification_count = 0;
                notification_unread = 0;
            } else {
                for (int i = 0; i < notification_count; i++) {
                    int py = ny + 61 + 55 * i;
                    if (inside(mouse_x, mouse_y, nx + notification_width() - 43, py, 34, 49)) {
                        int at = notification_count - 1 - i;
                        for (int j = at; j < notification_count - 1; j++) {
                            copy(notification_text[j], notification_text[j + 1], 48);
                            copy(notification_time[j], notification_time[j + 1], 9);
                        }
                        notification_count--;
                        if (notification_unread > notification_count) notification_unread = notification_count;
                        break;
                    }
                }
            }
            dirty = 1;
            return;
        }
        notification_open = 0;
        dirty = 1;
    }
    if (quick_open) {
        int qx = sw - quick_width() - 16;
        int qy = sh - 60 - quick_height();
        if (inside(mouse_x, mouse_y, qx, qy, quick_width(), quick_height())) {
            if (inside(mouse_x, mouse_y, qx + 16, qy + 60, 230, 34)) { open_window(CALENDAR); return; }
            if (inside(mouse_x, mouse_y, qx + 16, qy + 102, 110, 34)) { open_window(APPEAR); return; }
            if (inside(mouse_x, mouse_y, qx + 136, qy + 102, 110, 34)) { toggle_desktop(); return; }
            if (inside(mouse_x, mouse_y, qx + 16, qy + 144, 230, 28)) { glass_level = (glass_level + 1) % 3; }
            if (inside(mouse_x, mouse_y, qx + 16, qy + 211, 46, 31)) { volume_level = volume_level >= 10 ? volume_level - 10 : 0; volume_muted = 0; }
            if (inside(mouse_x, mouse_y, qx + 72, qy + 211, 46, 31)) { volume_level = volume_level <= 90 ? volume_level + 10 : 100; volume_muted = 0; }
            if (inside(mouse_x, mouse_y, qx + 129, qy + 211, 117, 31)) { volume_muted = !volume_muted; }
            if (inside(mouse_x, mouse_y, qx + 16, qy + 271, 230, 31)) { run_open = 1; run_anim = 8; quick_open = 0; dirty = 1; return; }
            if (inside(mouse_x, mouse_y, qx + 16, qy + 338, 230, 34)) { toggle_dark_aero(); return; }
            if (inside(mouse_x, mouse_y, qx + 16, qy + 375, 230, 34)) { toggle_quiet(); return; }
            dirty = 1;
            return;
        }
        quick_open = 0;
        dirty = 1;
    }
    if (context_open) {
        if (inside(mouse_x, mouse_y, context_x, context_y, context_width(), context_height())) {
            int selected = (mouse_y - context_y - 13) / 38;
            context_open = 0;
            if (mouse_y >= context_y + 13 && selected >= 0 && selected < 5) {
                if (selected == 1) open_window(APPEAR);
                else if (selected == 2) open_window(WINDOWS_APP);
                else if (selected == 3) toggle_desktop();
                else if (selected == 4) toggle_dark_aero();
            }
            dirty = 1;
            return;
        }
        context_open = 0;
        dirty = 1;
    }
    if (start_open) {
        int my = menu_top();
        int mx = menu_left();
        int mw = menu_width();
        int mh = menu_height();
        if (inside(mouse_x, mouse_y, mx, my, mw, mh)) {
            if (mh >= 420) {
                for (int k = 0; k < APPS; k++) {
                    int app = menu_row_app(k);
                    if (app < 0) break;
                    int col = k % 5, row = k / 5;
                    if (inside(mouse_x, mouse_y, mx + 23 + col * 92, my + 139 + row * 93, 83, 78)) {
                        open_window(app);
                        return;
                    }
                }
                if (inside(mouse_x, mouse_y, mx + 21, my + 362, 216, 38)) { open_window(FILES); return; }
                if (inside(mouse_x, mouse_y, mx + 252, my + 362, 216, 38)) { open_window(NOTES); return; }
                if (inside(mouse_x, mouse_y, mx + mw - 140, my + mh - 34, 70, 32)) reboot_machine();
                if (inside(mouse_x, mouse_y, mx + mw - 69, my + mh - 34, 66, 32)) halt_machine();
            } else {
                int max_rows = (mh - 116) / 24;
                if (max_rows > APPS) max_rows = APPS;
                for (int k = 0; k < max_rows; k++) {
                    int id = menu_row_app(k);
                    if (id < 0) break;
                    if (inside(mouse_x, mouse_y, mx + 13, my + 58 + k * 24, mw - 26, 22)) {
                        open_window(id);
                        return;
                    }
                }
            }
            dirty = 1;
            return;
        }
        start_open = 0;
        dirty = 1;
    }
    int stack[APPS], stack_count = 0;
    for (int i = 0; i < APPS; i++) {
        if (!windows[i].opened || windows[i].minimized) continue;
        int at = stack_count;
        while (at > 0 && windows[stack[at - 1]].order < windows[i].order) {
            stack[at] = stack[at - 1];
            at--;
        }
        stack[at] = i;
        stack_count++;
    }
    for (int layer = 0; layer < stack_count; layer++) {
            int i = stack[layer];
            Window *w = &windows[i];
            int hit_x, hit_y, hit_w, hit_h;
            animated_window_rect(i, &hit_x, &hit_y, &hit_w, &hit_h);
            if (!inside(mouse_x, mouse_y, hit_x, hit_y, hit_w, hit_h) && !inside(mouse_x, mouse_y, w->x, w->y, w->w, w->h)) continue;
            if (active_window() != i) raise_window(i);
            if (cm_window_handle(i)) cm_post(cm_window_handle(i), CM_MSG_MOUSE, mouse_x - hit_x, mouse_y - hit_y);
            if (inside(mouse_x, mouse_y, hit_x + hit_w - 31, hit_y + 4, 27, 25) || inside(mouse_x, mouse_y, w->x + w->w - 31, w->y + 4, 27, 25)) close_window(i, 0);
            else if (inside(mouse_x, mouse_y, hit_x + hit_w - 61, hit_y + 4, 27, 25) || inside(mouse_x, mouse_y, w->x + w->w - 61, w->y + 4, 27, 25)) toggle_maximize(i);
            else if (inside(mouse_x, mouse_y, hit_x + hit_w - 91, hit_y + 4, 27, 25) || inside(mouse_x, mouse_y, w->x + w->w - 91, w->y + 4, 27, 25)) close_window(i, 1);
            else if (!w->maximized && !snap_side[i] && inside(mouse_x, mouse_y, w->x + w->w - 21, w->y + w->h - 21, 21, 21)) {
                resize_app = i;
                drag_dx = w->x + w->w - mouse_x;
                drag_dy = w->y + w->h - mouse_y;
            }
            else if (inside(mouse_x, mouse_y, w->x, w->y, w->w, 34) && !w->maximized) {
                if (snap_side[i]) {
                    int prev_w = w->restore_w;
                    w->x = mouse_x - prev_w / 2;
                    w->y = mouse_y - 16;
                    w->w = prev_w;
                    w->h = w->restore_h;
                    snap_side[i] = 0;
                }
                drag_app = i;
                drag_dx = mouse_x - w->x;
                drag_dy = mouse_y - w->y;
            } else if (i == CALC) {
                int xx = w->x + 2;
                int yy = w->y + 34;
                const char *keys = "789/456*123-C0=+";
                for (int k = 0; k < 16; k++) {
                    if (inside(mouse_x, mouse_y, xx + 20 + (k % 4) * 66, yy + 113 + (k / 4) * 47, 57, 39)) calc_press(keys[k]);
                }
            } else if (i == FILES) {
                int item = files_row_hit(w, mouse_x, mouse_y);
                if (item >= 0) {
                    selected_file = item;
                    file_search_focus = 0;
                    if (accept_double_click(1, item, mouse_x, mouse_y)) files_open_selected();
                } else {
                    click_wait = 0;
                    files_action(w, mouse_x, mouse_y);
                }
            } else if (i == APPEAR) {
                settings_action(w, mouse_x, mouse_y);
            } else if (i == CALENDAR) {
                int xx = w->x + 2;
                int yy = w->y + 34;
                int cw = w->w - 4;
                if (inside(mouse_x, mouse_y, xx + cw - 109, yy + 29, 35, 32) && calendar_shift > -60) { calendar_shift--; calendar_day = 0; }
                if (inside(mouse_x, mouse_y, xx + cw - 65, yy + 29, 35, 32) && calendar_shift < 60) { calendar_shift++; calendar_day = 0; }
                int year, month;
                calendar_month(&year, &month);
                int cell = (cw - 48) / 7;
                if (cell < 25) cell = 25;
                int first = calendar_weekday(year, month);
                for (int day = 1; day <= month_days(year,month); day++) {
                    int k = first + day - 1;
                    int cx = xx + 21 + (k % 7) * cell;
                    int cy = yy + 145 + (k / 7) * 34;
                    if (inside(mouse_x, mouse_y, cx, cy, cell - 5, 29)) calendar_day = day;
                }
            } else if (i == WINDOWS_APP) {
                int xx = w->x + 2;
                int yy = w->y + 34;
                int cw = w->w - 4;
                int row = 0;
                for (int a = 0; a < APPS; a++) {
                    if (!windows[a].opened || a == WINDOWS_APP) continue;
                    int cy = yy + 63 + 36 * row++;
                    if (inside(mouse_x, mouse_y, xx + cw - 163, cy + 3, 78, 26)) { open_window(a); return; }
                    if (inside(mouse_x, mouse_y, xx + cw - 80, cy + 3, 61, 26)) { close_window(a, 0); return; }
                }
            } else if (i == SKETCH) {
                int xx = w->x + 2;
                int yy = w->y + 34;
                int cw = w->w - 4;
                int scale = cw >= 900 ? 3 : 2;
                int tools_x = xx + cw - 247;
                if (inside(mouse_x, mouse_y, xx + 18, yy + 44, SKETCH_W * scale, SKETCH_H * scale)) {
                    snapshot_sketch();
                    sketch_stroking = 0;
                    sketch_paint(mouse_x, mouse_y);
                }
                for (int k = 0; k < PINTA_TOOLS; k++) {
                    int tx = tools_x + (k % 2) * 118, ty = yy + 68 + (k / 2) * 44;
                    if (inside(mouse_x, mouse_y, tx, ty, 107, 36)) { sketch_tool = k; sketch_stroking = 0; }
                }
                if (inside(mouse_x, mouse_y, tools_x + 8, yy + 249, 34, 29) && sketch_brush > 1) sketch_brush--;
                if (inside(mouse_x, mouse_y, tools_x + 52, yy + 249, 34, 29) && sketch_brush < 12) sketch_brush++;
                if (inside(mouse_x, mouse_y, tools_x + 8, yy + 317, 105, 33)) undo_sketch();
                if (inside(mouse_x, mouse_y, tools_x + 122, yy + 317, 105, 33)) redo_sketch();
                if (inside(mouse_x, mouse_y, tools_x + 8, yy + 359, 105, 33)) {
                    snapshot_sketch();
                    for (int row = 0; row < SKETCH_H; row++) for (int col = 0; col < SKETCH_W; col++) sketch_pixels[row][col] = 0;
                    sketch_dirty = 1;
                }
                if (inside(mouse_x, mouse_y, tools_x + 122, yy + 359, 105, 33)) pinta_save_document();
                int bottom = yy + (w->h - 36) - 44;
                for (int k = 0; k < 8; k++) if (inside(mouse_x, mouse_y, xx + 18 + 41 * k, bottom, 29, 29)) sketch_color = k + 1;
                if (cw < 900) {
                    if (inside(mouse_x, mouse_y, xx + w->w - 4 - 199, bottom + 1, 85, 28)) undo_sketch();
                    if (inside(mouse_x, mouse_y, xx + w->w - 4 - 102, bottom + 1, 84, 28)) {
                        snapshot_sketch();
                        for (int row = 0; row < SKETCH_H; row++) for (int col = 0; col < SKETCH_W; col++) sketch_pixels[row][col] = 0;
                        sketch_dirty = 1;
                    }
                }
            }
            dirty = 1;
            return;
    }
    int slot = desktop_slot_hit(mouse_x, mouse_y);
    if (slot >= 0) {
        desktop_focus = slot;
        if (accept_double_click(2, slot, mouse_x, mouse_y)) {
            desktop_focus = -1;
            open_window(desk_shortcuts[slot]);
        }
        dirty = 1;
        return;
    }
    click_wait = 0;
}

static void handle_right_click(void) {
    if (tab_active) finish_switcher(0);
    desktop_focus = -1;
    run_open = 0;
    context_open = 0;
    start_open = 0;
    quick_open = 0;
    notification_open = 0;
    if (mouse_y >= sh - 53) { dirty = 1; return; }
    for (int i = 0; i < APPS; i++) {
        Window *w = &windows[i];
        if (w->opened && !w->minimized && inside(mouse_x, mouse_y, w->x, w->y, w->w, w->h)) { dirty = 1; return; }
    }
    context_x = mouse_x < sw - context_width() - 8 ? mouse_x : sw - context_width() - 8;
    context_y = mouse_y < sh - context_height() - 60 ? mouse_y : sh - context_height() - 60;
    if (context_x < 6) context_x = 6;
    if (context_y < 6) context_y = 6;
    context_open = 1;
    context_anim = 8;
    dirty = 1;
}

static void handle_mouse_byte(uint8_t b) {
    if (mouse_index == 0 && (b & 0xc8u) != 0x08u) return;
    mouse_packet[mouse_index++] = b;
    if (mouse_index != 3) return;
    mouse_index = 0;
    if ((mouse_packet[0] & 0xc8u) != 0x08u) return;
    int dx = (int8_t)mouse_packet[1];
    int dy = (int8_t)mouse_packet[2];
    int nx = mouse_x + dx;
    int ny = mouse_y - dy;
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx >= sw) nx = sw - 1;
    if (ny >= sh) ny = sh - 1;
    if (nx != mouse_x || ny != mouse_y) {
        int hover = taskbar_hit(nx, ny);
        int dhover = desktop_slot_hit(nx, ny);
        if (hover != task_hover || dhover != desktop_hover || context_open || drag_app >= 0 || resize_app >= 0 || (left_down && active_window() == SKETCH) || hover_visible) dirty = 1;
        else pointer_only = 1;
        if (hover != task_hover) { hover_delay = 0; hover_visible = 0; }
        task_hover = hover;
        desktop_hover = dhover;
        mouse_x = nx;
        mouse_y = ny;
        if (left_down && drag_app < 0 && resize_app < 0 && active_window() == SKETCH) sketch_paint(mouse_x, mouse_y);
        if (left_down && resize_app >= 0) {
            Window *w = &windows[resize_app];
            int width = mouse_x + drag_dx - w->x;
            int height = mouse_y + drag_dy - w->y;
            if (width < 500) width = 500;
            if (height < 390) height = 390;
            if (width > sw - w->x - 5) width = sw - w->x - 5;
            if (height > sh - w->y - 57) height = sh - w->y - 57;
            w->w = width;
            w->h = height;
        }
        if (left_down && drag_app >= 0) {
            Window *w = &windows[drag_app];
            w->x = mouse_x - drag_dx;
            w->y = mouse_y - drag_dy;
            if (w->x < 0) w->x = 0;
            if (w->y < 5) w->y = 5;
            if (w->x > sw - 70) w->x = sw - 70;
            if (w->y > sh - 75) w->y = sh - 75;
        }
    }
    int press = mouse_packet[0] & 1;
    int right = mouse_packet[0] & 2;
    if (right && !right_down) handle_right_click();
    right_down = right;
    if (press && !left_down) handle_click();
    if (!press && left_down && drag_app >= 0 && sw >= 1000) {
        int a = drag_app;
        if (mouse_x <= 12) snap_window(a, -1);
        else if (mouse_x >= sw - 13) snap_window(a, 1);
    }
    if (!press && left_down && active_window() == SKETCH) sketch_finish(mouse_x, mouse_y);
    if (!press) { drag_app = -1; resize_app = -1; }
    left_down = press;
}

static const char plain_keys[128] = {
    [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',[0x08]='7',[0x09]='8',[0x0a]='9',[0x0b]='0',
    [0x0c]='-',[0x0d]='=',[0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',[0x15]='y',[0x16]='u',[0x17]='i',
    [0x18]='o',[0x19]='p',[0x1a]='[',[0x1b]=']',[0x1e]='a',[0x1f]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',
    [0x24]='j',[0x25]='k',[0x26]='l',[0x27]=';',[0x28]='\'',[0x29]='`',[0x2b]='\\',[0x2c]='z',[0x2d]='x',
    [0x2e]='c',[0x2f]='v',[0x30]='b',[0x31]='n',[0x32]='m',[0x33]=',',[0x34]='.',[0x35]='/',[0x39]=' '
};

static const char shift_keys[128] = {
    [0x02]='!',[0x03]='@',[0x04]='#',[0x05]='$',[0x06]='%',[0x07]='^',[0x08]='&',[0x09]='*',[0x0a]='(',[0x0b]=')',
    [0x0c]='_',[0x0d]='+',[0x1a]='{',[0x1b]='}',[0x27]=':',[0x28]='"',[0x29]='~',[0x2b]='|',
    [0x33]='<',[0x34]='>',[0x35]='?'
};

static void cm_ui_dispatch(int app, const struct CmMessage *event) {
    if (event->message != CM_MSG_KEY && event->message != CM_MSG_CHAR) return;
    if (app != NOTES && app != CONSOLE) return;
    int key = event->wparam;
    int ch = event->lparam;
    if (app == NOTES && notes_select_all) {
        if (key == 0x0e) { notes_len = 0; notes[0] = 0; notes_dirty = 1; notes_select_all = 0; dirty = 1; return; }
        if (ch || key == 0x1c) { notes_len = 0; notes[0] = 0; notes_select_all = 0; notes_dirty = 1; }
    }
    if (key == 0x0e) {
        if (app == NOTES && notes_len > 0) { notes[--notes_len] = 0; notes_dirty = 1; }
        if (app == CONSOLE && console_len > 0) console_input[--console_len] = 0;
    } else if (key == 0x1c) {
        if (app == NOTES && notes_len < 2046) { notes[notes_len++] = '\n'; notes[notes_len] = 0; notes_dirty = 1; }
        else if (app == CONSOLE) execute_command();
    } else if (ch) {
        if (app == NOTES && notes_len < 2046) { notes[notes_len++] = (char)ch; notes[notes_len] = 0; notes_dirty = 1; }
        if (app == CONSOLE && console_len < 78) { console_input[console_len++] = (char)ch; console_input[console_len] = 0; }
    }
    dirty = 1;
}

static int clipboard_copy_file(int cut) {
    if (selected_file < 0 || selected_file >= files_count() || !storage_ready) return 0;
    const char *name = files_name(selected_file);
    if (storage_slot(name) < 0) return 0;
    copy(clipboard_filename, name, sizeof(clipboard_filename));
    clipboard_type = cut ? 2 : 1;
    return 1;
}

static int clipboard_paste_file(void) {
    if (!storage_ready || (clipboard_type != 1 && clipboard_type != 2)) return 0;
    if (storage_slot(clipboard_filename) < 0) return 0;
    if (clipboard_type == 2) { clipboard_type = 0; return 1; }
    char content[STORAGE_MAX_BYTES + 1];
    int n = storage_load(clipboard_filename, content, sizeof(content));
    if (n < 0) return 0;
    char filename[24] = "Copy_";
    int len = size(filename);
    for (int i = 0; clipboard_filename[i] && len < 18; i++) filename[len++] = clipboard_filename[i];
    filename[len] = 0;
    int suffix = 1;
    while (storage_slot(filename) >= 0 && suffix < 10) {
        filename[len] = (char)('0' + suffix++);
        filename[len + 1] = 0;
    }
    if (storage_slot(filename) >= 0 || !storage_save(filename, content, (uint32_t)n)) return 0;
    files_restore();
    for (int i = FILE_COUNT; i < files_count(); i++) if (same(files_name(i), filename)) selected_file = i;
    file_location = 6;
    return 1;
}

static void clipboard_notes_copy(int cut) {
    clipboard_text_len = notes_len;
    for (int i = 0; i < notes_len; i++) clipboard_text[i] = notes[i];
    clipboard_text[clipboard_text_len] = 0;
    clipboard_type = 3;
    if (cut) { notes_len = 0; notes[0] = 0; notes_dirty = 1; }
    notes_select_all = 0;
    dirty = 1;
}

static void clipboard_notes_paste(void) {
    if (clipboard_type != 3) return;
    if (notes_select_all) { notes_len = 0; notes[0] = 0; notes_select_all = 0; }
    for (int i = 0; i < clipboard_text_len && notes_len < (int)sizeof(notes) - 1; i++) notes[notes_len++] = clipboard_text[i];
    notes[notes_len] = 0;
    notes_dirty = 1;
    dirty = 1;
}

static void handle_key(uint8_t code) {
    if (code == 0xe0) { keyboard_extended = 1; return; }
    int extended = keyboard_extended;
    keyboard_extended = 0;
    int released = (code & 0x80) != 0;
    uint8_t key = code & 0x7f;
    if (extended && (key == 0x5b || key == 0x5c)) {
        if (!released) {
            start_open = !start_open;
            if (start_open) { start_anim = PANEL_STEPS; menu_query_len = 0; menu_query[0] = 0; }
            context_open = 0; quick_open = 0; notification_open = 0;
            dirty = 1;
        }
        return;
    }
    if (key == 0x2a || key == 0x36) { shift_down = !released; return; }
    if (key == 0x1d) { control_down = !released; return; }
    if (key == 0x38) {
        alt_down = !released;
        if (released) finish_switcher(1);
        return;
    }
    if (released) return;
    if (key == 0x01) {
        if (file_search_focus || settings_search_focus) { file_search_focus=0; settings_search_focus=0; dirty=1; return; }
        finish_switcher(0);
        desktop_focus = -1;
        run_open = 0;
        start_open = 0; context_open = 0; quick_open = 0; notification_open = 0;
        menu_query_len = 0; menu_query[0] = 0; dirty = 1; return;
    }
    if (control_down && !alt_down && key == 0x12) { open_window(FILES); return; }
    if (control_down && !alt_down && (key == 0x2e || key == 0x2f || key == 0x2d)) {
        int app = active_window();
        if (app == FILES) {
            int ok = key == 0x2f ? clipboard_paste_file() : clipboard_copy_file(key == 0x2d);
            if (!ok) notify(key == 0x2f ? "Paste failed: attach a writable disk" : "Copy requires a file on disk");
        } else if (app == NOTES) {
            if (key == 0x2f) clipboard_notes_paste();
            else clipboard_notes_copy(key == 0x2d);
        }
        dirty = 1;
        return;
    }
    if (control_down && !alt_down && key == 0x1e && active_window() == NOTES) {
        notes_select_all = 1; dirty = 1; return;
    }
    if (control_down && alt_down && key == 0x32) { toggle_dark_aero(); return; }
    if (alt_down && key == 0x0f) { start_or_cycle_switcher(); return; }
    if (tab_active) return;
    if (control_down && alt_down && key == 0x20) {
        desktop_focus = desktop_focus < 0 ? 0 : -1;
        start_open = 0; context_open = 0; quick_open = 0; notification_open = 0;
        dirty = 1; return;
    }
    if (desktop_focus >= 0) {
        if (key == 0x1c) {
            int app = desk_shortcuts[desktop_focus];
            desktop_focus = -1;
            open_window(app);
            return;
        }
        int next = desktop_focus;
        if (key == 0x48 && next % 4 > 0) next--;
        if (key == 0x50 && next % 4 < 3) next++;
        if (key == 0x4b && next >= 4) next -= 4;
        if (key == 0x4d && next < 4) next += 4;
        if (next != desktop_focus) { desktop_focus = next; dirty = 1; }
        return;
    }
    if (run_open) {
        if (key == 0x0e && run_len > 0) run_input[--run_len] = 0;
        else if (key == 0x1c) { run_command(); return; }
        else if (key == 0x3b || (alt_down && key == 0x13)) { run_open = 0; }
        else if (run_len < 38) {
            char ch = plain_keys[key];
            if (shift_keys[key] && shift_down) ch = shift_keys[key];
            else if (shift_down && ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
            if (ch && !control_down && !alt_down) { run_input[run_len++] = ch; run_input[run_len] = 0; }
        }
        dirty = 1;
        return;
    }
    if (key == 0x3b) { context_open = 0; quick_open = 0; notification_open = 0; start_open = !start_open; if (start_open) start_anim = 8; menu_query_len = 0; menu_query[0] = 0; dirty = 1; return; }
    if (context_open) { context_open = 0; dirty = 1; return; }
    if (quick_open) { quick_open = 0; dirty = 1; return; }
    if (notification_open) { notification_open = 0; dirty = 1; return; }
    if (control_down && alt_down && key == 0x31) {
        notification_open = !notification_open;
        if (notification_open) { notification_unread = 0; notification_anim = 8; }
        quick_open = 0;
        dirty = 1;
        return;
    }
    if (start_open) {
        if (key == 0x0e && menu_query_len > 0) menu_query[--menu_query_len] = 0;
        else if (key == 0x1c) {
            int id = menu_row_app(0);
            if (id >= 0) open_window(id);
            return;
        } else if (menu_query_len < 30) {
            char ch = plain_keys[key];
            if (shift_keys[key] && shift_down) ch = shift_keys[key];
            if (ch && !control_down && !alt_down) {
                menu_query[menu_query_len++] = ch;
                menu_query[menu_query_len] = 0;
            }
        }
        dirty = 1;
        return;
    }
    if (alt_down && key == 0x20) { toggle_desktop(); return; }
    if (control_down && alt_down && (key == 0x48 || key == 0x50)) {
        int app = active_window();
        if (app >= 0) {
            if (key == 0x48) toggle_maximize(app);
            else close_window(app, 1);
        }
        return;
    }
    if (control_down && alt_down && (key == 0x4b || key == 0x4d)) {
        int a = active_window();
        if (a >= 0) snap_window(a, key == 0x4b ? -1 : 1);
        return;
    }
    if (key == 0x57) { int a = active_window(); if (a >= 0) toggle_maximize(a); return; }
    if (alt_down && key == 0x3e) {
        int a = active_window();
        if (a >= 0) close_window(a, 0);
        dirty = 1;
        return;
    }
    if (control_down && key >= 0x02 && key <= 0x08) { open_window(pinned_apps[key - 0x02]); return; }
    if (control_down && key == 0x13) { run_open = !run_open; if (run_open) { run_anim = 8; start_open = 0; context_open = 0; quick_open = 0; notification_open = 0; } dirty = 1; return; }
    if (control_down && active_window() == SKETCH && key == 0x1f) { pinta_save_document(); return; }
    if (control_down && active_window() == SKETCH && key == 0x2c) { undo_sketch(); return; }
    if (control_down && active_window() == SKETCH && key == 0x15) { redo_sketch(); return; }
    if (control_down && key == 0x1f && active_window() == NOTES) { notes_commit(); return; }
    if (control_down && key == 0x31) { open_window(NOTES); return; }
    if (control_down && key == 0x2e) { open_window(CONSOLE); return; }
    if (control_down && key == 0x14) { open_window(CLOCK); return; }
    if (control_down && key == 0x1f) { open_window(SKETCH); return; }
    if (control_down && key == 0x20) { open_window(APPEAR); return; }
    if (control_down && key == 0x25) { open_window(CALENDAR); return; }
    if (control_down && key == 0x11) { open_window(WINDOWS_APP); return; }
    int app = active_window();
    if (app == FILES) {
        if (control_down && key == 0x21) { file_search_focus=1; dirty=1; return; }
        if (file_search_focus) {
            if (key == 0x0e && file_search_len) file_search[--file_search_len]=0;
            else if (key == 0x1c) file_search_focus=0;
            else if (file_search_len < 30) {
                char letter=plain_keys[key];
                if (shift_keys[key] && shift_down) letter=shift_keys[key];
                if (letter && !control_down && !alt_down) { file_search[file_search_len++]=letter; file_search[file_search_len]=0; }
            }
            dirty=1; return;
        }
        if (key == 0x1c) { files_open_selected(); return; }
        if (key == 0x48 || key == 0x50) {
            int pos=0, shown=files_visible_count();
            for (int i=0;i<shown;i++) if (files_item(i)==selected_file) pos=i;
            pos += key == 0x48 ? -1 : 1;
            if (pos < 0) pos=0;
            if (pos >= shown) pos=shown-1;
            if (pos >= 0) selected_file=files_item(pos);
            dirty=1; return;
        }
    }
    if (app == APPEAR) {
        if (control_down && key == 0x21) { settings_search_focus=1; dirty=1; return; }
        if (settings_search_focus) {
            if (key == 0x0e && settings_search_len) settings_search[--settings_search_len]=0;
            else if (key == 0x1c) { settings_search_apply(); return; }
            else if (settings_search_len < 30) {
                char letter=plain_keys[key];
                if (shift_keys[key] && shift_down) letter=shift_keys[key];
                if (letter && !control_down && !alt_down) { settings_search[settings_search_len++]=letter; settings_search[settings_search_len]=0; }
            }
            dirty=1; return;
        }
    }
    if (app != NOTES && app != CONSOLE && app != CALC) return;
    char ch = plain_keys[key];
    if (shift_keys[key] && shift_down) ch = shift_keys[key];
    else if (shift_down && ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
    if (app == CALC) {
        if (key == 0x0e) calc_press('B');
        else if (key == 0x1c) calc_press('=');
        else if (key == 0x2e && !control_down) calc_press('C');
        else if ((ch >= '0' && ch <= '9') || ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '=') calc_press(ch);
        return;
    }
    uint32_t handle = cm_window_handle(app);
    if (handle && (key == 0x0e || key == 0x1c || (ch && !control_down))) {
        cm_post(handle, key == 0x0e || key == 0x1c ? CM_MSG_KEY : CM_MSG_CHAR, key, ch && !control_down ? ch : 0);
        cm_pump(16, cm_ui_dispatch);
    }
    dirty = 1;
}

static void wait_input_ready(void) {
    for (int i = 0; i < 100000; i++) if (!(in8(0x64) & 2)) return;
}

static void write_controller(uint8_t cmd) {
    wait_input_ready();
    out8(0x64, cmd);
}

static void write_data(uint8_t value) {
    wait_input_ready();
    out8(0x60, value);
}

static int read_data(void) {
    for (int i = 0; i < 1000000; i++) if (in8(0x64) & 1) return in8(0x60);
    return -1;
}

static void write_mouse(uint8_t value) {
    write_controller(0xd4);
    write_data(value);
    read_data();
}

static void init_mouse(void) {
    for (int i = 0; i < 32 && (in8(0x64) & 1); i++) in8(0x60);
    write_controller(0xa8);
    write_controller(0x20);
    int config = read_data();
    if (config >= 0) {
        write_controller(0x60);
        write_data((uint8_t)((config | 3) & ~0x20));
    }
    write_mouse(0xf6);
    write_mouse(0xf4);
}

static int hover_delay_target(void) { return 120; }

static void update_hover_timer(int pulse) {
    int active = task_hover >= 0 && !start_open && !context_open && !quick_open && !notification_open && !run_open && !tab_active;
    if (!active) {
        if (hover_delay || hover_visible) dirty = 1;
        hover_delay = 0;
        hover_visible = 0;
        return;
    }
    if (!pulse) return;
    if (hover_delay < hover_delay_target()) hover_delay++;
    if (!hover_visible && hover_delay >= hover_delay_target()) { hover_visible = 1; dirty = 1; }
}

static int animations_pending(void) {
    for (int i = 0; i < PANEL_COUNT; i++) if (panel_step[i] != panel_goal[i]) return 1;
    if (start_anim || context_anim || quick_anim || notification_anim || run_anim) return 1;
    for (int i = 0; i < APPS; i++) if (anim_ticks[i] > 0 || icon_motion[i]) return 1;
    return 0;
}

static uint16_t read_frame_counter(void) {
    out8(0x43, 0x00);
    uint16_t lo = in8(0x40);
    uint16_t hi = in8(0x40);
    return (uint16_t)(lo | (hi << 8));
}

static int advance_frame_counter(uint16_t sample) {
    uint32_t elapsed = timer_counter >= sample ? (uint32_t)(timer_counter - sample) : (uint32_t)(timer_counter + 65535u - sample);
    timer_counter = sample;
    if (elapsed == 0) {
        if (++timer_unchanged >= 2048) {
            timer_unchanged = 0;
            timer_accum = 0;
            return 1;
        }
        return 0;
    }
    timer_unchanged = 0;
    timer_accum += elapsed;
    if (timer_accum < 9943) return 0;
    int pulses = (int)(timer_accum / 9943);
    timer_accum %= 9943;
    return pulses > 6 ? 6 : pulses;
}

static void init_frame_clock(void) {
    out8(0x43, 0x34);
    out8(0x40, 0xff);
    out8(0x40, 0xff);
    timer_counter = read_frame_counter();
    timer_accum = 0;
    timer_poll = 0;
    timer_unchanged = 0;
}

static int frame_clock_tick(void) {
    if (++timer_poll < 32) return 0;
    timer_poll = 0;
    return advance_frame_counter(read_frame_counter());
}

static void init_cpu(void) {
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
    ((uint32_t *)cpu_vendor)[0] = b;
    ((uint32_t *)cpu_vendor)[1] = d;
    ((uint32_t *)cpu_vendor)[2] = c;
    cpu_vendor[12] = 0;
}

void kmain(void) {
    sw = (int)boot_width;
    sh = (int)boot_height;
    if (sw < 1 || sh < 1 || sw > 1920 || sh > 1080) for (;;) __asm__ volatile("cli; hlt");
    init_cpu();
    if (boot_bpp == 8) program_palette();
    mouse_x = sw / 2;
    mouse_y = sh / 2;
    for (int i = 0; i < APPS; i++) {
        int base_w = sw >= 1700 ? 1050 : sw >= 1100 ? 760 : sw >= 700 ? 590 : sw - 25;
        int base_h = sh >= 1000 ? 700 : sh >= 700 ? 485 : sh >= 540 ? 430 : sh - 67;
        windows[i].x = (sw >= 1700 ? (sw - base_w) / 2 : sw >= 1100 ? (sw - base_w) / 2 : sw >= 700 ? 145 : 12) + 14 * (i % 4);
        windows[i].y = (sh >= 1000 ? (sh - base_h) / 2 - 12 : sh >= 700 ? 87 : sh >= 490 ? 65 : 9) + 15 * (i % 4);
        windows[i].w = base_w;
        windows[i].h = base_h;
        windows[i].opened = 0;
    }
    if (sw < 500) {
        for (int i = 0; i < APPS; i++) {
            windows[i].x = 8;
            windows[i].y = 8;
            windows[i].w = sw - 16;
            windows[i].h = sh - 60;
        }
    }
    notes_len = size(notes);
    NtInitializeIoManager();
    if (storage_mount()) {
        nt_registry_load();
        int loaded_theme = 0;
        if (nt_restore_theme(&loaded_theme)) {
            theme = loaded_theme;
            if (theme != 3) previous_theme = theme;
        }
        notes_select("notes.txt");
        pinta_load_document();
        sketch_disk_loaded = 1;
        files_restore();
        log_write("QuartzieFS disk mounted at D: (ATA IDE)");
    } else log_write("RAM only: attach Quartzie disk to enable persistence");
    if (theme < 0 || theme > 3) theme = 0;
    if (previous_theme < 0 || previous_theme > 2) previous_theme = 0;
    if (accent_index < 0 || accent_index > 3) accent_index = 0;
    log_write("Quartzie kernel 0.32 / rebuilt UI dispatch");
    init_mouse();
    init_frame_clock();
    refresh_clock();
    notify("Quartzie 0.32 is ready");
    repaint();
#ifdef __i386__
    cm_userspace_ready = 0;
    log_write("Safe desktop: protected workers disabled until useron");
#endif
    uint32_t cycles = 0;
    for (;;) {
        if ((++cycles & 16383u) == 0) refresh_clock();
#ifdef __i386__
        if (cm_userspace_ready && (cm_paint_pending() || (!(cycles & 65535u) && cm_proc_has_work()))) {
            cm_proc_run();
            if (cm_paint_collect(&sketch_pixels[0][0], SKETCH_W * SKETCH_H)) dirty = 1;
            if (pe_reported_exits != cm_pe_completed) {
                pe_reported_exits=cm_pe_completed;
                char line[72]="PE32 process exited: ", number_buffer[12];
                number(number_buffer,cm_pe_last_exit);
                append(line,number_buffer,72);
                log_write(line);
            }
            init_frame_clock();
        }
#endif
        for (int step = 0; step < 64; step++) {
            uint8_t status = in8(0x64);
            if (!(status & 1)) break;
            uint8_t byte = in8(0x60);
            if (status & 0x20) handle_mouse_byte(byte);
            else handle_key(byte);
        }
        cm_pump(16, cm_ui_dispatch);
        int need_pulse = animations_pending() || task_hover >= 0 || hover_visible || run_open || click_wait;
        int animation_pulse = need_pulse ? frame_clock_tick() : 0;
        if (animation_pulse) {
            update_hover_timer(1);
            if (click_wait) click_wait--;
        } else update_hover_timer(0);
        if (dirty || (animation_pulse && animations_pending())) {
            animation_step_allowed = animation_pulse;
            repaint();
            animation_step_allowed = 1;
        } else if (pointer_only) move_pointer_only();
        __asm__ volatile("pause");
    }
}
