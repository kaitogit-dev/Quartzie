#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../src/boot.c"
static uint32_t mock_frame[128*128];
static uint32_t mock_scene[128*128];
static uint32_t original[128*128];
static int pointer_event;
static EFI_STATUS EFIAPI fake_relative_state(EFI_SIMPLE_POINTER_PROTOCOL *self,EFI_SIMPLE_POINTER_STATE *state){
    (void)self;
    if(!pointer_event)return 1;
    pointer_event--;
    memset(state,0,sizeof(*state));
    state->RelativeMovementX=4;
    return 0;
}
static EFI_SIMPLE_POINTER_MODE fake_mode={400,400,0,0,0};
static EFI_SIMPLE_POINTER_PROTOCOL fake_pointer={0,fake_relative_state,0,&fake_mode};
#define VERIFY(cond) do{if(!(cond)){fprintf(stderr,"FAIL cursor test line %d: %s\n",__LINE__,#cond);exit(1);}}while(0)
int main(void){
    desktop.width=128;desktop.height=128;desktop.pixels=mock_scene;
    screen=mock_frame;scanline=128;pixel_format=1;
    for(int y=0;y<128;y++)for(int x=0;x<128;x++){
        mock_scene[y*128+x]=(uint32_t)(0x104090+(x*7%40)*0x10000+(y*3%20)*0x100);
        mock_frame[y*128+x]=mock_scene[y*128+x];
    }
    memcpy(original,mock_frame,sizeof(original));
    cursor_prepare();
    int visible=0;
    for(int i=0;i<27*31;i++)visible+=cursor_covered[i]!=0;
    VERIFY(visible>100&&visible<27*31);
    cursor_draw(20,20);
    VERIFY(memcmp(original,mock_frame,sizeof(original))!=0);
    cursor_draw(60,60);
    for(int y=20;y<51;y++)for(int x=20;x<47;x++)VERIFY(mock_frame[y*128+x]==original[y*128+x]);
    cursor_hide();
    VERIFY(memcmp(original,mock_frame,sizeof(original))==0);
    cursor_draw(60,60);
    uint32_t icon_before[27*31];
    for(int y=0;y<31;y++)for(int x=0;x<27;x++)icon_before[y*27+x]=mock_frame[(60+y)*128+60+x];
    for(int y=0;y<12;y++)for(int x=0;x<12;x++)mock_scene[y*128+x]=0x5544bb;
    present_damage(0,0,12,12);
    VERIFY(cursor_old_x==60&&cursor_old_y==60);
    for(int y=0;y<31;y++)for(int x=0;x<27;x++)VERIFY(icon_before[y*27+x]==mock_frame[(60+y)*128+60+x]);
    for(int y=60;y<91;y++)for(int x=60;x<87;x++)mock_scene[y*128+x]=0x4477cc;
    present_damage(55,55,40,40);
    cursor_hide();
    for(int y=0;y<128;y++)for(int x=0;x<128;x++)VERIFY(mock_frame[y*128+x]==mock_scene[y*128+x]);
    pointer_relative=&fake_pointer;
    pointer_absolute=0;
    pointer_ps2=0;
    pointer_x=18;
    pointer_y=18;
    cursor_draw(pointer_x,pointer_y);
    desktop.dirty=0;
    for(int i=0;i<8;i++){
        pointer_event=1;
        pointer_sample(0);
        VERIFY(pointer_x==19+i&&pointer_y==18);
        VERIFY(cursor_old_x==pointer_x&&cursor_old_y==pointer_y);
        VERIFY(desktop.dirty==0);
    }
    pointer_event=0;
    pointer_sample(0);
    VERIFY(pointer_x==26&&desktop.dirty==0);
    pointer_relative=0;
    cursor_hide();
    for(int y=0;y<128;y++)for(int x=0;x<128;x++)VERIFY(mock_frame[y*128+x]==mock_scene[y*128+x]);
    puts("PASS: idle pointer movement with fractional precision, no desktop redraw and cursor restoration");
    return 0;
}
