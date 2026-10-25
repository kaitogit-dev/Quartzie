#ifndef QUARTZIE_UI_H
#define QUARTZIE_UI_H
#include <stdint.h>
#include <stddef.h>
#include "storage.h"
#include "media.h"
#define Q_MAX_W 1920
#define Q_MAX_H 1080
#define Q_UI_SCRATCH_PIXELS (4*((Q_MAX_W/4+16)*(Q_MAX_H/4+16))+3*Q_MAX_W*Q_MAX_H)
#define Q_APPS 9
#define Q_DOCK_APPS 7
#define Q_FILES 0
#define Q_NOTES 1
#define Q_TERMINAL 2
#define Q_CALCULATOR 3
#define Q_SETTINGS 4
#define Q_ABOUT 5
#define Q_STORE 6
#define Q_PHOTOS 7
#define Q_MEDIA 8
#define Q_SETTINGS_PAGES 6
typedef struct {
    int x,y,w,h,open,z,minimized,maximized,restore_x,restore_y,restore_w,restore_h;
    int tween_step,tween_kind,tween_x,tween_y,tween_w,tween_h;
} QWindow;
typedef struct {
    int width,height,mouse_x,mouse_y,dirty,theme,accent;
    int menu,notifications,quick,active,drag,drag_x,drag_y;
    int pressed,last_button,frame_count,hover;
    int drag_start_x,drag_start_y,drag_moved;
    int note_length,note_cursor,note_scroll,note_dirty,command_length,calc_length;
    int terminal_volume,terminal_count,photo_zoom,store_page;
    uint32_t audio_display_us;
    char terminal_path[QFS_PATH],terminal_lines[7][96];
    int menu_search_length,calc_history_count;
    int settings_page,settings_search_length;
    char settings_search[32];
    char notes[2048],command[128],calc[64],answer[64],status[96];
    char menu_search[24],calc_history[3][64];
    QWindow windows[Q_APPS];
    QStorage *storage;
    QFsEntry files[QFS_ENTRIES];
    int file_volume,file_count,file_scroll,file_selected,file_confirm;
    int file_click_index,file_operation,file_input_length;
    int file_view,file_sort,file_desc,file_menu,file_show_hidden,file_extensions,file_compact,file_details,file_nav;
    int file_search_active,file_address_active,file_search_length,file_address_length,file_visible_count,file_submenu,file_checkboxes;
    int file_location,file_history_count,file_history_index,file_clip_volume,file_clip_cut,file_clip_dir,file_checked_count,file_clip_count;
    uint8_t file_checked[QFS_ENTRIES];
    int file_order[QFS_ENTRIES],file_history_volumes[12];
    char file_search[64],file_address[QFS_PATH],file_clip_path[QFS_PATH],file_clip_name[72];
    char file_history_paths[12][QFS_PATH];
    char file_input[72];
    uint32_t file_click_elapsed_us;
    char file_path[QFS_PATH],file_status[80],note_path[QFS_PATH],media_name[72];
    int note_volume,media_volume;
    uint8_t *media_buffer;
    size_t media_capacity;
    QMedia media;
    uint32_t *pixels,*wallpaper,*blurred;
    uint32_t *drag_base,*dock_background;
    int dock_cache_valid;
    int drag_cache_valid,drag_cache_app,drag_prev_x,drag_prev_y;
    int clip_left,clip_top,clip_right,clip_bottom;
    int damage_valid,damage_x,damage_y,damage_w,damage_h;
    int hover_only,hover_last;
    int render_app,glass_cached_app,render_topbar;
    int animations_enabled,dock_lift[Q_APPS],dock_pulse[Q_APPS];
    int start_hover,start_lift,start_pulse;
    int menu_hover,menu_lift[Q_APPS];
    int scene_tween_pending,scene_tween_active,scene_tween_step,scene_tween_kind;
    int scene_x0,scene_y0,scene_x1,scene_y1,scene_render_final,scene_tween_app;
    uint32_t scene_elapsed_us,scene_paint_us,dock_elapsed_us;
} QDesktop;
void q_ui_set_scratch(uint32_t *storage);
void q_init(QDesktop *q,int width,int height,uint32_t *pixels,uint32_t *wallpaper,uint32_t *blurred);
void q_draw(QDesktop *q);
void q_attach_storage(QDesktop *q,QStorage *storage,uint8_t *buffer,size_t capacity);
void q_enable_animations(QDesktop *q);
void q_tick(QDesktop *q);
void q_tick_timed(QDesktop *q,uint32_t elapsed_us);
void q_drag_buffers(QDesktop *q,uint32_t *background);
void q_dock_buffers(QDesktop *q,uint32_t *background);
void q_click(QDesktop *q,int x,int y);
void q_press(QDesktop *q,int x,int y);
void q_release(QDesktop *q);
void q_move(QDesktop *q,int x,int y);
void q_key(QDesktop *q,uint16_t ch,uint16_t scan);
void q_set_theme(QDesktop *q,int dark);
void q_open(QDesktop *q,int app);
int q_dock_hit(QDesktop *q,int x,int y);
#endif
