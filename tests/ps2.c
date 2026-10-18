#include "ps2.h"
#include <stdio.h>
#include <stdlib.h>
#define check(x) do{if(!(x)){fprintf(stderr,"FAIL PS2 line %d\n",__LINE__);exit(1);}}while(0)
int main(void){
    QPs2Mouse mouse={0};int dx=0,dy=0,button=0;
    check(!q_ps2_feed(&mouse,0xfa,&dx,&dy,&button));
    check(!q_ps2_feed(&mouse,0xfe,&dx,&dy,&button));
    check(!q_ps2_feed(&mouse,0x09,&dx,&dy,&button));
    check(!q_ps2_feed(&mouse,0x07,&dx,&dy,&button));
    check(q_ps2_feed(&mouse,0x03,&dx,&dy,&button));
    check(dx==7&&dy==-3&&button==1);
    check(!q_ps2_feed(&mouse,0x38,&dx,&dy,&button));
    check(!q_ps2_feed(&mouse,0xf9,&dx,&dy,&button));
    check(q_ps2_feed(&mouse,0xfc,&dx,&dy,&button));
    check(dx==-7&&dy==4&&button==0);
    check(!q_ps2_feed(&mouse,0xc8,&dx,&dy,&button));
    check(mouse.index==0);
    puts("PASS: PS2 packet synchronization, movement, buttons and overflow rejection");
    return 0;
}
