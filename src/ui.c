#include "ui.h"
#include "font.h"
#include <stddef.h>
#include <stdint.h>

static int clamp(int x,int lo,int hi){return x<lo?lo:x>hi?hi:x;}
static int absval(int x){return x<0?-x:x;}
static int hit(int px,int py,int x,int y,int w,int h){return px>=x&&py>=y&&px<x+w&&py<y+h;}
static uint32_t rgb(int r,int g,int b){return (uint32_t)((r<<16)|(g<<8)|b);}
static uint32_t blend(uint32_t a,uint32_t b,int alpha){
    int inv=256-alpha;
    return rgb(((a>>16&255)*inv+(b>>16&255)*alpha)>>8,
               ((a>>8&255)*inv+(b>>8&255)*alpha)>>8,
               ((a&255)*inv+(b&255)*alpha)>>8);
}
static void pixel(QDesktop *q,int x,int y,uint32_t color){
    if((unsigned)x<(unsigned)q->width&&(unsigned)y<(unsigned)q->height)q->pixels[(size_t)y*q->width+x]=color;
}
static void rectangle(QDesktop *q,int x,int y,int w,int h,uint32_t color){
    int x0=clamp(x,0,q->width),x1=clamp(x+w,0,q->width);
    int y0=clamp(y,0,q->height),y1=clamp(y+h,0,q->height);
    for(int yy=y0;yy<y1;yy++){
        uint32_t *p=q->pixels+(size_t)yy*q->width+x0;
        for(int xx=x0;xx<x1;xx++)*p++=color;
    }
}
static int round_inside(int x,int y,int w,int h,int radius){
    if(x<0||y<0||x>=w||y>=h)return 0;
    int cx=x<radius?radius:x>=w-radius?w-radius-1:x;
    int cy=y<radius?radius:y>=h-radius?h-radius-1:y;
    int dx=x-cx,dy=y-cy;
    return dx*dx+dy*dy<=radius*radius;
}
static void circular(QDesktop *q,int cx,int cy,int r,uint32_t c,int a){
    if(r<=0)return;
    int x0=clamp(cx-r-1,0,q->width),x1=clamp(cx+r+2,0,q->width);
    int y0=clamp(cy-r-1,0,q->height),y1=clamp(cy+r+2,0,q->height);
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){
        int dx=x-cx,dy=y-cy,d=dx*dx+dy*dy;
        if(d>(r+1)*(r+1))continue;
        int alpha=d<=(r-1)*(r-1)?a:a*((r+1)*(r+1)-d)/(4*r);
        if(alpha<1)continue;
        uint32_t *p=q->pixels+(size_t)y*q->width+x;
        *p=blend(*p,c,clamp(alpha,0,256));
    }
}
static void segment(QDesktop *q,int ax,int ay,int bx,int by,uint32_t c){
    int dx=absval(bx-ax),dy=absval(by-ay),sx=ax<bx?1:-1,sy=ay<by?1:-1,err=dx-dy;
    for(;;){pixel(q,ax,ay,c);if(ax==bx&&ay==by)break;int e=2*err;if(e>-dy){err-=dy;ax+=sx;}if(e<dx){err+=dx;ay+=sy;}}
}
static void soft_shadow(QDesktop *q,int x,int y,int w,int h,int r,int spread,int intensity){
    int x0=clamp(x-spread,0,q->width),x1=clamp(x+w+spread,0,q->width);
    int y0=clamp(y-spread,0,q->height),y1=clamp(y+h+spread,0,q->height);
    for(int yy=y0;yy<y1;yy++){
        int cy=clamp(yy,y+r,y+h-r-1);
        for(int xx=x0;xx<x1;xx++){
            int cx=clamp(xx,x+r,x+w-r-1);
            int dx=absval(xx-cx),dy=absval(yy-cy);
            int near=dx<dy?dx:dy,far=dx>dy?dx:dy;
            int distance=far+near*3/8-r;
            if(distance<0||distance>=spread)continue;
            int strength=intensity*(spread-distance)*(spread-distance)/(spread*spread);
            uint32_t *dst=&q->pixels[(size_t)yy*q->width+xx];
            *dst=blend(*dst,0x030a19,strength);
        }
    }
}
static void surface(QDesktop *q,int x,int y,int w,int h,int radius,uint32_t tint,int alpha){
    int x0=clamp(x,0,q->width),x1=clamp(x+w,0,q->width);
    int y0=clamp(y,0,q->height),y1=clamp(y+h,0,q->height);
    if(x0>=x1||y0>=y1)return;
    int bw=q->width;
    for(int py=y0;py<y1;py++){
        int up=clamp(py-12,0,q->height-1),down=clamp(py+12,0,q->height-1);
        for(int px=x0;px<x1;px++){
            int left=clamp(px-12,0,q->width-1),right=clamp(px+12,0,q->width-1);
            uint32_t a=q->pixels[(size_t)up*bw+px];
            uint32_t b=q->pixels[(size_t)py*bw+left];
            uint32_t c=q->pixels[(size_t)py*bw+px];
            uint32_t d=q->pixels[(size_t)py*bw+right];
            uint32_t e=q->pixels[(size_t)down*bw+px];
            q->blurred[(size_t)py*bw+px]=rgb((((a>>16&255)+(b>>16&255)+(c>>16&255)*4+(d>>16&255)+(e>>16&255))>>3),
                                              (((a>>8&255)+(b>>8&255)+(c>>8&255)*4+(d>>8&255)+(e>>8&255))>>3),
                                              (((a&255)+(b&255)+(c&255)*4+(d&255)+(e&255))>>3));
        }
    }
    for(int py=y0;py<y1;py++)for(int px=x0;px<x1;px++){
        int dx=px-x,dy=py-y;
        if(!round_inside(dx,dy,w,h,radius))continue;
        int sheen=dy<h/2?24*(h/2-dy)/(h/2+1):0;
        int grain=((px*13+py*23)^(px*3+py*7))&7;
        int material=clamp(alpha+sheen+grain-4,0,235);
        size_t at=(size_t)py*bw+px;
        uint32_t frosted=blend(q->pixels[at],q->blurred[at],184);
        uint32_t c=blend(frosted,tint,material);
        if(dx==1||dy==1)c=blend(c,0xf7fbff,q->theme?38:73);
        if(dx==w-2||dy==h-2)c=blend(c,q->theme?0x8baad8:0x486284,q->theme?45:38);
        q->pixels[at]=c;
    }
}
static void pane(QDesktop *q,int x,int y,int w,int h,int r,uint32_t tint,int alpha){
    soft_shadow(q,x,y+9,w,h,r,36,83);
    surface(q,x,y,w,h,r,tint,alpha);
}
static void string_limit(QDesktop *q,int x,int baseline,const char *s,uint32_t color,int style,int max_width,int max_lines){
    int start=x,line=0;
    int leading=style==0?20:style==1?26:style==2?35:51;
    const uint8_t *bitmap=q_font_data();
    for(;*s;s++){
        if(*s=='\n'){x=start;baseline+=leading;if(++line>=max_lines)break;continue;}
        const QGlyph *g=q_font_glyph(style,(unsigned char)*s);
        if(x+g->advance>start+max_width)break;
        for(int yy=0;yy<g->height;yy++){
            int py=baseline+g->top+yy;
            if((unsigned)py>=(unsigned)q->height)continue;
            for(int xx=0;xx<g->width;xx++){
                int px=x+g->left+xx;
                if((unsigned)px>=(unsigned)q->width)continue;
                size_t index=(size_t)g->offset+(size_t)yy*g->width+xx;
                uint8_t byte=bitmap[index>>1];
                int a=(index&1)?byte&15:byte>>4;
                if(a<1)continue;
                uint32_t *dst=q->pixels+(size_t)py*q->width+px;
                *dst=blend(*dst,color,a*17);
            }
        }
        x+=g->advance;
    }
}
static void text(QDesktop *q,int x,int baseline,const char *s,uint32_t c,int style){
    string_limit(q,x,baseline,s,c,style,q->width-x,40);
}
static int text_width(const char *s,int style){
    int result=0;for(;*s&&*s!='\n';s++)result+=q_font_glyph(style,(unsigned char)*s)->advance;return result;
}
static uint32_t foreground(QDesktop *q){return q->theme?0xf0f8ff:0x122c49;}
static uint32_t secondary(QDesktop *q){return q->theme?0xa6c2de:0x365676;}
static uint32_t material(QDesktop *q){return q->theme?0x17283e:0xdbebf9;}
static uint32_t inner_material(QDesktop *q){return q->theme?0x2d415c:0xe7f4ff;}
static uint32_t accent(QDesktop *q){
    static const uint32_t shades[]={0x5dcfe3,0x9e9efa,0x77d5af,0xf4ae85};
    return shades[q->accent%4];
}
static void wallpaper(QDesktop *q){
    int w=q->width,h=q->height;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){
        int u=x*1024/w,v=y*1024/h;
        int r=11+v*7/1024+u*10/1024;
        int g=19+u*11/1024+v*14/1024;
        int b=42+u*30/1024+v*19/1024;
        int dx=u-780,dy=v-250,dist=dx*dx+dy*dy;
        int f=clamp(200-(dist/1500),0,200);
        r+=f*18/200;g+=f*97/200;b+=f*136/200;
        dx=u-170;dy=v-850;dist=dx*dx+dy*dy;
        f=clamp(210-dist/1300,0,210);
        r+=f*81/210;g+=f*25/210;b+=f*113/210;
        dx=u-420;dy=v-140;dist=dx*dx+dy*dy;
        f=clamp(190-dist/1800,0,190);
        r+=f*39/190;g+=f*36/190;b+=f*84/190;
        int wave=absval((v*5-u*2)/4-295);
        f=clamp(130-wave/3,0,130);
        r+=f*13/130;g+=f*30/130;b+=f*37/130;
        if(q->theme){r=r*66/100;g=g*71/100;b=b*78/100;}
        q->wallpaper[(size_t)y*w+x]=rgb(clamp(r,0,255),clamp(g,0,255),clamp(b,0,255));
    }
}
static void thin_rule(QDesktop *q,int x,int y,int w,int alpha){
    for(int xx=clamp(x,0,q->width);xx<clamp(x+w,0,q->width);xx++){
        uint32_t *p=q->pixels+(size_t)y*q->width+xx;
        *p=blend(*p,0xe1f3ff,alpha);
    }
}
static void icon(QDesktop *q,int cx,int cy,int id,uint32_t color){
    int x=cx,y=cy;
    if(id==Q_FILES){
        circular(q,x,y,18,0x72bfec,170);
        rectangle(q,x-13,y-6,26,17,color);
        rectangle(q,x-13,y-10,11,6,color);
        rectangle(q,x-9,y-2,18,2,0x83c8ee);
    }else if(id==Q_NOTES){
        circular(q,x,y,18,0x9da7ff,145);
        rectangle(q,x-10,y-13,20,26,color);
        rectangle(q,x-7,y-8,14,2,0x7b98ba);
        rectangle(q,x-7,y-3,11,2,0x7b98ba);
        rectangle(q,x-7,y+2,14,2,0x7b98ba);
    }else if(id==Q_TERMINAL){
        circular(q,x,y,18,0x577caa,180);
        rectangle(q,x-12,y-9,24,18,color);
        segment(q,x-7,y-4,x-3,y,0x244b72);
        segment(q,x-3,y,x-7,y+4,0x244b72);
        rectangle(q,x-1,y+4,8,2,0x244b72);
    }else if(id==Q_CALCULATOR){
        circular(q,x,y,18,0x7fd1c6,135);
        rectangle(q,x-9,y-13,18,26,color);
        rectangle(q,x-6,y-9,12,6,0x456f91);
        for(int i=0;i<9;i++)rectangle(q,x-6+(i%3)*5,y+1+(i/3)*5,3,3,0x456f91);
    }else if(id==Q_SETTINGS){
        circular(q,x,y,17,0x9f9ce3,145);
        circular(q,x,y,12,color,255);
        circular(q,x,y,6,0x426789,255);
        circular(q,x,y,3,0xcedde9,255);
    }else{
        circular(q,x,y,17,0x85a9f6,135);
        circular(q,x,y,11,color,255);
        text(q,x-text_width("i",0)/2,y+5,"i",0x365a7b,0);
    }
}
static int dock_x(QDesktop *q){return (q->width-555)/2;}
static int dock_y(QDesktop *q){return q->height-89;}
static int menu_x(QDesktop *q){return (q->width-510)/2;}
static int menu_y(QDesktop *q){return clamp(q->height-607,54,q->height-100);}
static int desktop_shortcut(QDesktop *q,int x,int y){
    (void)q;
    for(int i=0;i<Q_APPS;i++){
        int xx=37+(i%2)*94,yy=113+(i/2)*104;
        if(hit(x,y,xx,yy,85,91))return i;
    }
    return -1;
}
static const char *names[Q_APPS]={"Files","Notes","Terminal","Calculator","Settings","About"};
void q_init(QDesktop *q,int w,int h,uint32_t *pixels,uint32_t *wallpaper_pixels,uint32_t *workspace){
    uint8_t *data=(uint8_t *)q;
    for(size_t n=0;n<sizeof(*q);n++)data[n]=0;
    q->width=w;q->height=h;q->pixels=pixels;q->wallpaper=wallpaper_pixels;q->blurred=workspace;
    q->active=-1;q->drag=-1;q->hover=-1;q->mouse_x=w/2;q->mouse_y=h/2;q->dirty=1;q->answer[0]='0';
    for(int i=0;i<Q_APPS;i++){
        QWindow *a=&q->windows[i];
        a->w=clamp(w-210,530,880);a->h=clamp(h-170,390,600);
        a->x=(w-a->w)/2+(i%3)*18-18;a->y=(h-a->h)/2+(i%3)*16-16;
    }
    wallpaper(q);
}
static void close_popups(QDesktop *q){q->menu=0;q->notifications=0;q->quick=0;}
void q_open(QDesktop *q,int app){
    if(app<0||app>=Q_APPS)return;
    int z=0;for(int i=0;i<Q_APPS;i++)if(q->windows[i].z>z)z=q->windows[i].z;
    if(z>10000){for(int i=0;i<Q_APPS;i++)q->windows[i].z=i+1;z=Q_APPS;}
    q->windows[app].open=1;q->windows[app].z=z+1;q->active=app;
    close_popups(q);q->dirty=1;
}
int q_dock_hit(QDesktop *q,int x,int y){
    int left=dock_x(q),top=dock_y(q);
    if(hit(x,y,left+11,top+8,52,53))return 10;
    for(int i=0;i<Q_APPS;i++)if(hit(x,y,left+77+i*77,top+8,61,53))return i;
    if(hit(x,y,q->width-119,11,44,38))return 11;
    if(hit(x,y,q->width-69,11,43,38))return 12;
    return -1;
}
static void draw_topbar(QDesktop *q){
    surface(q,0,0,q->width,61,0,0x132741,138);
    thin_rule(q,0,60,q->width,41);
    circular(q,38,30,15,accent(q),235);
    text(q,33,36,"Q",0x092e49,1);
    text(q,66,38,"Quartzie",0xf4faff,1);
    text(q,205,37,"Files",0xa9cee6,0);
    text(q,271,37,"Notes",0xa9cee6,0);
    text(q,343,37,"Settings",0xa9cee6,0);
    surface(q,q->width-118,11,43,38,13,0xc1dcef,58);
    circular(q,q->width-97,28,7,0xd3faff,220);
    rectangle(q,q->width-103,34,12,2,0xd3faff);
    surface(q,q->width-69,11,43,38,13,0xc1dcef,58);
    circular(q,q->width-47,29,10,0xe0f3ff,140);
    circular(q,q->width-47,29,5,0x3d617c,255);
}
static void draw_desktop(QDesktop *q){
    pane(q,21,90,219,354,24,0xbed7ef,112);
    text(q,42,126,"Favorites",foreground(q),1);
    for(int i=0;i<Q_APPS;i++){
        int x=37+(i%2)*94,y=113+(i/2)*104;
        if(q->hover==i+20)surface(q,x,y,86,91,17,inner_material(q),q->theme?63:116);
        circular(q,x+42,y+38,27,0xc7e5fa,q->theme?40:76);
        icon(q,x+42,y+38,i,q->theme?0xe7f6ff:0xf7fbff);
        int width=text_width(names[i],0);
        text(q,x+(86-width)/2,y+82,names[i],foreground(q),0);
    }
    if(q->width>=1100&&q->height>=660){
        int x=q->width-395,y=108;
        pane(q,x,y,363,236,25,0xcadfee,117);
        text(q,x+26,y+39,"Quick access",foreground(q),1);
        thin_rule(q,x+25,y+59,310,55);
        surface(q,x+25,y+81,313,58,17,inner_material(q),q->theme?86:96);
        icon(q,x+57,y+109,Q_NOTES,0xfcffff);
        text(q,x+92,y+115,"New note",foreground(q),1);
        surface(q,x+25,y+151,313,58,17,inner_material(q),q->theme?86:96);
        icon(q,x+57,y+180,Q_FILES,0xfcffff);
        text(q,x+92,y+186,"Browse files",foreground(q),1);
    }
}
static void draw_dock(QDesktop *q){
    int x=dock_x(q),y=dock_y(q);
    pane(q,x,y,555,70,25,0xcadceb,q->theme?107:112);
    surface(q,x+11,y+8,52,53,17,accent(q),122);
    text(q,x+28,y+46,"Q",0x173c54,2);
    rectangle(q,x+69,y+14,1,41,0x9cbbce);
    for(int i=0;i<Q_APPS;i++){
        int xx=x+77+i*77;
        if((q->active==i&&q->windows[i].open)||q->hover==i){
            surface(q,xx,y+7,61,54,17,q->theme?0x8fb4cc:0xddecf9,q->theme?94:128);
        }
        icon(q,xx+30,y+32,i,q->theme?0xd7edfd:0xf6fcff);
        if(q->windows[i].open)circular(q,xx+30,y+59,3,accent(q),255);
    }
    if(q->hover>=0&&q->hover<Q_APPS){
        int idx=q->hover,xx=x+77+idx*77;
        int width=text_width(names[idx],0)+26;
        int tx=clamp(xx+30-width/2,12,q->width-width-12);
        surface(q,tx,y-42,width,33,12,q->theme?0x153149:0xe4eff9,q->theme?186:202);
        text(q,tx+13,y-21,names[idx],foreground(q),0);
    }
}
static void draw_menu(QDesktop *q){
    if(!q->menu)return;
    int x=menu_x(q),y=menu_y(q),w=510;
    pane(q,x,y,w,434,26,material(q),q->theme?181:146);
    text(q,x+29,y+49,"Applications",foreground(q),2);
    
    for(int i=0;i<Q_APPS;i++){
        int bx=x+26+(i%3)*159,by=y+112+(i/3)*144;
        surface(q,bx,by,141,126,19,inner_material(q),q->theme?109:128);
        icon(q,bx+70,by+49,i,q->theme?0xecfaff:0xffffff);
        int width=text_width(names[i],1);
        text(q,bx+(141-width)/2,by+104,names[i],foreground(q),1);
    }
}
static void draw_notifications(QDesktop *q){
    if(!q->notifications)return;
    int x=q->width-361,y=71;
    pane(q,x,y,334,209,22,material(q),q->theme?179:153);
    text(q,x+25,y+43,"Notifications",foreground(q),1);
    thin_rule(q,x+25,y+61,285,55);
    text(q,x+27,y+108,"All caught up",foreground(q),1);
    text(q,x+27,y+135,"Updates will appear here.",secondary(q),0);
}
static void draw_quick(QDesktop *q){
    if(!q->quick)return;
    int x=q->width-363,y=72;
    pane(q,x,y,336,293,22,material(q),q->theme?181:159);
    text(q,x+24,y+40,"Control center",foreground(q),1);
    thin_rule(q,x+24,y+58,289,54);
    surface(q,x+19,y+80,298,72,16,inner_material(q),q->theme?103:119);
    text(q,x+37,y+110,"Appearance",foreground(q),1);
    text(q,x+37,y+132,q->theme?"Dark":"Light",secondary(q),0);
    surface(q,x+19,y+164,298,68,16,inner_material(q),q->theme?103:119);
    text(q,x+37,y+191,"Accent",foreground(q),1);
    for(int i=0;i<4;i++)circular(q,x+178+i*32,y+194,10,(uint32_t[]){0x5dcfe3,0x9e9efa,0x77d5af,0xf4ae85}[i],255);
    text(q,x+38,y+266,"Open settings",foreground(q),0);
}
static void close_window(QDesktop *q,int app){
    q->windows[app].open=0;q->active=-1;
    for(int i=0;i<Q_APPS;i++)if(q->windows[i].open&&(q->active<0||q->windows[i].z>q->windows[q->active].z))q->active=i;
    q->dirty=1;
}
static void window_content(QDesktop *q,int app,QWindow *w){
    int x=w->x+32,y=w->y+85,cw=w->w-64;
    uint32_t ink=foreground(q),muted=secondary(q);
    if(app==Q_FILES){
        text(q,x,y,"Files",ink,2);
        surface(q,x,y+30,cw,54,14,inner_material(q),q->theme?95:115);
        text(q,x+21,y+63,"Home     /     Local",ink,0);
        surface(q,x,y+102,cw,133,17,inner_material(q),q->theme?100:126);
        text(q,x+24,y+139,"No volumes mounted",ink,1);
        text(q,x+24,y+169,"Connect a writable disk to browse files.",muted,0);
        surface(q,x+24,y+186,153,33,12,accent(q),145);
        text(q,x+38,y+208,"Open Notes",0x173248,0);
    }else if(app==Q_NOTES){
        text(q,x,y,"Notes",ink,2);
        surface(q,x,y+30,cw,w->h-158,17,inner_material(q),q->theme?74:138);
        string_limit(q,x+22,y+73,q->note_length?q->notes:"Write something...",q->note_length?ink:muted,1,cw-44,clamp((w->h-195)/26,1,20));
    }else if(app==Q_TERMINAL){
        text(q,x,y,"Terminal",ink,2);
        surface(q,x,y+30,cw,w->h-157,17,0x0d2237,221);
        text(q,x+22,y+72,"quartzie >",0x8edee8,1);
        text(q,x+22,y+117,q->status[0]?q->status:"Type help for commands",0xe0f5fb,0);
        text(q,x+22,y+162,">",0x88d8e5,1);
        text(q,x+48,y+162,q->command,0xf8feff,1);
    }else if(app==Q_CALCULATOR){
        text(q,x,y,"Calculator",ink,2);
        surface(q,x,y+27,322,81,16,inner_material(q),q->theme?111:144);
        const char *output=q->calc_length?q->calc:q->answer;
        text(q,x+22,y+80,output,ink,2);
        const char *keys="789/456*123-C0=+";
        for(int i=0;i<16;i++){
            int bx=x+(i%4)*82,by=y+128+(i/4)*57;
            surface(q,bx,by,73,49,13,(i%4)==3?0xa4ddeb:inner_material(q),q->theme?121:151);
            char value[2]={keys[i],0};
            text(q,bx+(73-text_width(value,1))/2,by+32,value,ink,1);
        }
    }else if(app==Q_SETTINGS){
        text(q,x,y,"Settings",ink,2);
        surface(q,x,y+36,cw,91,18,inner_material(q),q->theme?106:139);
        text(q,x+24,y+72,"Appearance",ink,1);
        text(q,x+24,y+102,q->theme?"Dark glass":"Light glass",muted,0);
        surface(q,x,y+143,cw,97,18,inner_material(q),q->theme?106:139);
        text(q,x+24,y+178,"Accent color",ink,1);
        for(int i=0;i<4;i++){
            int bx=x+175+i*50;
            circular(q,bx,y+198,15,(uint32_t[]){0x5dcfe3,0x9e9efa,0x77d5af,0xf4ae85}[i],255);
            if(q->accent==i)circular(q,bx,y+198,5,0xfaffff,205);
        }
    }else{
        text(q,x,y,"Quartzie",ink,2);
        text(q,x,y+54,"A space to create.",ink,1);
        thin_rule(q,x,y+79,clamp(cw-28,1,cw),55);
        text(q,x,y+118,"Keyboard",muted,0);
        text(q,x,y+150,"F1-F6  Open applications",ink,0);
        text(q,x,y+178,"F7  Applications",ink,0);
        text(q,x,y+206,"F8  Notifications",ink,0);
        text(q,x,y+234,"F9  Control center",ink,0);
        text(q,x,y+262,"Esc  Close overlays",ink,0);
    }
}
static void draw_window(QDesktop *q,int app){
    QWindow *w=&q->windows[app];if(!w->open)return;
    pane(q,w->x,w->y,w->w,w->h,25,material(q),q->theme?180:149);
    surface(q,w->x+1,w->y+1,w->w-2,55,23,q->theme?0x304c6a:0xe5f2fc,q->theme?111:129);
    thin_rule(q,w->x+23,w->y+56,w->w-46,45);
    text(q,w->x+29,w->y+36,names[app],foreground(q),1);
    surface(q,w->x+w->w-108,w->y+13,36,29,10,inner_material(q),q->theme?90:151);
    surface(q,w->x+w->w-59,w->y+13,36,29,10,0xeac0cb,q->theme?143:166);
    rectangle(q,w->x+w->w-98,w->y+28,14,2,foreground(q));
    segment(q,w->x+w->w-48,w->y+20,w->x+w->w-35,w->y+33,0x71475d);
    segment(q,w->x+w->w-35,w->y+20,w->x+w->w-48,w->y+33,0x71475d);
    window_content(q,app,w);
}
void q_draw(QDesktop *q){
    if(!q->dirty)return;
    size_t total=(size_t)q->width*q->height;
    for(size_t i=0;i<total;i++)q->pixels[i]=q->wallpaper[i];
    draw_desktop(q);
    int drawn[Q_APPS]={0};
    for(int layer=0;layer<Q_APPS;layer++){
        int app=-1;
        for(int i=0;i<Q_APPS;i++)if(!drawn[i]&&q->windows[i].open&&(app<0||q->windows[i].z<q->windows[app].z))app=i;
        if(app<0)break;
        drawn[app]=1;draw_window(q,app);
    }
    draw_topbar(q);
    draw_dock(q);
    draw_menu(q);
    draw_notifications(q);
    draw_quick(q);
    q->frame_count++;
    q->dirty=0;
}
static void calc_input(QDesktop *q,char key){
    if(key=='C'){q->calc_length=0;q->calc[0]=0;q->answer[0]='0';q->answer[1]=0;return;}
    if(key=='='){
        int a=0,b=0,at=0,sign=1;char op;
        while(q->calc[at]>='0'&&q->calc[at]<='9'){a=clamp(a*10+q->calc[at++]-'0',0,1000000);}
        op=q->calc[at++];if(q->calc[at]=='-'){sign=-1;at++;}
        while(q->calc[at]>='0'&&q->calc[at]<='9'){b=clamp(b*10+q->calc[at++]-'0',0,1000000);}
        b*=sign;
        int n=op=='+'?a+b:op=='-'?a-b:op=='*'?a*b:op=='/'&&b?a/b:0;
        if(op=='/'&&!b){q->answer[0]='E';q->answer[1]='R';q->answer[2]='R';q->answer[3]=0;}
        else{
            char digits[16];int length=0,pos=0,neg=n<0;
            unsigned int v=neg?(unsigned int)(-(int64_t)n):(unsigned int)n;
            do{digits[length++]=(char)('0'+v%10);v/=10;}while(v&&length<15);
            if(neg)q->answer[pos++]='-';
            while(length)q->answer[pos++]=digits[--length];
            q->answer[pos]=0;
        }
        q->calc_length=0;q->calc[0]=0;return;
    }
    if(q->calc_length<30){q->calc[q->calc_length++]=key;q->calc[q->calc_length]=0;}
}
void q_click(QDesktop *q,int x,int y){
    int dock=q_dock_hit(q,x,y);
    if(dock==10){int was=q->menu;close_popups(q);q->menu=!was;q->dirty=1;return;}
    if(dock>=0&&dock<Q_APPS){q_open(q,dock);return;}
    if(dock==11){int was=q->notifications;close_popups(q);q->notifications=!was;q->dirty=1;return;}
    if(dock==12){int was=q->quick;close_popups(q);q->quick=!was;q->dirty=1;return;}
    if(q->menu){
        int mx=menu_x(q),my=menu_y(q);
        if(hit(x,y,mx,my,510,434)){
            for(int i=0;i<Q_APPS;i++)if(hit(x,y,mx+26+i%3*159,my+112+i/3*144,141,126)){q_open(q,i);return;}
            return;
        }
        q->menu=0;q->dirty=1;
    }
    if(q->notifications){
        if(hit(x,y,q->width-361,71,334,209))return;
        q->notifications=0;q->dirty=1;
    }
    if(q->quick){
        int bx=q->width-363,by=72;
        if(hit(x,y,bx+19,by+80,298,72)){q->theme=!q->theme;wallpaper(q);q->dirty=1;return;}
        if(hit(x,y,bx+19,by+164,298,68)){
            if(x>=bx+178&&x<=bx+295)q->accent=clamp((x-(bx+162))/32,0,3);
            else q->accent=(q->accent+1)%4;
            q->dirty=1;return;
        }
        if(hit(x,y,bx+19,by+243,298,43)){q_open(q,Q_SETTINGS);return;}
        if(hit(x,y,bx,by,336,293))return;
        q->quick=0;q->dirty=1;
    }
    if(y<61){
        if(hit(x,y,196,10,58,43)){q_open(q,Q_FILES);return;}
        if(hit(x,y,263,10,65,43)){q_open(q,Q_NOTES);return;}
        if(hit(x,y,335,10,83,43)){q_open(q,Q_SETTINGS);return;}
        return;
    }
    int chosen=-1;
    for(int i=0;i<Q_APPS;i++)if(q->windows[i].open&&hit(x,y,q->windows[i].x,q->windows[i].y,q->windows[i].w,q->windows[i].h)&&
        (chosen<0||q->windows[i].z>q->windows[chosen].z))chosen=i;
    if(chosen>=0){
        QWindow *w=&q->windows[chosen];
        if(hit(x,y,w->x+w->w-59,w->y+13,36,29)||hit(x,y,w->x+w->w-108,w->y+13,36,29)){
            close_window(q,chosen);return;
        }
        q_open(q,chosen);
        if(hit(x,y,w->x,w->y,w->w-125,55)){
            q->drag=chosen;q->drag_x=x-w->x;q->drag_y=y-w->y;return;
        }
        int bx=w->x+32,by=w->y+85;
        if(chosen==Q_SETTINGS){
            if(hit(x,y,bx,by+36,w->w-64,91)){q->theme=!q->theme;wallpaper(q);q->dirty=1;}
            else if(hit(x,y,bx,by+143,w->w-64,97)){
                if(x>=bx+160&&x<=bx+360)q->accent=clamp((x-(bx+150))/50,0,3);
                else q->accent=(q->accent+1)%4;
                q->dirty=1;
            }
        }else if(chosen==Q_FILES){
            if(hit(x,y,bx+24,by+186,153,33)){q_open(q,Q_NOTES);return;}
        }else if(chosen==Q_CALCULATOR){
            const char *keys="789/456*123-C0=+";
            for(int i=0;i<16;i++)if(hit(x,y,bx+(i%4)*82,by+128+(i/4)*57,73,49)){
                calc_input(q,keys[i]);q->dirty=1;break;
            }
        }
        return;
    }
    if(q->width>=1100&&q->height>=660){
        int ax=q->width-395;
        if(hit(x,y,ax+25,108+81,313,58)){q_open(q,Q_NOTES);return;}
        if(hit(x,y,ax+25,108+151,313,58)){q_open(q,Q_FILES);return;}
    }
    int shortcut=desktop_shortcut(q,x,y);
    if(shortcut>=0){q_open(q,shortcut);return;}
    q->active=-1;q->dirty=1;
}
void q_press(QDesktop *q,int x,int y){q->pressed=1;q_click(q,x,y);}
void q_release(QDesktop *q){q->pressed=0;q->drag=-1;}
void q_move(QDesktop *q,int x,int y){
    q->mouse_x=clamp(x,0,q->width-1);q->mouse_y=clamp(y,0,q->height-1);
    if(q->pressed&&q->drag>=0){
        QWindow *w=&q->windows[q->drag];
        w->x=clamp(q->mouse_x-q->drag_x,0,q->width-w->w);
        w->y=clamp(q->mouse_y-q->drag_y,61,q->height-44);
        q->dirty=1;
    }
    int hover=-1;
    int dock=q_dock_hit(q,q->mouse_x,q->mouse_y);
    if(dock>=0&&dock<Q_APPS)hover=dock;
    else if(dock<0&&q->active<0){int index=desktop_shortcut(q,q->mouse_x,q->mouse_y);if(index>=0)hover=index+20;}
    if(hover!=q->hover){q->hover=hover;q->dirty=1;}
}
void q_key(QDesktop *q,uint16_t ch,uint16_t scan){
    if(scan==23||ch==27){close_popups(q);q->dirty=1;return;}
    if(scan>=11&&scan<=16){q_open(q,(int)scan-11);return;}
    if(scan==17){q->menu=!q->menu;q->notifications=0;q->quick=0;q->dirty=1;return;}
    if(scan==18){q->notifications=!q->notifications;q->menu=0;q->quick=0;q->dirty=1;return;}
    if(scan==19){q->quick=!q->quick;q->menu=0;q->notifications=0;q->dirty=1;return;}
    if(ch=='\r'){
        if(q->active==Q_TERMINAL){
            if(q->command_length){
                const char *msg="Unknown command";
                if(q->command[0]=='h')msg="help   clear   files   notes";
                else if(q->command[0]=='c')msg="";
                else if(q->command[0]=='f'){q_open(q,Q_FILES);}
                else if(q->command[0]=='n'){q_open(q,Q_NOTES);}
                for(int i=0;i<95;i++){q->status[i]=msg[i];if(!msg[i])break;}
            }
            q->command_length=0;q->command[0]=0;
        }else if(q->active==Q_NOTES&&q->note_length<2047){q->notes[q->note_length++]='\n';q->notes[q->note_length]=0;}
        q->dirty=1;return;
    }
    if(ch==8){
        if(q->active==Q_NOTES&&q->note_length)q->notes[--q->note_length]=0;
        else if(q->active==Q_TERMINAL&&q->command_length)q->command[--q->command_length]=0;
        else if(q->active==Q_CALCULATOR&&q->calc_length)q->calc[--q->calc_length]=0;
        q->dirty=1;return;
    }
    if(ch<32||ch>126)return;
    if(q->menu&&ch>='1'&&ch<'1'+Q_APPS){q_open(q,ch-'1');return;}
    if(q->active==Q_NOTES&&q->note_length<2047){q->notes[q->note_length++]=(char)ch;q->notes[q->note_length]=0;q->dirty=1;}
    else if(q->active==Q_TERMINAL&&q->command_length<120){q->command[q->command_length++]=(char)ch;q->command[q->command_length]=0;q->dirty=1;}
    else if(q->active==Q_CALCULATOR){calc_input(q,(char)ch);q->dirty=1;}
}
