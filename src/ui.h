#ifndef QUARTZIE_UI_H
#define QUARTZIE_UI_H
#include <stdint.h>
#define Q_MAX_W 1920
#define Q_MAX_H 1080
#define Q_APPS 6
#define Q_FILES 0
#define Q_NOTES 1
#define Q_TERMINAL 2
#define Q_CALCULATOR 3
#define Q_SETTINGS 4
#define Q_ABOUT 5
typedef struct {
    int x,y,w,h,open,z,minimized,maximized,restore_x,restore_y,restore_w,restore_h;
    int tween_step,tween_kind,tween_x,tween_y,tween_w,tween_h;
} QWindow;
typedef struct {
    int width,height,mouse_x,mouse_y,dirty,theme,accent;
    int menu,notifications,quick,active,drag,drag_x,drag_y;
    int pressed,last_button,frame_count,hover;
    int note_length,command_length,calc_length;
    char notes[2048],command[128],calc[64],answer[64],status[96];
    QWindow windows[Q_APPS];
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
void q_init(QDesktop *q,int width,int height,uint32_t *pixels,uint32_t *wallpaper,uint32_t *blurred);
void q_draw(QDesktop *q);
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
void q_open(QDesktop *q,int app);
int q_dock_hit(QDesktop *q,int x,int y);
#endif
