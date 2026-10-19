#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/ui.c"
static uint32_t framebuffer[256*256];
static uint32_t wall[256*256];
static uint32_t work[256*256];
static QDesktop desktop;
static void require(int ok){if(!ok){fputs("FAIL: backdrop blur\n",stderr);exit(1);}}
int main(void){
    q_init(&desktop,256,256,framebuffer,wall,work);
    for(int y=0;y<256;y++)for(int x=0;x<256;x++){
        uint32_t c=((x/3+y/3)&1)?0xffffff:0;
        desktop.pixels[(size_t)y*256+x]=c;
    }
    int a=desktop.pixels[128*256+125]>>16&255;
    int b=desktop.pixels[128*256+128]>>16&255;
    require(a!=b);
    surface(&desktop,40,40,178,178,24,0xcce0f0,65);
    a=desktop.pixels[128*256+125]>>16&255;
    b=desktop.pixels[128*256+128]>>16&255;
    require(a>55&&a<220&&b>55&&b<220);
    require(absval(a-b)<40);
    require(desktop.pixels[40*256+40]!=desktop.pixels[128*256+128]);
    puts("PASS: actual softened backdrop, translucent glass and rounded edges");
    return 0;
}
