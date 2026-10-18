#include "ui.h"
#include <stdint.h>
#include <stddef.h>
extern const uint8_t *q_glyph(int c);
static int clamp(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
static int within(int x,int y,int a,int b,int w,int h){return x>=a&&y>=b&&x<a+w&&y<b+h;}
static uint32_t mix(uint32_t a,uint32_t b,int alpha){
    int inv=256-alpha;
    uint32_t r=(((a>>16&255)*inv+(b>>16&255)*alpha)>>8);
    uint32_t g=(((a>>8&255)*inv+(b>>8&255)*alpha)>>8);
    uint32_t bl=(((a&255)*inv+(b&255)*alpha)>>8);
    return r<<16|g<<8|bl;
}
static uint32_t rgb(int r,int g,int b){return (uint32_t)(r<<16|g<<8|b);}
static void put(QDesktop *q,int x,int y,uint32_t c){if((unsigned)x<(unsigned)q->width&&(unsigned)y<(unsigned)q->height)q->pixels[(size_t)y*q->width+x]=c;}
static void block(QDesktop *q,int x,int y,int w,int h,uint32_t c){
    int x0=clamp(x,0,q->width),x1=clamp(x+w,0,q->width);
    int y0=clamp(y,0,q->height),y1=clamp(y+h,0,q->height);
    for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++)q->pixels[(size_t)yy*q->width+xx]=c;
}
static int rounded_hit(int x,int y,int w,int h,int r){
    if(x<0||y<0||x>=w||y>=h)return 0;
    int cx=x<r?r:x>=w-r?w-r-1:x;
    int cy=y<r?r:y>=h-r?h-r-1:y;
    int dx=x-cx,dy=y-cy;
    return dx*dx+dy*dy<=r*r;
}
static void shadow(QDesktop *q,int x,int y,int w,int h,int radius,int size){
    int x0=clamp(x-size,0,q->width),x1=clamp(x+w+size,0,q->width);
    int y0=clamp(y-size,0,q->height),y1=clamp(y+h+size,0,q->height);
    int r=radius;
    for(int yy=y0;yy<y1;yy++){
        int sy=yy<y+r?y+r:yy>=y+h-r?y+h-r-1:yy;
        for(int xx=x0;xx<x1;xx++){
            int sx=xx<x+r?x+r:xx>=x+w-r?x+w-r-1:xx;
            int dx=xx-sx,dy=yy-sy;
            int dist=dx*dx+dy*dy;
            if(dist<=r*r)continue;
            int beyond=dist-r*r;
            int span=size*(size+2*r);
            if(beyond>=span)continue;
            int opacity=100*(span-beyond)*(span-beyond)/(span*span);
            uint32_t *p=&q->pixels[(size_t)yy*q->width+xx];
            *p=mix(*p,0x071424,opacity);
        }
    }
}
static void glass(QDesktop *q,int x,int y,int w,int h,int r,uint32_t tint,int alpha){
    int x0=clamp(x,0,q->width),x1=clamp(x+w,0,q->width);
    int y0=clamp(y,0,q->height),y1=clamp(y+h,0,q->height);
    int dark=q->theme;
    for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++){
        int rx=xx-x,ry=yy-y;
        if(!rounded_hit(rx,ry,w,h,r))continue;
        size_t n=(size_t)yy*q->width+xx;
        uint32_t backdrop=mix(q->pixels[n],q->blurred[n],dark?106:145);
        int a=alpha;
        if(ry<17)a=clamp(a+((17-ry)*3),0,255);
        uint32_t output=mix(backdrop,tint,a);
        int boundary=rx<2||ry<2||rx>=w-2||ry>=h-2;
        if(boundary)output=mix(output,ry<h/2?0xecfbff:0x7896b3,dark?72:105);
        q->pixels[n]=output;
    }
}
static void panel(QDesktop *q,int x,int y,int w,int h,int r,uint32_t tint,int alpha){shadow(q,x,y+7,w,h,r,23);glass(q,x,y,w,h,r,tint,alpha);}
static void label(QDesktop *q,int x,int y,const char *s,uint32_t color,int scale){
    int start=x;
    for(;*s;s++){
        if(*s=='\n'){y+=10*scale;x=start;continue;}
        const uint8_t *g=q_glyph((unsigned char)*s);
        for(int col=0;col<5;col++)for(int row=0;row<7;row++)if(g[col]&(1u<<row))block(q,x+col*scale,y+row*scale,scale,scale,color);
        x+=6*scale;
        if(x>q->width-6*scale)break;
    }
}
static void line(QDesktop *q,int x0,int y0,int x1,int y1,uint32_t c){
    int dx=x1-x0;if(dx<0)dx=-dx;
    int dy=y1-y0;if(dy<0)dy=-dy;
    int sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx-dy;
    for(;;){put(q,x0,y0,c);if(x0==x1&&y0==y1)break;int e=2*err;if(e>-dy){err-=dy;x0+=sx;}if(e<dx){err+=dx;y0+=sy;}}
}
static void orb(QDesktop *q,int x,int y,int radius,uint32_t color){
    for(int yy=-radius;yy<=radius;yy++)for(int xx=-radius;xx<=radius;xx++)if(xx*xx+yy*yy<=radius*radius)put(q,x+xx,y+yy,color);
}
static void icon(QDesktop *q,int x,int y,int id,uint32_t c){
    if(id==Q_FILES){block(q,x-12,y-6,10,4,c);block(q,x-12,y-2,25,17,c);block(q,x-9,y+2,19,10,0x32617b);}
    else if(id==Q_NOTES){block(q,x-10,y-12,20,25,c);for(int i=0;i<4;i++)block(q,x-6,y-7+i*5,13,2,0x32617b);}
    else if(id==Q_TERMINAL){block(q,x-13,y-10,26,21,c);line(q,x-8,y-5,x-3,y,0x27435e);line(q,x-3,y,x-8,y+5,0x27435e);block(q,x,y+5,7,2,0x27435e);}
    else if(id==Q_CALCULATOR){block(q,x-10,y-13,20,27,c);block(q,x-7,y-9,14,6,0x32617b);for(int i=0;i<9;i++)block(q,x-7+i%3*5,y+i/3*5,3,3,0x32617b);}
    else if(id==Q_SETTINGS){orb(q,x,y,11,c);orb(q,x,y,5,0x32617b);}
    else {orb(q,x,y,12,c);label(q,x-4,y-7,"I",0x32617b,2);}
}
static void wallpaper(QDesktop *q){
    for(int y=0;y<q->height;y++)for(int x=0;x<q->width;x++){
        int nx=x*256/q->width,ny=y*256/q->height;
        int a=(nx*3+ny*2)/5;
        int glow=clamp(255-((nx-171)*(nx-171)+(ny-64)*(ny-64))/68,0,255);
        int halo=clamp(255-((nx-40)*(nx-40)+(ny-183)*(ny-183))/96,0,255);
        int wave=((x/6+y/9)%71)*2;
        int r=clamp(12+a/12+glow/9+halo/15+wave/17,0,255);
        int g=clamp(28+a/10+glow/4+halo/7+wave/15,0,255);
        int b=clamp(54+a/6+glow/3+halo/3+wave/11,0,255);
        uint32_t c=rgb(r,g,b);
        if(!q->theme)c=mix(c,0xdaedff,65);
        q->wallpaper[(size_t)y*q->width+x]=c;
    }
    for(int y=0;y<q->height;y++)for(int x=0;x<q->width;x++){
        int xx=clamp(x-9,0,q->width-1),right=clamp(x+9,0,q->width-1);
        int top=clamp(y-9,0,q->height-1),bottom=clamp(y+9,0,q->height-1);
        uint32_t v=q->wallpaper[(size_t)y*q->width+x];
        uint32_t a=q->wallpaper[(size_t)top*q->width+x];
        uint32_t b=q->wallpaper[(size_t)bottom*q->width+x];
        uint32_t c=q->wallpaper[(size_t)y*q->width+xx];
        uint32_t d=q->wallpaper[(size_t)y*q->width+right];
        q->blurred[(size_t)y*q->width+x]=rgb(((v>>16&255)*4+(a>>16&255)+(b>>16&255)+(c>>16&255)+(d>>16&255))/8,
            ((v>>8&255)*4+(a>>8&255)+(b>>8&255)+(c>>8&255)+(d>>8&255))/8,
            ((v&255)*4+(a&255)+(b&255)+(c&255)+(d&255))/8);
    }
}
static void close_popups(QDesktop *q){q->menu=0;q->notifications=0;q->quick=0;}
static const char *names[Q_APPS]={"FILES","NOTES","TERMINAL","CALCULATOR","SETTINGS","ABOUT"};
static int task_start(QDesktop *q){return (q->width-448)/2;}
static uint32_t accent(QDesktop *q){return (uint32_t[]){0x77c9ed,0x86d9bd,0xa9a4f4,0xf5ba89}[q->accent&3];}
void q_init(QDesktop *q,int width,int height,uint32_t *pixels,uint32_t *wallpaper_buf,uint32_t *blurred){
    unsigned char *p=(unsigned char *)q;
    for(size_t i=0;i<sizeof(*q);i++)p[i]=0;
    q->width=width;q->height=height;q->pixels=pixels;q->wallpaper=wallpaper_buf;q->blurred=blurred;
    q->mouse_x=width/2;q->mouse_y=height/2;q->active=-1;q->drag=-1;q->dirty=1;
    for(int i=0;i<Q_APPS;i++){
        QWindow *w=&q->windows[i];
        w->w=clamp(width-160,530,850);w->h=clamp(height-175,380,590);
        w->x=(width-w->w)/2+(i%3)*18-18;w->y=(height-w->h)/2+(i%3)*15-15;
    }
    const char *note="WELCOME TO QUARTZIE X64\n\nTHIS IS A NEW UEFI DESKTOP.\nNOTES ARE STORED IN MEMORY.";
    while(note[q->note_length]&&q->note_length<2047){q->notes[q->note_length]=note[q->note_length];q->note_length++;}
    const char *status="READY - UEFI X64";
    for(int i=0;status[i];i++)q->status[i]=status[i];
    const char *ans="0";q->answer[0]=ans[0];
    wallpaper(q);
}
void q_open(QDesktop *q,int app){
    if(app<0||app>=Q_APPS)return;
    int top=0;for(int i=0;i<Q_APPS;i++)if(q->windows[i].z>top)top=q->windows[i].z;
    if(top>10000){for(int i=0;i<Q_APPS;i++)q->windows[i].z=i+1;top=Q_APPS;}
    q->windows[app].open=1;q->windows[app].z=top+1;q->active=app;close_popups(q);q->dirty=1;
}
int q_dock_hit(QDesktop *q,int x,int y){
    int left=task_start(q);
    if(y<q->height-70||y>=q->height-16)return -1;
    if(x>=left+12&&x<left+60)return 10;
    for(int i=0;i<Q_APPS;i++)if(x>=left+78+i*55&&x<left+126+i*55)return i;
    if(x>=q->width-172&&x<q->width-116)return 11;
    if(x>=q->width-110&&x<q->width-22)return 12;
    return -1;
}
static void draw_taskbar(QDesktop *q){
    int left=task_start(q),y=q->height-70;
    panel(q,left,y,448,56,19,q->theme?0x14243b:0xf0f8ff,q->theme?155:145);
    glass(q,left+11,y+8,51,40,12,q->theme?0x204569:0xb2dfff,130);
    label(q,left+27,y+19,"Q",0xf6fcff,2);
    for(int i=0;i<Q_APPS;i++){
        int xx=left+78+i*55;
        if(q->active==i&&q->windows[i].open)glass(q,xx,y+8,48,40,10,q->theme?0x356a83:0xb2dfff,100);
        icon(q,xx+23,y+27,i,q->theme?0xc9e7ff:0x356588);
        if(q->windows[i].open)block(q,xx+15,y+48,16,2,accent(q));
    }
    if(q->width>760){
        int sx=q->width-176;
        panel(q,sx,y,164,56,18,q->theme?0x14243b:0xf0f8ff,q->theme?150:143);
        orb(q,sx+28,y+25,7,0xb6dff2);block(q,sx+24,y+31,9,3,0xb6dff2);
        label(q,sx+57,y+15,"UEFI",q->theme?0xd8ebff:0x2d4f74,2);
        label(q,sx+57,y+34,"64-BIT",q->theme?0xa0c5e1:0x4b7090,1);
    }
}
static void draw_desktop(QDesktop *q){
    int cx=q->width/2;
    label(q,54,61,"QUARTZIE",0xe7f7ff,3);
    label(q,56,95,"FLUID DESKTOP  /  X64",0xb8dcf4,2);
    glass(q,51,135,290,1,1,0xffffff,130);
    panel(q,52,160,332,160,23,q->theme?0x172c48:0xedf8ff,q->theme?150:138);
    label(q,73,180,"A NEW FOUNDATION",q->theme?0xecf7ff:0x23486a,2);
    label(q,74,216,"UEFI NATIVE BOOT",q->theme?0xc1d9ef:0x42688a,2);
    label(q,74,248,"64-BIT PROCESSOR",q->theme?0xc1d9ef:0x42688a,2);
    label(q,74,280,"SOFTWARE GLASS",q->theme?0xc1d9ef:0x42688a,2);
    if(q->width>1050){
        orb(q,q->width-210,185,65,0x275c7a);orb(q,q->width-210,185,58,0x4a9eae);orb(q,q->width-210,185,42,0x76c8d2);
        label(q,q->width-243,173,"Q",0xe8ffff,5);
    }
    label(q,cx-132,q->height-111,"QUARTZIE X64",0xcde9ff,2);
}
static void draw_menu(QDesktop *q){
    if(!q->menu)return;
    int x=(q->width-490)/2,y=q->height-542;
    panel(q,x,y,490,455,22,q->theme?0x15253e:0xf5fbff,q->theme?158:151);
    label(q,x+25,y+23,"QUARTZIE",q->theme?0xffffff:0x264d6e,3);
    label(q,x+26,y+63,"APPLICATIONS",q->theme?0xaacbde:0x5d829f,2);
    for(int i=0;i<Q_APPS;i++){
        int xx=x+25+(i%3)*153,yy=y+110+(i/3)*113;
        glass(q,xx,yy,136,99,14,q->theme?0x29445e:0xe5f4ff,125);
        icon(q,xx+68,yy+35,i,q->theme?0xeaf8ff:0x316181);
        label(q,xx+10,yy+71,names[i],q->theme?0xe4f3ff:0x385b7d,1);
    }
    glass(q,x+25,y+350,440,66,14,q->theme?0x29445e:0xe5f4ff,118);
    label(q,x+40,y+371,"X64 UEFI  /  NEW KERNEL",q->theme?0xd6eeff:0x326283,2);
}
static void draw_notifications(QDesktop *q){
    if(!q->notifications)return;
    int x=q->width-365,y=q->height-412;
    panel(q,x,y,350,325,22,q->theme?0x15253e:0xf4faff,q->theme?158:148);
    label(q,x+22,y+24,"NOTIFICATIONS",q->theme?0xf4fbff:0x315372,2);
    glass(q,x+17,y+85,316,92,15,q->theme?0x315270:0xdaefff,136);
    label(q,x+28,y+102,"WELCOME BACK",q->theme?0xf1fbff:0x274764,2);
    label(q,x+28,y+132,"QUARTZIE X64 IS READY",q->theme?0xb9d9ef:0x50738c,1);
    label(q,x+26,y+253,"NO OTHER NOTIFICATIONS",q->theme?0xc8e0f1:0x587994,2);
}
static void draw_quick(QDesktop *q){
    if(!q->quick)return;
    int x=q->width-365,y=q->height-435;
    panel(q,x,y,350,348,22,q->theme?0x15253e:0xf4faff,q->theme?158:148);
    label(q,x+22,y+22,"QUICK SETTINGS",q->theme?0xf4fbff:0x315372,2);
    glass(q,x+19,y+80,312,75,15,q->theme?0x315270:0xdaefff,126);
    label(q,x+37,y+109,q->theme?"DARK GLASS":"LIGHT GLASS",q->theme?0xffffff:0x315372,2);
    glass(q,x+19,y+175,312,75,15,q->theme?0x315270:0xdaefff,126);
    label(q,x+37,y+204,"CHANGE ACCENT",q->theme?0xffffff:0x315372,2);
    glass(q,x+19,y+271,312,49,12,q->theme?0x315270:0xdaefff,126);
    label(q,x+37,y+289,"OPEN SETTINGS",q->theme?0xffffff:0x315372,2);
}
static void draw_window_content(QDesktop *q,int id,QWindow *w){
    int x=w->x+25,y=w->y+67,ww=w->w-50;
    uint32_t ink=q->theme?0xe0f3ff:0x2d5372;
    uint32_t soft=q->theme?0xa8ccdf:0x4f7693;
    if(id==Q_FILES){
        label(q,x,y,"EXPLORER",ink,2);
        label(q,x,y+40,"THIS IS A NEW X64 RUNTIME",soft,2);
        glass(q,x,y+91,ww,76,14,q->theme?0x26415c:0xd7eefe,128);
        label(q,x+18,y+109,"SYSTEM DRIVE",ink,2);
        label(q,x+18,y+139,"FILESYSTEM NOT MOUNTED",soft,1);
        glass(q,x,y+185,ww,67,14,q->theme?0x26415c:0xd7eefe,128);
        label(q,x+18,y+211,"OPEN NOTES FOR SCRATCHPAD",ink,1);
    }else if(id==Q_NOTES){
        label(q,x,y,"SCRATCHPAD",ink,2);
        label(q,x,y+35,"IN MEMORY  /  NOT YET PERSISTENT",soft,1);
        glass(q,x,y+67,ww,w->h-175,13,q->theme?0x1e344b:0xf4fbff,128);
        label(q,x+20,y+88,q->notes,ink,2);
        block(q,x+17+(q->note_length%35)*12,y+88+(q->note_length/35)*20,2,16,0x63d2ea);
    }else if(id==Q_TERMINAL){
        label(q,x,y,"QUARTZIE CONSOLE",ink,2);
        glass(q,x,y+49,ww,w->h-161,13,q->theme?0x0a192c:0x173552,195);
        label(q,x+15,y+69,"QUARTZIE X64  /  UEFI SHELL",0x8bd9e9,2);
        label(q,x+15,y+106,q->status,0xd7f5ff,1);
        label(q,x+15,y+148,">",0x86e0ef,2);
        label(q,x+36,y+148,q->command,0xffffff,2);
    }else if(id==Q_CALCULATOR){
        label(q,x,y,"CALCULATOR",ink,2);
        glass(q,x,y+44,ww,66,13,q->theme?0x224461:0xe5f6ff,119);
        label(q,x+18,y+67,q->calc_length?q->calc:q->answer,ink,2);
        const char *keys="789/456*123-C0=+";
        for(int i=0;i<16;i++){
            int xx=x+(i%4)*75,yy=y+139+(i/4)*53;
            glass(q,xx,yy,66,46,10,q->theme?0x28506d:0xd9efff,120);
            char txt[2]={keys[i],0};label(q,xx+24,yy+14,txt,ink,2);
        }
    }else if(id==Q_SETTINGS){
        label(q,x,y,"APPEARANCE",ink,2);
        glass(q,x,y+58,ww,76,14,q->theme?0x26415c:0xd7eefe,120);
        label(q,x+22,y+86,"THEME",ink,2);
        label(q,x+170,y+86,q->theme?"DARK":"LIGHT",soft,2);
        glass(q,x,y+153,ww,76,14,q->theme?0x26415c:0xd7eefe,120);
        label(q,x+22,y+183,"ACCENT",ink,2);
        for(int i=0;i<4;i++)orb(q,x+206+i*53,y+191,13,(uint32_t[]){0x77c9ed,0x86d9bd,0xa9a4f4,0xf5ba89}[i]);
        label(q,x,y+264,"CLICK A ROW TO CHANGE THEME",soft,1);
    }else{
        label(q,x,y,"QUARTZIE X64",ink,3);
        label(q,x,y+71,"BUILT FOR 64-BIT UEFI",soft,2);
        label(q,x,y+114,"A NEW DESKTOP IMPLEMENTATION",soft,2);
        label(q,x,y+157,"NATIVE FRAMEBUFFER RENDERING",soft,2);
        label(q,x,y+200,"EXPERIMENTAL PREVIEW",soft,2);
    }
}
static void draw_window(QDesktop *q,int id){
    QWindow *w=&q->windows[id];if(!w->open)return;
    int is_active=q->active==id;
    panel(q,w->x,w->y,w->w,w->h,19,q->theme?0x16283e:0xf5fbff,q->theme?166:165);
    glass(q,w->x+3,w->y+3,w->w-6,43,15,q->theme?0x366080:0xaad8f0,is_active?150:120);
    orb(q,w->x+24,w->y+25,8,accent(q));
    label(q,w->x+47,w->y+15,names[id],is_active?0xfcffff:0xc6d9e7,2);
    glass(q,w->x+w->w-93,w->y+9,34,27,9,q->theme?0x264b68:0xd9eefa,115);
    glass(q,w->x+w->w-49,w->y+9,34,27,9,0xb65269,155);
    block(q,w->x+w->w-83,w->y+23,13,2,0xecfbff);
    line(q,w->x+w->w-39,w->y+17,w->x+w->w-27,w->y+29,0xffffff);
    line(q,w->x+w->w-27,w->y+17,w->x+w->w-39,w->y+29,0xffffff);
    draw_window_content(q,id,w);
}
void q_draw(QDesktop *q){
    if(!q->dirty)return;
    size_t total=(size_t)q->width*q->height;
    for(size_t i=0;i<total;i++)q->pixels[i]=q->wallpaper[i];
    draw_desktop(q);
    int drawn[Q_APPS]={0};
    for(int k=0;k<Q_APPS;k++){
        int pick=-1;
        for(int i=0;i<Q_APPS;i++)if(q->windows[i].open&&!drawn[i]&&(pick<0||q->windows[i].z<q->windows[pick].z))pick=i;
        if(pick<0)break;
        drawn[pick]=1;draw_window(q,pick);
    }
    draw_taskbar(q);
    draw_menu(q);
    draw_notifications(q);
    draw_quick(q);
    q->frame_count++;q->dirty=0;
}
static void calc_input(QDesktop *q,char ch){
    if(ch=='C'){q->calc_length=0;q->calc[0]=0;q->answer[0]='0';q->answer[1]=0;return;}
    if(ch=='='){
        int first=0,second=0,at=0,sign=1;char op=0;
        while(q->calc[at]>='0'&&q->calc[at]<='9'){first=first*10+q->calc[at++]-'0';if(first>1000000)first=1000000;}
        op=q->calc[at++];if(q->calc[at]=='-'){sign=-1;at++;}
        while(q->calc[at]>='0'&&q->calc[at]<='9'){second=second*10+q->calc[at++]-'0';if(second>1000000)second=1000000;}
        second*=sign;
        int result=op=='+'?first+second:op=='-'?first-second:op=='*'?first*second:op=='/'&&second?first/second:0;
        if(op=='/'&&!second){q->answer[0]='E';q->answer[1]='R';q->answer[2]='R';q->answer[3]=0;}
        else{
            char rev[16];int n=0,neg=result<0;unsigned int z=neg?(unsigned int)(-(int64_t)result):(unsigned int)result;
            do{rev[n++]=(char)('0'+z%10);z/=10;}while(z&&n<15);
            int pos=0;if(neg)q->answer[pos++]='-';while(n)q->answer[pos++]=rev[--n];q->answer[pos]=0;
        }
        q->calc_length=0;q->calc[0]=0;
        return;
    }
    if(q->calc_length<30){q->calc[q->calc_length++]=ch;q->calc[q->calc_length]=0;}
}
void q_click(QDesktop *q,int x,int y){
    int dock=q_dock_hit(q,x,y);
    if(dock==10){q->menu=!q->menu;q->quick=0;q->notifications=0;q->dirty=1;return;}
    if(dock>=0&&dock<Q_APPS){q_open(q,dock);return;}
    if(dock==11){q->notifications=!q->notifications;q->menu=0;q->quick=0;q->dirty=1;return;}
    if(dock==12){q->quick=!q->quick;q->menu=0;q->notifications=0;q->dirty=1;return;}
    if(q->menu){
        int mx=(q->width-490)/2,my=q->height-542;
        if(within(x,y,mx,my,490,455)){
            for(int i=0;i<Q_APPS;i++)if(within(x,y,mx+25+(i%3)*153,my+110+(i/3)*113,136,99)){q_open(q,i);return;}
            return;
        }
        q->menu=0;q->dirty=1;
    }
    if(q->notifications){
        if(within(x,y,q->width-365,q->height-412,350,325))return;
        q->notifications=0;q->dirty=1;
    }
    if(q->quick){
        int bx=q->width-365,by=q->height-435;
        if(within(x,y,bx+19,by+80,312,75)){q->theme=!q->theme;wallpaper(q);q->dirty=1;return;}
        if(within(x,y,bx+19,by+175,312,75)){q->accent=(q->accent+1)%4;q->dirty=1;return;}
        if(within(x,y,bx+19,by+271,312,49)){q_open(q,Q_SETTINGS);return;}
        if(within(x,y,bx,by,350,348))return;
        q->quick=0;q->dirty=1;
    }
    int hit=-1;
    for(int i=0;i<Q_APPS;i++)if(q->windows[i].open&&within(x,y,q->windows[i].x,q->windows[i].y,q->windows[i].w,q->windows[i].h)&&
        (hit<0||q->windows[i].z>q->windows[hit].z))hit=i;
    if(hit>=0){
        QWindow *w=&q->windows[hit];
        if(within(x,y,w->x+w->w-49,w->y+9,34,27)){w->open=0;q->active=-1;for(int i=0;i<Q_APPS;i++)if(q->windows[i].open&&(q->active<0||q->windows[i].z>q->windows[q->active].z))q->active=i;q->dirty=1;return;}
        if(within(x,y,w->x+w->w-93,w->y+9,34,27)){w->open=0;q->active=-1;q->dirty=1;return;}
        q_open(q,hit);
        if(within(x,y,w->x,w->y,w->w-105,46)){q->drag=hit;q->drag_x=x-w->x;q->drag_y=y-w->y;return;}
        if(hit==Q_SETTINGS){
            int bx=w->x+25,by=w->y+67;
            if(within(x,y,bx,by+58,w->w-50,76)){q->theme=!q->theme;wallpaper(q);q->dirty=1;}
            else if(within(x,y,bx,by+153,w->w-50,76)){q->accent=(q->accent+1)%4;q->dirty=1;}
        }else if(hit==Q_CALCULATOR){
            int bx=w->x+25,by=w->y+67;
            const char *keys="789/456*123-C0=+";
            for(int i=0;i<16;i++)if(within(x,y,bx+(i%4)*75,by+139+(i/4)*53,66,46)){calc_input(q,keys[i]);q->dirty=1;break;}
        }
        return;
    }
    q->active=-1;q->dirty=1;
}
void q_press(QDesktop *q,int x,int y){q->pressed=1;q_click(q,x,y);}
void q_release(QDesktop *q){q->pressed=0;q->drag=-1;}
void q_move(QDesktop *q,int x,int y){
    q->mouse_x=clamp(x,0,q->width-1);q->mouse_y=clamp(y,0,q->height-1);
    if(q->pressed&&q->drag>=0){
        QWindow *w=&q->windows[q->drag];
        w->x=clamp(q->mouse_x-q->drag_x,0,q->width-w->w);
        w->y=clamp(q->mouse_y-q->drag_y,0,q->height-w->h-72);
        q->dirty=1;
    }
}
void q_key(QDesktop *q,uint16_t ch,uint16_t scan){
    if(scan>=11&&scan<=16){q_open(q,scan-11);return;}
    if(scan==17){q->menu=!q->menu;q->quick=0;q->notifications=0;q->dirty=1;return;}
    if(scan==18){q->notifications=!q->notifications;q->menu=0;q->quick=0;q->dirty=1;return;}
    if(scan==19){q->quick=!q->quick;q->menu=0;q->notifications=0;q->dirty=1;return;}
    if(scan==23||ch==27){close_popups(q);q->dirty=1;return;}
    if(ch=='\r'){
        if(q->active==Q_TERMINAL){
            if(q->command_length){
                const char *cmd=q->command;
                const char *message="UNKNOWN COMMAND";
                if(cmd[0]=='h'||cmd[0]=='H')message="HELP  INFO  CLEAR  APPS";
                if(cmd[0]=='i'||cmd[0]=='I')message="QUARTZIE X64 UEFI DESKTOP";
                if(cmd[0]=='c'||cmd[0]=='C')message="CONSOLE CLEARED";
                for(int i=0;i<95;i++){q->status[i]=message[i];if(!message[i])break;}
                if(cmd[0]=='a'||cmd[0]=='A')q_open(q,Q_FILES);
            }
            q->command_length=0;q->command[0]=0;
        }else if(q->active==Q_NOTES&&q->note_length<2047){q->notes[q->note_length++]='\n';q->notes[q->note_length]=0;}
        q->dirty=1;return;
    }
    if(ch==8){
        if(q->active==Q_NOTES&&q->note_length>0)q->notes[--q->note_length]=0;
        else if(q->active==Q_TERMINAL&&q->command_length>0)q->command[--q->command_length]=0;
        else if(q->active==Q_CALCULATOR&&q->calc_length>0)q->calc[--q->calc_length]=0;
        q->dirty=1;return;
    }
    if(ch<32||ch>126)return;
    if(q->menu){for(int i=0;i<Q_APPS;i++)if(ch==('1'+i)){q_open(q,i);return;}}
    if(q->active==Q_NOTES&&q->note_length<2047){q->notes[q->note_length++]=(char)ch;q->notes[q->note_length]=0;q->dirty=1;}
    else if(q->active==Q_TERMINAL&&q->command_length<120){q->command[q->command_length++]=(char)ch;q->command[q->command_length]=0;q->dirty=1;}
    else if(q->active==Q_CALCULATOR){calc_input(q,(char)ch);q->dirty=1;}
}
