#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/ui.c"
static uint32_t framebuffer[256*256];
static uint32_t wall[256*256];
static uint32_t work[256*256];
static QDesktop desktop;
static void require(int ok){if(!ok){fputs("FAIL: backdrop blur\n",stderr);exit(1);}}
static uint32_t channel_blend(uint32_t a,uint32_t b,int weight){
    int inverse=256-weight;
    return rgb((int)((a>>16&255)*inverse+(b>>16&255)*weight)>>8,
        (int)((a>>8&255)*inverse+(b>>8&255)*weight)>>8,
        (int)((a&255)*inverse+(b&255)*weight)>>8);
}
static uint32_t sampled_blur(const uint32_t *data,int x,int y,int width,int height){
    int ax=clamp(x/4,0,width-1),ay=clamp(y/4,0,height-1);
    int bx=clamp(ax+1,0,width-1),by=clamp(ay+1,0,height-1);
    int fx=x%4,fy=y%4;
    if(fx<0)fx=0;
    if(fy<0)fy=0;
    int w00=(4-fx)*(4-fy),w10=fx*(4-fy),w01=(4-fx)*fy,w11=fx*fy;
    uint32_t a=data[ay*width+ax],b=data[ay*width+bx];
    uint32_t c=data[by*width+ax],d=data[by*width+bx];
    return rgb(((a>>16&255)*w00+(b>>16&255)*w10+(c>>16&255)*w01+(d>>16&255)*w11)/16,
        ((a>>8&255)*w00+(b>>8&255)*w10+(c>>8&255)*w01+(d>>8&255)*w11)/16,
        ((a&255)*w00+(b&255)*w10+(c&255)*w01+(d&255)*w11)/16);
}
int main(void){
    for(uint32_t i=0;i<3072;i++){
        uint32_t a=(i*1664525u+1013904223u)&0xffffffu;
        uint32_t b=(i*22695477u+1u)&0xffffffu;
        int weight=(int)(i%257u);
        require(blend(a,b,weight)==channel_blend(a,b,weight));
    }
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
    for(int y=0;y<23;y++)for(int x=0;x<23;x++){
        uint32_t c=glass_final[(size_t)y*64+x];
        require(glass_fetch(glass_final,x*4+1,y*4+2,0,0,64,64)==
            sampled_blur(glass_final,x*4+1,y*4+2,64,64));
        require((c&0xff000000u)==0);
    }
    int samples[6][2]={{79,70},{82,80},{120,135},{180,99},{220,178},{248,245}};
    desktop.render_app=Q_FILES;
    desktop.glass_cached_app=-1;
    desktop.windows[Q_FILES].x=65;
    desktop.windows[Q_FILES].y=60;
    desktop.windows[Q_FILES].w=174;
    desktop.windows[Q_FILES].h=150;
    glass_filter(&desktop,0,0,64,64);
    uint32_t reference[6];
    for(int i=0;i<6;i++)reference[i]=glass_fetch(glass_final,samples[i][0],samples[i][1],0,0,64,64);
    prepare_window_glass(&desktop,Q_FILES,&desktop.windows[Q_FILES]);
    require(glass_cache_cols<64&&glass_cache_rows<64);
    for(int i=0;i<6;i++){
        int px=samples[i][0],py=samples[i][1];
        if(px>=65&&px<239&&py>=60&&py<210)
            require(desktop.blurred[(size_t)py*256+px]==reference[i]);
    }
    puts("PASS: matching packed color math, cropped glass sampling and rounded edges");
    return 0;
}
