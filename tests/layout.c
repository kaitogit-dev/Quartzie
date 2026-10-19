#include "ui.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t pixels[1280*720];
static uint32_t background[1280*720];
static uint32_t blurred[1280*720];
static QDesktop ui;

static void require(int condition,int line){
    if(!condition){fprintf(stderr,"FAIL layout line %d\n",line);exit(1);}
}
#define CHECK(condition) require((condition),__LINE__)

static int difference(int y){
    int x=130;
    uint32_t foreground=pixels[(size_t)y*1280+x];
    uint32_t source=background[(size_t)y*1280+x];
    int r=(int)(foreground>>16&255)-(int)(source>>16&255);
    int g=(int)(foreground>>8&255)-(int)(source>>8&255);
    int b=(int)(foreground&255)-(int)(source&255);
    return abs(r)+abs(g)+abs(b);
}

int main(void){
    q_init(&ui,1280,720,pixels,background,blurred);
    q_draw(&ui);
    for(int y=112;y<334;y+=17)for(int x=920;x<1240;x+=23){
        CHECK(pixels[(size_t)y*1280+x]==background[(size_t)y*1280+x]);
    }
    q_click(&ui,1040,200);
    for(int i=0;i<Q_APPS;i++)CHECK(!ui.windows[i].open);
    int a=difference(446),b=difference(454),c=difference(466),d=difference(480);
    CHECK(a>b&&b>c&&c>=d);
    CHECK(a<100);
    q_open(&ui,Q_FILES);
    q_draw(&ui);
    QWindow *w=&ui.windows[Q_FILES];
    int x=w->x+260,top=w->y+51,below=w->y+65;
    uint32_t p=pixels[(size_t)top*1280+x],v=pixels[(size_t)below*1280+x];
    CHECK(abs((int)(p>>16&255)-(int)(v>>16&255))<35);
    CHECK(abs((int)(p>>8&255)-(int)(v>>8&255))<35);
    CHECK(abs((int)(p&255)-(int)(v&255))<35);
    puts("PASS: no quick access panel, no shadow strip and integrated window header");
    return 0;
}
