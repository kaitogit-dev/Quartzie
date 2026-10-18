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
#define require(x) do { if (!(x)) { fprintf(stderr, "FAIL compositor line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void image_matches_display(void) {
    uint32_t *frame = (uint32_t *)(uintptr_t)boot_fb;
    for (int i = 0; i < sw * sh; i++) {
        if (canvas[i] != frame[i]) {
            fprintf(stderr, "Mismatch at %d,%d  canvas=%06x display=%06x\n",i%sw,i/sw,canvas[i],frame[i]);
            exit(1);
        }
    }
}
static void frame(void) { repaint(); image_matches_display(); }
int main(void) {
    require(mmap((void *)0x400000, 1280*720*4, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0) != MAP_FAILED);
    require(mmap((void *)0xD00000, 1280*720*4, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0) != MAP_FAILED);
    require(mmap((void *)0x2000000, 1280*720*4, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0) != MAP_FAILED);
    sw = 1280; sh = 720;
    for (int i = 0; i < APPS; i++) { windows[i].x=260+14*(i%4); windows[i].y=87+15*(i%4); windows[i].w=760; windows[i].h=485; }
    mouse_x=112; mouse_y=108;
    frame();
    require(pointer_saved && !dirty);
    mouse_x=114;mouse_y=110;
    pointer_only=1;
    move_pointer_only();
    image_matches_display();
    mouse_x=1100;mouse_y=612;
    open_window(FILES);
    animation_step_allowed=0;
    frame();
    require(anim_ticks[FILES]==WINDOW_OPEN_STEPS && !dirty);
    for (int i=0;i<6;i++) frame();
    require(anim_ticks[FILES]==WINDOW_OPEN_STEPS);
    animation_step_allowed=1;
    for (int i=0;i<TASK_ICON_FRAMES+2;i++) frame();
    require(anim_ticks[FILES]==0 && !anim_visible[FILES] && !dirty);
    close_window(FILES,0);
    for (int i=0;i<8;i++) frame();
    require(!windows[FILES].opened && !anim_visible[FILES] && !dirty);
    mouse_x=380;mouse_y=180;
    dirty=1;
    frame();
    quick_open=1;quick_anim=8;dirty=1;
    for(int i=0;i<9;i++)frame();
    quick_open=0;dirty=1;
    frame();
    start_open=1;start_anim=8;dirty=1;
    for(int i=0;i<9;i++)frame();
    start_open=0;dirty=1;
    frame();
    open_window(NOTES);
    for(int i=0;i<9;i++)frame();
    mouse_x=280;mouse_y=110;pointer_only=1;
    move_pointer_only();image_matches_display();
    printf("PASS: cursor never missing from composed framebuffer, no stale cursor ghosts, paced frame progression, clean exit animation, correct partial region presentation\n");
    return 0;
}
