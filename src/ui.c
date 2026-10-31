#include "ui.h"
#include "font.h"
#include "icons.h"
#include <stddef.h>
#include <stdint.h>

static int clamp(int x,int lo,int hi){return x<lo?lo:x>hi?hi:x;}
static int absval(int x){return x<0?-x:x;}
static int hit(int px,int py,int x,int y,int w,int h){return px>=x&&py>=y&&px<x+w&&py<y+h;}
static uint32_t rgb(int r,int g,int b){return (uint32_t)((r<<16)|(g<<8)|b);}
static uint32_t blend(uint32_t a,uint32_t b,int alpha){
    uint32_t inv=(uint32_t)(256-alpha),weight=(uint32_t)alpha;
    uint32_t rb=(a&0x00ff00ffu)*inv+(b&0x00ff00ffu)*weight;
    uint32_t green=(a&0x0000ff00u)*inv+(b&0x0000ff00u)*weight;
    return ((rb>>8)&0x00ff00ffu)|((green>>8)&0x0000ff00u);
}
typedef struct {
    uint32_t ink,muted,material,inset,dock,hover,selected,tooltip;
    uint32_t operator_button,close_button,close_ink,rule,shadow,rim,highlight;
} QPalette;
static const QPalette light_palette={
    0x122c49,0x365676,0xdbebf9,0xe7f4ff,0xcadceb,0xddecf9,0xc6e2ff,0xe4eff9,
    0xa4ddeb,0xeac0cb,0x71475d,0xe1f3ff,0x102743,0xffffff,0xf7fbff
};
static const QPalette dark_palette={
    0xeaf5ff,0xa8c2da,0x101c2e,0x1a2d44,0x142539,0x283e59,0x324b6a,0x192e46,
    0x2d455f,0x462934,0xffdbe2,0x7295b5,0x07101b,0x83a5c2,0x7da8cc
};
static const QPalette *colors(QDesktop *q){return q->theme?&dark_palette:&light_palette;}
static void pixel(QDesktop *q,int x,int y,uint32_t color){
    if(x>=q->clip_left&&x<q->clip_right&&y>=q->clip_top&&y<q->clip_bottom)q->pixels[(size_t)y*q->width+x]=color;
}
static void rectangle(QDesktop *q,int x,int y,int w,int h,uint32_t color){
    int x0=clamp(x,q->clip_left,q->clip_right),x1=clamp(x+w,q->clip_left,q->clip_right);
    int y0=clamp(y,q->clip_top,q->clip_bottom),y1=clamp(y+h,q->clip_top,q->clip_bottom);
    for(int yy=y0;yy<y1;yy++){
        uint32_t *p=q->pixels+(size_t)yy*q->width+x0;
        for(int xx=x0;xx<x1;xx++)*p++=color;
    }
}
static int round_sample16(int sx,int sy,int w,int h,int r){
    int rw=w*16,rh=h*16,rr=r*16;
    if(sx<0||sy<0||sx>=rw||sy>=rh)return 0;
    if(r<=0)return 1;
    int cx=sx<rr?rr:sx>=rw-rr?rw-rr-1:sx;
    int cy=sy<rr?rr:sy>=rh-rr?rh-rr-1:sy;
    int dx=sx-cx,dy=sy-cy;
    return dx*dx+dy*dy<=rr*rr;
}
static int round_cover(int x,int y,int w,int h,int r){
    if(x<0||y<0||x>=w||y>=h)return 0;
    if(r<=0||(x>=r&&x<w-r)||(y>=r&&y<h-r))return 256;
    return (round_sample16(x*16+4,y*16+4,w,h,r)+
            round_sample16(x*16+12,y*16+4,w,h,r)+
            round_sample16(x*16+4,y*16+12,w,h,r)+
            round_sample16(x*16+12,y*16+12,w,h,r))*64;
}
static void circular(QDesktop *q,int cx,int cy,int r,uint32_t color,int alpha){
    if(r<=0)return;
    int x0=clamp(cx-r-1,q->clip_left,q->clip_right),x1=clamp(cx+r+2,q->clip_left,q->clip_right);
    int y0=clamp(cy-r-1,q->clip_top,q->clip_bottom),y1=clamp(cy+r+2,q->clip_top,q->clip_bottom);
    const int rr=r*16;
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){
        int cover=0;
        for(int j=0;j<2;j++)for(int i=0;i<2;i++){
            int dx=x*16+4+i*8-cx*16,dy=y*16+4+j*8-cy*16;
            if(dx*dx+dy*dy<=rr*rr)cover+=64;
        }
        if(!cover)continue;
        uint32_t *p=q->pixels+(size_t)y*q->width+x;
        *p=blend(*p,color,cover*alpha/256);
    }
}
static void segment(QDesktop *q,int ax,int ay,int bx,int by,uint32_t color){
    int dx=bx-ax,dy=by-ay;
    int x0=clamp((ax<bx?ax:bx)-2,q->clip_left,q->clip_right);
    int x1=clamp((ax>bx?ax:bx)+3,q->clip_left,q->clip_right);
    int y0=clamp((ay<by?ay:by)-2,q->clip_top,q->clip_bottom);
    int y1=clamp((ay>by?ay:by)+3,q->clip_top,q->clip_bottom);
    int64_t length=(int64_t)dx*dx+(int64_t)dy*dy;
    if(!length){pixel(q,ax,ay,color);return;}
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){
        int cover=0;
        for(int sy=0;sy<2;sy++)for(int sx=0;sx<2;sx++){
            int64_t px=(int64_t)x*4+1+sx*2-(int64_t)ax*4;
            int64_t py=(int64_t)y*4+1+sy*2-(int64_t)ay*4;
            int64_t t=(px*dx+py*dy);
            if(t<0)t=0;
            if(t>length*4)t=length*4;
            int64_t cross=px*4*length-t*4*dx;
            int64_t other=py*4*length-t*4*dy;
            if(cross*cross+other*other<=length*length*64)cover+=64;
        }
        if(cover){uint32_t *p=q->pixels+(size_t)y*q->width+x;*p=blend(*p,color,cover);}
    }
}
static void soft_shadow(QDesktop *q,int x,int y,int w,int h,int r,int spread,int intensity){
    int x0=clamp(x-spread,q->clip_left,q->clip_right),x1=clamp(x+w+spread,q->clip_left,q->clip_right);
    int y0=clamp(y-spread,q->clip_top,q->clip_bottom),y1=clamp(y+h+spread,q->clip_top,q->clip_bottom);
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
            *dst=blend(*dst,colors(q)->shadow,strength);
        }
    }
}
#define GLASS_SCALE 4
#define GLASS_SIDE (Q_MAX_W / GLASS_SCALE + 16)
#define GLASS_ROWS (Q_MAX_H / GLASS_SCALE + 16)
#if defined(QUARTZIE_FIRMWARE)
static uint32_t *glass_source,*glass_horizontal,*glass_final,*glass_cached;
static uint32_t *scene_from,*scene_to,*scene_back;
void q_ui_set_scratch(uint32_t *storage){
    glass_source=storage;
    glass_horizontal=glass_source+GLASS_SIDE*GLASS_ROWS;
    glass_final=glass_horizontal+GLASS_SIDE*GLASS_ROWS;
    glass_cached=glass_final+GLASS_SIDE*GLASS_ROWS;
    scene_from=glass_cached+GLASS_SIDE*GLASS_ROWS;
    scene_to=scene_from+Q_MAX_W*Q_MAX_H;
    scene_back=scene_to+Q_MAX_W*Q_MAX_H;
}
#else
static uint32_t glass_source[GLASS_SIDE * GLASS_ROWS];
static uint32_t glass_horizontal[GLASS_SIDE * GLASS_ROWS];
static uint32_t glass_final[GLASS_SIDE * GLASS_ROWS];
static uint32_t glass_cached[GLASS_SIDE * GLASS_ROWS];

static uint32_t scene_from[Q_MAX_W*Q_MAX_H];
static uint32_t scene_to[Q_MAX_W*Q_MAX_H];
static uint32_t scene_back[Q_MAX_W*Q_MAX_H];
void q_ui_set_scratch(uint32_t *storage){(void)storage;}
#endif
static int glass_cache_cols,glass_cache_rows;
static const int glass_weight[7]={1,2,3,4,3,2,1};
static void glass_filter(QDesktop *q,int sx,int sy,int cols,int rows){
    const uint32_t *source=q->render_topbar?q->wallpaper:q->pixels;
    for(int yy=0;yy<rows;yy++){
        int y=clamp(sy+yy*GLASS_SCALE,0,q->height-1);
        int y1=clamp(y+2,0,q->height-1);
        for(int xx=0;xx<cols;xx++){
            int x=clamp(sx+xx*GLASS_SCALE,0,q->width-1);
            int x1=clamp(x+2,0,q->width-1);
            uint32_t a=source[(size_t)y*q->width+x];
            uint32_t b=source[(size_t)y*q->width+x1];
            uint32_t c=source[(size_t)y1*q->width+x];
            uint32_t d=source[(size_t)y1*q->width+x1];
            glass_source[(size_t)yy*cols+xx]=rgb(((a>>16&255)+(b>>16&255)+(c>>16&255)+(d>>16&255))/4,
                ((a>>8&255)+(b>>8&255)+(c>>8&255)+(d>>8&255))/4,
                ((a&255)+(b&255)+(c&255)+(d&255))/4);
        }
    }
    for(int yy=0;yy<rows;yy++)for(int xx=0;xx<cols;xx++){
        uint32_t rb=0,green=0;
        for(int k=-3;k<=3;k++){
            uint32_t c=glass_source[(size_t)yy*cols+clamp(xx+k,0,cols-1)];
            uint32_t weight=(uint32_t)glass_weight[k+3];
            rb+=(c&0x00ff00ffu)*weight;
            green+=(c&0x0000ff00u)*weight;
        }
        glass_horizontal[(size_t)yy*cols+xx]=((rb>>4)&0x00ff00ffu)|((green>>4)&0x0000ff00u);
    }
    for(int yy=0;yy<rows;yy++)for(int xx=0;xx<cols;xx++){
        uint32_t rb=0,green=0;
        for(int k=-3;k<=3;k++){
            uint32_t c=glass_horizontal[(size_t)clamp(yy+k,0,rows-1)*cols+xx];
            uint32_t weight=(uint32_t)glass_weight[k+3];
            rb+=(c&0x00ff00ffu)*weight;
            green+=(c&0x0000ff00u)*weight;
        }
        glass_final[(size_t)yy*cols+xx]=((rb>>4)&0x00ff00ffu)|((green>>4)&0x0000ff00u);
    }
}
static uint32_t glass_fetch(const uint32_t *buffer,int px,int py,int sx,int sy,int cols,int rows){
    int dx=px-sx,dy=py-sy;
    int ax=clamp(dx/GLASS_SCALE,0,cols-1),ay=clamp(dy/GLASS_SCALE,0,rows-1);
    int bx=clamp(ax+1,0,cols-1),by=clamp(ay+1,0,rows-1);
    int fx=dx%GLASS_SCALE,fy=dy%GLASS_SCALE;
    if(fx<0)fx=0;
    if(fy<0)fy=0;
    uint32_t a=buffer[(size_t)ay*cols+ax];
    uint32_t b=buffer[(size_t)ay*cols+bx];
    uint32_t c=buffer[(size_t)by*cols+ax];
    uint32_t d=buffer[(size_t)by*cols+bx];
    int w00=(GLASS_SCALE-fx)*(GLASS_SCALE-fy);
    int w10=fx*(GLASS_SCALE-fy),w01=(GLASS_SCALE-fx)*fy,w11=fx*fy;
    uint32_t rb=(a&0x00ff00ffu)*(uint32_t)w00+(b&0x00ff00ffu)*(uint32_t)w10+
        (c&0x00ff00ffu)*(uint32_t)w01+(d&0x00ff00ffu)*(uint32_t)w11;
    uint32_t green=(a&0x0000ff00u)*(uint32_t)w00+(b&0x0000ff00u)*(uint32_t)w10+
        (c&0x0000ff00u)*(uint32_t)w01+(d&0x0000ff00u)*(uint32_t)w11;
    return ((rb>>4)&0x00ff00ffu)|((green>>4)&0x0000ff00u);
}
static void surface(QDesktop *q,int x,int y,int w,int h,int radius,uint32_t tint,int alpha){
    if(w<=0||h<=0)return;
    int x0=clamp(x,q->clip_left,q->clip_right),x1=clamp(x+w,q->clip_left,q->clip_right);
    int y0=clamp(y,q->clip_top,q->clip_bottom),y1=clamp(y+h,q->clip_top,q->clip_bottom);
    if(x0>=x1||y0>=y1)return;
    int cached=q->glass_cached_app>=0&&q->render_app==q->glass_cached_app&&glass_cache_cols>0;
    int sx=cached?0:clamp(x0-16,0,q->width-1);
    int sy=cached?0:clamp(y0-16,0,q->height-1);
    int ex=cached?q->width:clamp(x1+20,0,q->width);
    int ey=cached?q->height:clamp(y1+20,0,q->height);
    int cols=cached?glass_cache_cols:(ex-sx+GLASS_SCALE-1)/GLASS_SCALE;
    int rows=cached?glass_cache_rows:(ey-sy+GLASS_SCALE-1)/GLASS_SCALE;
    if(cols<1||rows<1||cols>GLASS_SIDE||rows>GLASS_ROWS)return;
    if(!cached)glass_filter(q,sx,sy,cols,rows);
    int bw=q->width,dark=q->theme;
    int texture=radius>=16&&w>=150&&h>=60;
    uint32_t highlight=colors(q)->highlight,rim_color=colors(q)->rim;
    int top_alpha=dark?28:100,edge_alpha=dark?14:47;
    uint32_t lower_color=dark?0x050b14u:0x557099u;
    int lower_alpha=dark?40:17;
    for(int py=y0;py<y1;py++){
        int dy=py-y;
        int sheen=dy<h/2?18*(h/2-dy)/(h/2+1):0;
        int material=clamp(alpha-44+sheen,12,190);
        size_t row=(size_t)py*bw;
        for(int px=x0;px<x1;px++){
            int dx=px-x;
            int cover=radius?round_cover(dx,dy,w,h,radius):256;
            if(!cover)continue;
            size_t at=row+px;
            uint32_t backdrop=cached?q->blurred[at]:glass_fetch(glass_final,px,py,sx,sy,cols,rows);
            uint32_t c=blend(backdrop,tint,material);
            if(texture){
                uint32_t noise=(uint32_t)px*0x9e3779b1u^(uint32_t)py*0x85ebca6bu;
                noise^=noise>>15;
                c=blend(c,(noise&1u)?0xffffffu:0x000000u,(int)((noise>>5)&3u));
            }
            int rim=0;
            if(radius){
                int inner=round_cover(dx-1,dy-1,w-2,h-2,radius>1?radius-1:0);
                rim=cover-inner;
            }else if(dx==0||dy==0||dx==w-1||dy==h-1)rim=256;
            if(dy<2)c=blend(c,highlight,top_alpha);
            if(dx<2||dy<2)c=blend(c,rim_color,edge_alpha);
            if(dx>w-3||dy>h-3)c=blend(c,lower_color,lower_alpha);
            if(rim>0)c=blend(c,rim_color,dark?clamp(rim/5,0,48):clamp(rim/3,0,72));
            q->pixels[at]=cover==256?c:blend(q->pixels[at],c,cover);
        }
    }
}
static void pane(QDesktop *q,int x,int y,int w,int h,int r,uint32_t tint,int alpha){
    soft_shadow(q,x,y,w,h,r,32,46);
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
            if(py<q->clip_top||py>=q->clip_bottom)continue;
            for(int xx=0;xx<g->width;xx++){
                int px=x+g->left+xx;
                if(px<q->clip_left||px>=q->clip_right)continue;
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
static uint32_t foreground(QDesktop *q){return colors(q)->ink;}
static uint32_t secondary(QDesktop *q){return colors(q)->muted;}
static uint32_t material(QDesktop *q){return colors(q)->material;}
static uint32_t inner_material(QDesktop *q){return colors(q)->inset;}
static uint32_t accent(QDesktop *q){
    static const uint32_t shades[]={0x5dcfe3,0x9e9efa,0x77d5af,0xf4ae85,0xef709a,0xe8ca70,0x69a6ff,0xadaee7};
    return shades[q->accent%8];
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
        if(q->wallpaper_mode==1){r+=85;g+=18;b-=11;}
        else if(q->wallpaper_mode==2){r-=2;g+=80;b+=31;}
        q->wallpaper[(size_t)y*w+x]=rgb(clamp(r,0,255),clamp(g,0,255),clamp(b,0,255));
    }
}
static void thin_rule(QDesktop *q,int x,int y,int w,int alpha){
    if(y<q->clip_top||y>=q->clip_bottom)return;
    for(int xx=clamp(x,q->clip_left,q->clip_right);xx<clamp(x+w,q->clip_left,q->clip_right);xx++){
        uint32_t *p=q->pixels+(size_t)y*q->width+xx;
        *p=blend(*p,colors(q)->rule,alpha);
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
static void sprite(QDesktop *q,int cx,int cy,int sprite_id,int size){
    const uint32_t *source=q_icon_pixels_sized((unsigned int)sprite_id,size);
    if(!source||size<1)return;
    int source_size=(size==25||size==27||size==28||size==40||size==43||size==58)?size:Q_ICON_SIZE;
    int x=cx-size/2,y=cy-size/2;
    int x0=clamp(x,q->clip_left,q->clip_right),x1=clamp(x+size,q->clip_left,q->clip_right);
    int y0=clamp(y,q->clip_top,q->clip_bottom),y1=clamp(y+size,q->clip_top,q->clip_bottom);
    if(x0>=x1||y0>=y1)return;
    for(int py=y0;py<y1;py++){
        int sy=(py-y)*source_size/size;
        for(int px=x0;px<x1;px++){
            int sx=(px-x)*source_size/size;
            uint32_t raw=source[sy*source_size+sx];
            int alpha=(int)(raw>>24);
            if(!alpha)continue;
            uint32_t *dst=q->pixels+(size_t)py*q->width+px;
            *dst=blend(*dst,raw&0xffffffu,alpha==255?256:alpha);
        }
    }
}
static void app_icon(QDesktop *q,int cx,int cy,int app,int size,uint32_t color){
    int icon_id=-1;
    if(app==Q_FILES)icon_id=QI_FILE_EXPLORER;
    else if(app==Q_NOTES)icon_id=QI_NOTES;
    else if(app==Q_TERMINAL)icon_id=QI_CMD;
    else if(app==Q_SETTINGS)icon_id=QI_SETTINGS;
    else if(app==Q_CALCULATOR)icon_id=QI_CALCULATOR;
    else if(app==Q_ABOUT)icon_id=QI_ABOUT;
    else if(app==Q_STORE)icon_id=QI_STORE;
    else if(app==Q_PHOTOS)icon_id=QI_IMAGE;
    else if(app==Q_MEDIA)icon_id=QI_MEDIA_PLAYER;
    if(app==Q_TASKS){
        int side=size*3/4;
        surface(q,cx-side/2,cy-side/2,side,side,side/5,0x316a9e,238);
        for(int i=0;i<4;i++){
            int bar=side*(i+2)/8;
            rectangle(q,cx-side/2+side/8+i*side/5,cy+side/2-side/8-bar,side/9,bar,0xe2f8ff);
        }
    }else if(icon_id>=0)sprite(q,cx,cy,icon_id,size);
    else icon(q,cx,cy,app,color);
}
static int dock_x(QDesktop *q){return (q->width-632)/2;}
static int dock_y(QDesktop *q){return q->height-89;}
static int menu_x(QDesktop *q){return (q->width-510)/2;}
static int menu_y(QDesktop *q){return clamp(q->height-607,54,q->height-100);}
static const char *names[Q_APPS]={"Files","Notes","Terminal","Calculator","Settings","About","Store","Photos","Media Player","Task Manager"};
static int menu_match(QDesktop *q,int app){
    if(!q->menu_search_length)return 1;
    const char *name=names[app];
    for(int start=0;name[start];start++){
        int j=0;
        while(q->menu_search[j]&&name[start+j]){
            char a=name[start+j],b=q->menu_search[j];
            if(a>='A'&&a<='Z')a=(char)(a+32);
            if(b>='A'&&b<='Z')b=(char)(b+32);
            if(a!=b)break;
            j++;
        }
        if(!q->menu_search[j])return 1;
    }
    return 0;
}
static int menu_app_at(QDesktop *q,int slot){
    for(int i=0;i<Q_APPS;i++)if(menu_match(q,i)){
        if(!slot)return i;
        slot--;
    }
    return -1;
}
static int menu_slot_of(QDesktop *q,int app){
    int slot=0;
    for(int i=0;i<Q_APPS;i++)if(menu_match(q,i)){
        if(i==app)return slot;
        slot++;
    }
    return -1;
}
void q_init(QDesktop *q,int w,int h,uint32_t *pixels,uint32_t *wallpaper_pixels,uint32_t *workspace){
    uint8_t *data=(uint8_t *)q;
    for(size_t n=0;n<sizeof(*q);n++)data[n]=0;
    q->width=w;q->height=h;q->pixels=pixels;q->wallpaper=wallpaper_pixels;q->blurred=workspace;
    q->clip_left=0;q->clip_top=0;q->clip_right=w;q->clip_bottom=h;
    q->file_view=1;q->file_nav=1;q->file_extensions=1;q->file_clip_volume=-1;q->file_history_index=-1;
    q->file_selected=-1;q->file_click_index=-1;q->file_path[0]='\\';q->file_path[1]=0;q->note_volume=-1;q->terminal_path[0]='\\';q->terminal_path[1]=0;
    q->update_target=-1;q->update_source_volume=-1;q->store_confirm_remove=-1;q->media_volume_level=80;q->task_selected=-1;q->snap_menu_app=-1;q->active=-1;q->drag=-1;q->hover=-1;q->hover_last=-1;q->menu_hover=-1;q->render_app=-1;q->glass_cached_app=-1;q->mouse_x=w/2;q->mouse_y=h/2;q->dirty=1;q->answer[0]='0';
    for(int i=0;i<Q_APPS;i++){
        QWindow *a=&q->windows[i];
        a->w=clamp(w-210,530,880);a->h=clamp(h-170,390,600);
        a->x=(w-a->w)/2+(i%3)*18-18;a->y=(h-a->h)/2+(i%3)*16-16;
    }
    QWindow *explorer=&q->windows[Q_FILES];
    explorer->w=clamp(w-76,720,1260);explorer->h=clamp(h-100,530,830);
    explorer->x=(w-explorer->w)/2;explorer->y=(h-explorer->h)/2;
    QWindow *settings=&q->windows[Q_SETTINGS];
    settings->w=clamp(w-124,640,1060);
    settings->h=clamp(h-104,470,750);
    settings->x=(w-settings->w)/2;
    settings->y=(h-settings->h)/2;
    wallpaper(q);
}
static void close_popups(QDesktop *q){
    q->menu=0;q->menu_hover=-1;q->notifications=0;q->quick=0;
    q->menu_search_length=0;q->menu_search[0]=0;
    q->desktop_view=0;q->snap_menu_app=-1;
}
static const char *preferences_path(QDesktop *q){
    static char path[QFS_PATH];
    if(qfs_profile_path(q->storage,"QUARTZIE.CFG",path,sizeof(path))!=0)return "\\QUARTZIE.CFG";
    return path;
}
int q_save_preferences(QDesktop *q){
    if(!q||!q->storage)return -1;
    const uint8_t data[10]={'Q','Z','P','R',1,(uint8_t)q->theme,(uint8_t)q->accent,
        (uint8_t)q->animations_enabled,(uint8_t)q->wallpaper_mode,0};
    char original[QFS_PATH],previous[QFS_PATH];
    if(qfs_profile_path(q->storage,"QUARTZIE.CFG",original,sizeof(original))!=0||
       qfs_profile_path(q->storage,"QUARTZIE.BAK",previous,sizeof(previous))!=0)return -1;
    uint8_t current[10];size_t n=0;
    if(qfs_read(q->storage,q->storage->home_index,original,current,sizeof(current),&n)==0&&n==10){
        qfs_delete(q->storage,q->storage->home_index,previous);
        qfs_copy(q->storage,q->storage->home_index,original,q->storage->home_index,previous);
    }
    return qfs_write(q->storage,q->storage->home_index,original,data,sizeof(data));
}
void q_set_accent(QDesktop *q,int value){
    if(!q||value<0||value>7||q->accent==value)return;
    q->accent=value;q->dirty=1;q->hover_only=0;
    q->drag_cache_valid=0;q->dock_cache_valid=0;q->glass_cached_app=-1;
    q_save_preferences(q);
}

void q_set_theme(QDesktop *q,int dark){
    dark=dark!=0;
    if(q->theme==dark)return;
    q->theme=dark;
    q->hover_only=0;
    q->drag_cache_valid=0;
    q->dock_cache_valid=0;
    q->glass_cached_app=-1;
    q->scene_tween_pending=0;
    q->scene_tween_active=0;
    q->damage_valid=0;
    q->dirty=1;
    q_save_preferences(q);
}

static void wallpaper_apply(QDesktop *q,const QMedia *media){
    for(int y=0;y<q->height;y++){
        int sy=y*media->height/q->height;
        for(int x=0;x<q->width;x++){
            int sx=x*media->width/q->width;
            q->wallpaper[(size_t)y*q->width+x]=qm_pixel(media,sx,sy);
        }
    }
    q->dock_cache_valid=0;q->drag_cache_valid=0;q->glass_cached_app=-1;
    q->hover_only=0;q->scene_tween_active=0;q->scene_tween_pending=0;q->dirty=1;
}
static int wallpaper_saved(QDesktop *q){
    if(!q->storage||!q->media_buffer)return -1;
    char path[QFS_PATH];size_t length=0;QMedia image;
    if(qfs_profile_path(q->storage,"WALLPAPER.QIMG",path,sizeof(path))!=0||
       qfs_read(q->storage,q->storage->home_index,path,q->media_buffer,q->media_capacity,&length)!=0||
       qm_open(&image,q->media_buffer,length)!=0||image.kind!=QM_IMAGE)return -1;
    q->media.playing=0;q->media.kind=QM_NONE;
    wallpaper_apply(q,&image);
    return 0;
}
int q_wallpaper_mode(QDesktop *q,int mode){
    if(!q||mode<0||mode>3)return -1;
    if(mode==3&&wallpaper_saved(q)!=0)return -1;
    q->wallpaper_mode=mode;
    if(mode!=3){
        wallpaper(q);
        q->scene_tween_active=0;q->scene_tween_pending=0;
        q->dock_cache_valid=0;q->drag_cache_valid=0;q->glass_cached_app=-1;
        q->hover_only=0;q->dirty=1;
    }
    q_save_preferences(q);
    return 0;
}
int q_wallpaper_photo(QDesktop *q){
    if(!q||!q->storage||q->media.kind!=QM_IMAGE||!q->media.data)return -1;
    char path[QFS_PATH];
    if(qfs_profile_path(q->storage,"WALLPAPER.QIMG",path,sizeof(path))!=0||
       qfs_write(q->storage,q->storage->home_index,path,q->media.data,q->media.length)!=0)return -1;
    wallpaper_apply(q,&q->media);
    q->wallpaper_mode=3;q_save_preferences(q);
    return 0;
}

static void begin_scene_tween(QDesktop *q,int kind){
    if(!q->animations_enabled||q->drag>=0)return;
    size_t total=(size_t)q->width*q->height;
    for(size_t i=0;i<total;i++)scene_from[i]=q->pixels[i];
    q->scene_tween_kind=kind;
    q->scene_tween_step=0;
    q->scene_elapsed_us=0;
    q->scene_paint_us=0;
    q->scene_tween_pending=1;
    q->scene_tween_active=0;
    q->hover_only=0;
    q->drag_cache_valid=0;
}
static void start_tween(QDesktop *q,int app,int kind,int x,int y,int w,int h){
    if(!q->animations_enabled)return;
    QWindow *window=&q->windows[app];
    window->tween_kind=kind;
    q->scene_tween_app=app;
    window->tween_step=0;
    window->tween_x=x;window->tween_y=y;
    window->tween_w=w;window->tween_h=h;
    q->drag_cache_valid=0;
}
void q_enable_animations(QDesktop *q){
    if(!q->settings_loaded)q->animations_enabled=1;
}
void q_tick_timed(QDesktop *q,uint32_t elapsed_us){
    q->uptime_us+=elapsed_us;
    if(q->windows[Q_TASKS].open&&!q->windows[Q_TASKS].minimized){
        q->task_refresh_us+=elapsed_us;
        if(q->task_refresh_us>=1000000u){q->task_refresh_us%=1000000u;q->dirty=1;q->hover_only=0;}
    }
    if(q->file_click_index>=0){
        if(elapsed_us>=420000u-q->file_click_elapsed_us){
            q->file_click_index=-1;
            q->file_click_elapsed_us=0;
        }else q->file_click_elapsed_us+=elapsed_us;
    }
    int media_playing=q->media.playing;
    if(qm_advance(&q->media,elapsed_us)){
        if(media_playing&&!q->media.playing&&q->media.kind!=QM_IMAGE){
            if(q->media_track_index+1<q->media_track_count||q->media_repeat)
                q_media_next(q,1);
        }
        if(q->media.kind==QM_VIDEO){q->dirty=1;q->hover_only=0;}
        else if(q->media.kind==QM_AUDIO){
            q->audio_display_us+=elapsed_us;
            if(q->audio_display_us>=250000u){q->audio_display_us%=250000u;q->dirty=1;q->hover_only=0;}
        }
    }
    if(!q->animations_enabled)return;
    if(elapsed_us>250000)elapsed_us=250000;
    int frame_advance=0;
    if(q->scene_tween_active){
        if(q->drag>=0){
            q->scene_tween_active=0;q->scene_tween_pending=0;q->dirty=1;
        }else{
            int duration=(q->scene_tween_kind==1||q->scene_tween_kind==7)?110000:(q->scene_tween_kind==2)?125000:130000;
            uint32_t remaining=q->scene_elapsed_us<(uint32_t)duration?(uint32_t)duration-q->scene_elapsed_us:0;
            if(elapsed_us>=remaining)q->scene_elapsed_us=(uint32_t)duration;
            else q->scene_elapsed_us+=elapsed_us;
            q->scene_paint_us+=elapsed_us;
            if(q->scene_paint_us>=16000||q->scene_elapsed_us==(uint32_t)duration){
                q->scene_paint_us%=16000;
                if(q->scene_tween_step<256)q->scene_tween_step++;
                q->dirty=1;
                q->hover_only=0;
                frame_advance=1;
            }
        }
    }
    int dock_change=0;
    q->dock_elapsed_us+=elapsed_us;
    uint32_t steps=q->dock_elapsed_us/8000;
    if(steps>16)steps=16;
    if(steps)q->dock_elapsed_us%=8000;
    if(steps){
        int target=q->start_hover?256:0;
        if(q->start_lift!=target){
            int advance=(int)(steps*24);
            int next=target>q->start_lift?clamp(q->start_lift+advance,0,256):clamp(q->start_lift-advance,0,256);
            if(next!=q->start_lift){q->start_lift=next;dock_change=1;}
        }
        if(q->start_pulse>0){
            int pulse=q->start_pulse-(int)(steps*20);
            q->start_pulse=pulse>0?pulse:0;
            dock_change=1;
        }
    }
    for(int i=0;i<Q_APPS;i++){
        if(steps&&q->menu){
            int target=q->menu_hover==i?256:0;
            if(q->menu_lift[i]!=target){
                int advance=(int)(steps*24);
                int next=target>q->menu_lift[i]?clamp(q->menu_lift[i]+advance,0,256):clamp(q->menu_lift[i]-advance,0,256);
                if(next!=q->menu_lift[i]){q->menu_lift[i]=next;dock_change=1;}
            }
        }else if(!q->menu)q->menu_lift[i]=0;
        QWindow *w=&q->windows[i];
        if(w->tween_kind){
            if(q->drag==i||(!q->scene_tween_pending&&!q->scene_tween_active)){
                w->tween_kind=0;w->tween_step=0;
            }else if(frame_advance&&w->tween_step<8)w->tween_step++;
        }
        if(!steps)continue;
        int target=q->hover==i?256:0;
        int current=q->dock_lift[i];
        if(current!=target){
            int advance=(int)(steps*24);
            int next=target>current?clamp(current+advance,0,256):clamp(current-advance,0,256);
            q->dock_lift[i]=next;dock_change=1;
        }
        if(q->dock_pulse[i]>0){
            int pulse=q->dock_pulse[i]-(int)(steps*20);
            q->dock_pulse[i]=pulse>0?pulse:0;
            dock_change=1;
        }
    }
    if(dock_change){
        if(!q->dirty&&q->drag<0)q->hover_only=1;
        q->dirty=1;
    }
}
void q_tick(QDesktop *q){
    q_tick_timed(q,12000);
}
void q_open(QDesktop *q,int app){
    q->hover_only=0;
    if(app<0||app>=Q_APPS)return;
    q->menu_hover=-1;
    int z=0;for(int i=0;i<Q_APPS;i++)if(q->windows[i].z>z)z=q->windows[i].z;
    if(z>10000){for(int i=0;i<Q_APPS;i++)q->windows[i].z=i+1;z=Q_APPS;}
    int was_hidden=!q->windows[app].open||q->windows[app].minimized;
    if(q->windows[app].open&&q->windows[app].desktop!=q->desktop)q_switch_desktop(q,q->windows[app].desktop);
    q->windows[app].open=1;q->windows[app].minimized=0;q->windows[app].z=z+1;q->active=app;
    if(was_hidden){
        QWindow *w=&q->windows[app];
        if(w->z==z+1&&w->desktop!=q->desktop)w->desktop=q->desktop;
        start_tween(q,app,1,w->x+12,w->y+20,w->w-24,w->h-40);
        begin_scene_tween(q,1);
    }
    close_popups(q);q->dirty=1;
}
int q_dock_hit(QDesktop *q,int x,int y){
    int left=dock_x(q),top=dock_y(q);
    if(hit(x,y,left+11,top+8,52,53))return 10;
    for(int i=0;i<Q_DOCK_APPS;i++)if(hit(x,y,left+77+i*77,top+8,61,53))return i;
    if(hit(x,y,left+608,top+12,24,48))return 13;
    return -1;
}
static void draw_dock(QDesktop *q){
    int x=dock_x(q),y=dock_y(q);
    pane(q,x,y,632,70,25,colors(q)->dock,q->theme?205:133);
    int start_lift=q->animations_enabled?q->start_lift*5/256:0;
    if(q->menu||q->start_hover||q->start_lift>0)
        surface(q,x+7,y+7-start_lift,60,54,17,colors(q)->hover,q->theme?174:144);
    if(q->start_pulse>0)circular(q,x+37,y+36-start_lift,23,accent(q),q->start_pulse/4);
    sprite(q,x+37,y+33-start_lift,QI_START,43);
    rectangle(q,x+69,y+14,1,41,colors(q)->rule);
    for(int i=0;i<Q_DOCK_APPS;i++){
        int xx=x+77+i*77;
        int lift=q->animations_enabled?q->dock_lift[i]*5/256:0;
        int pulse=q->dock_pulse[i];
        if((q->active==i&&q->windows[i].open&&!q->windows[i].minimized)||q->hover==i||q->dock_lift[i]>0){
            surface(q,xx,y+7-lift,61,54,17,colors(q)->hover,q->theme?174:128);
        }
        if(pulse>0)circular(q,xx+30,y+36-lift,22,accent(q),pulse/4);
        app_icon(q,xx+30,y+36-lift,i,40,q->theme?0xd7edfd:0xf6fcff);
        if(q->windows[i].open)circular(q,xx+30,y+59,3,q->windows[i].desktop!=q->desktop?secondary(q):q->windows[i].minimized?secondary(q):accent(q),255);
    }
    surface(q,x+608,y+13,23,43,8,q->desktop_view?colors(q)->selected:inner_material(q),q->theme?176:139);
    for(int i=0;i<3;i++)rectangle(q,x+614,y+21+i*10,11,6,i==q->desktop?accent(q):secondary(q));
    if(q->start_hover){
        int width=text_width("Start",0)+26;
        int tx=clamp(x+37-width/2,12,q->width-width-12);
        surface(q,tx,y-42,width,33,12,colors(q)->tooltip,q->theme?198:202);
        text(q,tx+13,y-21,"Start",foreground(q),0);
    }else if(q->hover>=0&&q->hover<Q_DOCK_APPS){
        int idx=q->hover,xx=x+77+idx*77;
        int width=text_width(names[idx],0)+26;
        int tx=clamp(xx+30-width/2,12,q->width-width-12);
        surface(q,tx,y-42,width,33,12,colors(q)->tooltip,q->theme?198:202);
        text(q,tx+13,y-21,names[idx],foreground(q),0);
    }
}
static void draw_menu(QDesktop *q){
    if(!q->menu)return;
    int x=menu_x(q),y=menu_y(q),w=510;
    pane(q,x,y,w,434,26,material(q),q->theme?209:146);
    text(q,x+29,y+49,"Applications",foreground(q),2);
    surface(q,x+26,y+64,457,39,12,inner_material(q),q->theme?123:164);
    text(q,x+41,y+90,q->menu_search_length?q->menu_search:"Search apps...",q->menu_search_length?foreground(q):secondary(q),0);
    int matches=0;
    for(int slot=0;slot<Q_APPS;slot++){
        int app=menu_app_at(q,slot);
        if(app<0)break;
        matches++;
        int bx=x+26+(slot%4)*116,by=y+112+(slot/4)*103;
        int lift=q->animations_enabled?q->menu_lift[app]*6/256:0;
        int selected=q->menu_hover==app||q->menu_lift[app]>0;
        surface(q,bx,by-lift,107,94,17,selected?colors(q)->selected:inner_material(q),selected?(q->theme?190:173):(q->theme?168:128));
        if(selected)thin_rule(q,bx+14,by+88-lift,79,q->theme?74:112);
        app_icon(q,bx+53,by+35-lift,app,43,q->theme?0xecfaff:0xffffff);
        int width=text_width(names[app],1);
        text(q,bx+(107-width)/2,by+79-lift,names[app],foreground(q),0);
    }
    if(!matches)text(q,x+42,y+166,"No matching applications",secondary(q),1);
}
static void draw_notifications(QDesktop *q){
    if(!q->notifications)return;
    int x=q->width-361,y=71;
    pane(q,x,y,334,209,22,material(q),q->theme?209:153);
    text(q,x+25,y+43,"Notifications",foreground(q),1);
    thin_rule(q,x+25,y+61,285,55);
    text(q,x+27,y+108,"All caught up",foreground(q),1);
    text(q,x+27,y+135,"Updates will appear here.",secondary(q),0);
}
static void draw_quick(QDesktop *q){
    if(!q->quick)return;
    int x=q->width-363,y=72;
    pane(q,x,y,336,293,22,material(q),q->theme?209:159);
    text(q,x+24,y+40,"Control center",foreground(q),1);
    thin_rule(q,x+24,y+58,289,54);
    surface(q,x+19,y+80,298,72,16,inner_material(q),q->theme?103:119);
    text(q,x+37,y+110,"Appearance",foreground(q),1);
    text(q,x+37,y+132,q->theme?"Dark":"Light",secondary(q),0);
    surface(q,x+19,y+164,298,68,16,inner_material(q),q->theme?103:119);
    text(q,x+37,y+191,"Accent",foreground(q),1);
    for(int i=0;i<4;i++)circular(q,x+178+i*32,y+194,10,(uint32_t[]){0x5dcfe3,0x9e9efa,0x77d5af,0xf4ae85}[i],255);
    sprite(q,x+230,y+263,QI_NO_WIFI,27);
    sprite(q,x+277,y+263,QI_MUTED_VOLUME,27);
    text(q,x+38,y+266,"Open settings",foreground(q),0);
}
static void focus_remaining(QDesktop *q){
    q->active=-1;
    for(int i=0;i<Q_APPS;i++)if(q->windows[i].open&&!q->windows[i].minimized&&q->windows[i].desktop==q->desktop&&
       (q->active<0||q->windows[i].z>q->windows[q->active].z))q->active=i;
}
static void close_window(QDesktop *q,int app){
    if(q->windows[app].maximized){
        QWindow *w=&q->windows[app];
        w->x=w->restore_x;w->y=w->restore_y;w->w=w->restore_w;w->h=w->restore_h;
    }
    if(q->windows[app].snap_zone){
        QWindow *w=&q->windows[app];
        w->x=w->restore_x;w->y=w->restore_y;w->w=w->restore_w;w->h=w->restore_h;
    }
    q->windows[app].snap_zone=0;q->windows[app].open=0;q->windows[app].minimized=0;q->windows[app].maximized=0;
    q->windows[app].tween_kind=0;
    q->dock_pulse[app]=q->animations_enabled?256:0;
    begin_scene_tween(q,4);
    focus_remaining(q);q->dirty=1;q->hover_only=0;q->drag_cache_valid=0;
}
static void minimize_window(QDesktop *q,int app){
    q->windows[app].minimized=1;
    q->windows[app].tween_kind=0;
    q->dock_pulse[app]=q->animations_enabled?256:0;
    begin_scene_tween(q,3);
    focus_remaining(q);q->dirty=1;q->hover_only=0;q->drag_cache_valid=0;
}
static void maximize_window(QDesktop *q,int app){
    QWindow *w=&q->windows[app];
    int from_x=w->x,from_y=w->y,from_w=w->w,from_h=w->h;
    if(!w->maximized){
        if(!w->snap_zone){w->restore_x=w->x;w->restore_y=w->y;w->restore_w=w->w;w->restore_h=w->h;}
        w->snap_zone=0;
        w->x=0;w->y=0;w->w=q->width;w->h=q->height;w->maximized=1;
    }else{
        w->x=w->restore_x;w->y=w->restore_y;w->w=w->restore_w;w->h=w->restore_h;w->maximized=0;
    }
    start_tween(q,app,2,from_x,from_y,from_w,from_h);
    begin_scene_tween(q,2);
    q->dirty=1;q->hover_only=0;q->drag_cache_valid=0;
}


void q_snap_window(QDesktop *q,int app,int zone){
    if(!q||app<0||app>=Q_APPS||!q->windows[app].open||zone<0||zone>6)return;
    QWindow *w=&q->windows[app];
    if(!zone){
        if(!w->snap_zone&&!w->maximized)return;
        w->x=w->restore_x;w->y=w->restore_y;w->w=w->restore_w;w->h=w->restore_h;
        w->snap_zone=0;w->maximized=0;
    }else{
        if(!w->snap_zone&&!w->maximized){
            w->restore_x=w->x;w->restore_y=w->y;
            w->restore_w=w->w;w->restore_h=w->h;
        }
        int left=8,top=8,width=q->width-16,height=q->height-105;
        if(width<400)width=q->width;
        if(height<240)height=q->height-24;
        int half=width/2,mid=height/2;
        w->x=left+((zone==2||zone==4||zone==6)?half:0);
        w->y=top+((zone==5||zone==6)?mid:0);
        w->w=(zone==1||zone==3||zone==5)?half:width-half;
        w->h=zone<=2?height:(zone<=4?mid:height-mid);
        w->maximized=0;w->snap_zone=zone;
    }
    w->tween_kind=0;
    q->snap_menu_app=-1;q->drag_cache_valid=0;q->glass_cached_app=-1;
    q->dock_cache_valid=0;q->hover_only=0;q->dirty=1;
}
void q_switch_desktop(QDesktop *q,int desktop){
    if(!q||desktop<0||desktop>2)return;
    q->desktop=desktop;q->desktop_view=0;q->snap_menu_app=-1;
    q->scene_tween_active=0;q->scene_tween_pending=0;
    q->drag=-1;q->pressed=0;q->drag_cache_valid=0;
    q->glass_cached_app=-1;q->dock_cache_valid=0;
    focus_remaining(q);
    q->hover_only=0;q->dirty=1;
}
void q_move_to_desktop(QDesktop *q,int app,int desktop){
    if(!q||desktop<0||desktop>2||app<0||app>=Q_APPS||!q->windows[app].open)return;
    q->windows[app].desktop=desktop;
    if(q->active==app)focus_remaining(q);
    q->scene_tween_active=0;q->scene_tween_pending=0;
    q->desktop_view=0;q->drag_cache_valid=0;q->dock_cache_valid=0;q->glass_cached_app=-1;
    q->hover_only=0;q->dirty=1;
}
static void cycle_window(QDesktop *q){
    int best=-1,best_z=-1;
    for(int i=0;i<Q_APPS;i++)if(i!=q->active&&q->windows[i].open&&!q->windows[i].minimized&&q->windows[i].desktop==q->desktop){
        if(q->windows[i].z>best_z){best=i;best_z=q->windows[i].z;}
    }
    if(best>=0)q_open(q,best);
}

void q_attach_kernel(QDesktop *q,QNtKernel *kernel,int (*terminate_process)(QNtKernel *,uint32_t)){
    if(!q)return;
    q->kernel=kernel;
    q->terminate_process=terminate_process;
}
static int task_index(QDesktop *q,int row){
    if(row<0)return -1;
    if(q->task_tab==0){
        for(int i=0;i<Q_APPS;i++)if(q->windows[i].open){
            if(row--==0)return i;
        }
    }else if(q->task_tab==1&&q->kernel){
        for(int i=0;i<QNT_PROCESSES;i++)if(q->kernel->processes[i].alive){
            if(row--==0)return i;
        }
    }
    return -1;
}
static int task_count(QDesktop *q,int tab){
    int count=0;
    if(tab==0){for(int i=0;i<Q_APPS;i++)if(q->windows[i].open)count++;}
    else if(tab==1&&q->kernel){for(int i=0;i<QNT_PROCESSES;i++)if(q->kernel->processes[i].alive)count++;}
    return count;
}
int q_task_end_selected(QDesktop *q){
    if(!q||q->task_selected<0||q->task_tab>1)return 0;
    int index=q->task_selected;
    if(q->task_tab==0){
        if(index>=Q_APPS||!q->windows[index].open)return 0;
        close_window(q,index);
    }else{
        if(!q->kernel||!q->terminate_process||index>=QNT_PROCESSES||!q->kernel->processes[index].alive)return 0;
        uint32_t pid=q->kernel->processes[index].pid;
        if(q->terminate_process(q->kernel,pid)!=QNT_OK)return 0;
    }
    q->task_selected=-1;q->dirty=1;
    return 1;
}
static void task_number(char *out,uint64_t n){
    char reversed[24];int length=0,index=0;
    do{reversed[length++]=(char)('0'+n%10u);n/=10u;}while(n&&length<24);
    while(length)out[index++]=reversed[--length];
    out[index]=0;
}
static void draw_tasks(QDesktop *q,QWindow *w,int x,int y,int cw,uint32_t ink,uint32_t muted){
    int count=task_count(q,q->task_tab);
    const char *tabs[]={"Applications","Processes","Performance"};
    for(int i=0;i<3;i++){
        int left=x+i*145;
        if(left+136>x+cw)break;
        surface(q,left,y+17,136,39,10,q->task_tab==i?colors(q)->selected:inner_material(q),q->theme?191:145);
        string_limit(q,left+12,y+43,tabs[i],ink,0,123,1);
    }
    if(q->task_tab==2){
        int apps=task_count(q,0),processes=task_count(q,1),threads=0;
        uint64_t mapped=0;
        if(q->kernel){
            for(int i=0;i<QNT_THREADS;i++)if(q->kernel->threads[i].state)threads++;
            for(int i=0;i<QNT_PROCESSES;i++)if(q->kernel->processes[i].alive)
                for(uint32_t j=0;j<q->kernel->processes[i].region_count;j++)
                    mapped+=q->kernel->processes[i].regions[j].length;
        }
        char value[32];
        surface(q,x,y+79,cw,w->h-188,15,inner_material(q),q->theme?128:154);
        const char *labels[]={"Open apps","Processes","Threads","Mapped virtual MiB","Uptime seconds","Scheduler ticks","Context switches","System calls"};
        uint64_t numbers[]={ (uint64_t)apps,(uint64_t)processes,(uint64_t)threads,mapped/(1024u*1024u),
            q->uptime_us/1000000u,q->kernel?q->kernel->clock_ticks:0,
            q->kernel?q->kernel->switches:0,q->kernel?q->kernel->syscalls:0 };
        for(int i=0;i<8;i++){
            int yy=y+115+i*39;
            if(yy>y+w->h-120)break;
            text(q,x+20,yy,labels[i],ink,0);
            task_number(value,numbers[i]);
            text(q,x+cw-140,yy,value,accent(q),1);
        }
        return;
    }
    int max_rows=clamp((w->h-245)/42,3,10);
    if(q->task_scroll>=count)q->task_scroll=count>0?(count-1)/max_rows*max_rows:0;
    surface(q,x,y+75,cw,w->h-190,14,inner_material(q),q->theme?112:155);
    text(q,x+19,y+99,q->task_tab==0?"Application":"Native process",muted,0);
    text(q,x+cw-145,y+99,q->task_tab==0?"State":"PID / Memory",muted,0);
    for(int row=0;row<max_rows;row++){
        int index=task_index(q,q->task_scroll+row);
        if(index<0)break;
        int top=y+109+row*42;
        if(top+40>y+w->h-119)break;
        surface(q,x+9,top,cw-18,39,8,index==q->task_selected?colors(q)->selected:inner_material(q),q->theme?165:118);
        if(q->task_tab==0){
            app_icon(q,x+35,top+20,index,27,ink);
            string_limit(q,x+60,top+26,names[index],ink,0,cw-225,1);
            text(q,x+cw-144,top+26,q->windows[index].minimized?"Minimized":index==q->active?"Active":"Running",muted,0);
        }else{
            QNtProcess *p=&q->kernel->processes[index];
            char number[32],info[60];int len=0;
            const char *title="Native process";
            string_limit(q,x+25,top+26,title,ink,0,cw-230,1);
            task_number(number,p->pid);
            for(int i=0;number[i];i++)info[len++]=number[i];
            info[len++]=' ';info[len++]='/';info[len++]=' ';
            uint64_t vm=0;for(uint32_t i=0;i<p->region_count;i++)vm+=p->regions[i].length;
            task_number(number,vm/1024u);
            for(int i=0;number[i];i++)info[len++]=number[i];
            info[len++]='K';info[len]=0;
            text(q,x+cw-145,top+26,info,muted,0);
        }
    }
    int bottom=y+w->h-103;
    surface(q,x,bottom,124,39,11,inner_material(q),q->theme?151:143);
    text(q,x+26,bottom+26,"Previous",ink,0);
    surface(q,x+132,bottom,98,39,11,inner_material(q),q->theme?151:143);
    text(q,x+151,bottom+26,"Next",ink,0);
    surface(q,x+cw-151,bottom,151,39,11,q->task_selected>=0?colors(q)->selected:inner_material(q),q->theme?185:129);
    text(q,x+cw-127,bottom+26,"End task",ink,1);
}
static void tasks_click(QDesktop *q,QWindow *w,int x,int y){
    int left=w->x+32,top=w->y+60,cw=w->w-64;
    for(int i=0;i<3;i++)if(hit(x,y,left+i*145,top+17,136,39)){
        q->task_tab=i;q->task_selected=-1;q->task_scroll=0;q->dirty=1;return;
    }
    if(q->task_tab==2)return;
    int max_rows=clamp((w->h-245)/42,3,10);
    for(int row=0;row<max_rows;row++)if(hit(x,y,left+9,top+109+row*42,cw-18,39)){
        q->task_selected=task_index(q,q->task_scroll+row);q->dirty=1;return;
    }
    int bottom=top+w->h-103,count=task_count(q,q->task_tab);
    if(hit(x,y,left,bottom,124,39)&&q->task_scroll>0){
        q->task_scroll-=max_rows;if(q->task_scroll<0)q->task_scroll=0;q->task_selected=-1;q->dirty=1;return;
    }
    if(hit(x,y,left+132,bottom,98,39)&&q->task_scroll+max_rows<count){
        q->task_scroll+=max_rows;q->task_selected=-1;q->dirty=1;return;
    }
    if(hit(x,y,left+cw-151,bottom,151,39))q_task_end_selected(q);
}

static void files_rebuild(QDesktop *q);
static void files_history(QDesktop *q);
static void files_go(QDesktop *q,int volume,const char *path,int remember);
static int files_equal(const char *a,const char *b);
static void files_message(QDesktop *q,const char *message){
    int i=0;for(;message[i]&&i<79;i++)q->file_status[i]=message[i];q->file_status[i]=0;
    q->dirty=1;
}
static void files_refresh(QDesktop *q){
    q->file_count=0;q->file_selected=-1;q->file_scroll=0;q->file_confirm=0;q->file_operation=0;q->file_checked_count=0;
    for(int i=0;i<QFS_ENTRIES;i++)q->file_checked[i]=0;
    q->file_click_index=-1;q->file_click_elapsed_us=0;
    if(!q->storage||q->storage->count<1){files_message(q,"No mounted volumes");return;}
    int n;
    if(q->file_location==2){
        n=qtrash_list(q->storage,q->trash_entries,QTRASH_LIMIT);
        if(n<0){files_message(q,"Could not open Recycle Bin");return;}
        for(int i=0;i<n;i++){
            QFsEntry *e=&q->files[i];
            for(size_t j=0;j<sizeof(*e);j++)((uint8_t *)e)[j]=0;
            int k=0;for(;q->trash_entries[i].name[k]&&k<71;k++)e->name[k]=q->trash_entries[i].name[k];
            e->name[k]=0;e->directory=q->trash_entries[i].directory;e->size=q->trash_entries[i].size;
        }
    }else{
        n=qfs_list(q->storage,q->file_volume,q->file_path,q->files,QFS_ENTRIES);
        if(n<0){files_message(q,"Cannot open this folder");return;}
    }
    q->file_count=n;files_rebuild(q);files_message(q,"");
}
void q_attach_storage(QDesktop *q,QStorage *storage,uint8_t *buffer,size_t capacity){
    if(!q)return;
    int first_attach=q->storage==0;
    q->storage=storage;q->media_buffer=buffer;q->media_capacity=capacity;
    if(storage){
        qprofiles_load(storage);
        if(first_attach&&qprofiles_has_pin(storage,storage->profile)){
            q->locked=1;q->pin_input_len=0;q->pin_input[0]=0;
        }
        q->profile_count=qprofiles_list(storage,q->profiles,QPROFILE_LIMIT);
        if(q->profile_count<0)q->profile_count=0;
        int current_found=0;
        for(int j=0;j<q->profile_count;j++)
            if(files_equal(q->profiles[j].name,storage->profile))current_found=1;
        if(!current_found&&q->profile_count<QPROFILE_LIMIT){
            QProfile *extra=&q->profiles[q->profile_count++];
            int i=0;for(;storage->profile[i]&&i<24;i++)extra->name[i]=storage->profile[i];
            extra->name[i]=0;extra->has_pin=0;
        }
        q->profile_target=0;
        for(int j=0;j<q->profile_count;j++)
            if(files_equal(q->profiles[j].name,storage->profile))q->profile_target=j;
        uint8_t settings[10];size_t length=0;
        if(qfs_read(storage,storage->home_index,preferences_path(q),settings,sizeof(settings),&length)==0&&
           length==sizeof(settings)&&settings[0]=='Q'&&settings[1]=='Z'&&settings[2]=='P'&&settings[3]=='R'&&
           settings[4]==1&&settings[5]<=1&&settings[6]<=7&&settings[7]<=1&&settings[8]<=3){
            q->theme=settings[5];q->accent=settings[6];q->animations_enabled=settings[7];q->wallpaper_mode=settings[8];q->settings_loaded=1;
            if(q->wallpaper_mode==3){if(wallpaper_saved(q)!=0){q->wallpaper_mode=0;wallpaper(q);}}
            else wallpaper(q);
            q->dirty=1;q->glass_cached_app=-1;q->dock_cache_valid=0;
        }
    }
    q->terminal_volume=storage?storage->home_index:0;
    q->file_volume=storage?storage->home_index:0;
    q->file_path[0]='\\';q->file_path[1]=0;
    if(storage){
        char home[QFS_PATH];
        if(qfs_profile_path(storage,"",home,sizeof(home))==0){
            int i=0;for(;home[i];i++)q->file_path[i]=home[i];
            q->file_path[i]=0;
            for(i=0;home[i];i++)q->terminal_path[i]=home[i];
            q->terminal_path[i]=0;
        }
    }
    files_refresh(q);files_history(q);
}
static int file_suffix(const char *name,const char *suffix){
    int end=0,n=0;
    while(name[end])end++;
    while(suffix[n])n++;
    if(n>end)return 0;
    for(int i=0;i<n;i++){
        char a=name[end-n+i],b=suffix[i];
        if(a>='A'&&a<='Z')a=(char)(a+32);
        if(b>='A'&&b<='Z')b=(char)(b+32);
        if(a!=b)return 0;
    }
    return 1;
}
static const char *store_directory(QDesktop *q){
    static char path[QFS_PATH];
    if(!q->storage||q->storage->home_index==q->storage->ram_index)return 0;
    return qfs_profile_path(q->storage,"Applications",path,sizeof(path))==0?path:0;
}
static void store_message(QDesktop *q,const char *message){
    int i=0;
    for(;message[i]&&i<(int)sizeof(q->store_status)-1;i++)q->store_status[i]=message[i];
    q->store_status[i]=0;q->dirty=1;
}
static void store_refresh(QDesktop *q){
    q->store_installed_count=0;
    const char *directory=store_directory(q);
    if(!directory)return;
    int count=qfs_list(q->storage,q->storage->home_index,directory,q->store_installed,24);
    if(count<0)return;
    int out=0;
    for(int i=0;i<count;i++)if(!q->store_installed[i].directory&&file_suffix(q->store_installed[i].name,".qapp"))
        q->store_installed[out++]=q->store_installed[i];
    q->store_installed_count=out;
}
static void store_import(QDesktop *q,int volume,const char *path){
    if(!q->storage||!q->media_buffer||!q->media_capacity){files_message(q,"Package storage unavailable");return;}
    q->media.playing=0;q->media.kind=QM_NONE;
    size_t length=0;QPackage info={0};
    if(qfs_read(q->storage,volume,path,q->media_buffer,q->media_capacity,&length)!=0||
       qpkg_verify(q->media_buffer,length,&info)!=0){
        files_message(q,"Invalid, corrupt or oversized QApp package");return;
    }
    q->store_info=info;q->store_source_volume=volume;
    int i=0;for(;path[i]&&i<QFS_PATH-1;i++)q->store_source_path[i]=path[i];
    q->store_source_path[i]=0;q->store_pending=1;q->store_view=1;
    store_message(q,"Verified package ready to install");
    q_open(q,Q_STORE);
}
static void store_install(QDesktop *q){
    const char *directory=store_directory(q);
    if(!directory){store_message(q,"Attach persistent Home storage to install");return;}
    char filename[96],target[QFS_PATH];
    int i=0;
    for(;q->store_info.id[i]&&i<78;i++)filename[i]=q->store_info.id[i];
    filename[i++]='.';filename[i++]='q';filename[i++]='a';filename[i++]='p';filename[i++]='p';filename[i]=0;
    if(qfs_join(target,sizeof(target),directory,filename)!=0){store_message(q,"Invalid package identifier");return;}
    size_t length=0;QPackage verified={0};
    if(qfs_read(q->storage,q->store_source_volume,q->store_source_path,q->media_buffer,q->media_capacity,&length)!=0||
       qpkg_verify(q->media_buffer,length,&verified)!=0||!files_equal(verified.id,q->store_info.id)){
        store_message(q,"Source package changed or verification failed");return;
    }
    if(qfs_copy(q->storage,q->store_source_volume,q->store_source_path,q->storage->home_index,target)!=0){
        store_message(q,"Already installed or unable to write package");return;
    }
    QPackage installed={0};size_t installed_length=0;
    if(qfs_read(q->storage,q->storage->home_index,target,q->media_buffer,q->media_capacity,&installed_length)!=0||
       qpkg_verify(q->media_buffer,installed_length,&installed)!=0||!files_equal(installed.id,verified.id)){
        qfs_delete(q->storage,q->storage->home_index,target);
        store_message(q,"Package installation verification failed");return;
    }
    q->store_pending=0;q->store_confirm_remove=-1;store_refresh(q);
    store_message(q,"Package installed and verified");
}
static void store_remove(QDesktop *q,int index){
    if(index<0||index>=q->store_installed_count||!store_directory(q))return;
    if(q->store_confirm_remove!=index){q->store_confirm_remove=index;q->dirty=1;return;}
    char path[QFS_PATH];
    if(qfs_join(path,sizeof(path),store_directory(q),q->store_installed[index].name)!=0||
       qfs_delete(q->storage,q->storage->home_index,path)!=0){store_message(q,"Could not remove package");return;}
    q->store_confirm_remove=-1;store_refresh(q);store_message(q,"Package removed");
}

void q_attach_photo_output(QDesktop *q,uint8_t *output,size_t capacity){
    if(!q)return;
    q->photo_output=output;q->photo_output_capacity=capacity;
}
static void media_message(QDesktop *q,const char *text){
    int i=0;for(;text[i]&&i<95;i++)q->media_status[i]=text[i];
    q->media_status[i]=0;q->dirty=1;
}
static int media_supported(const char *name){
    return file_suffix(name,".bmp")||file_suffix(name,".png")||file_suffix(name,".jpg")||
        file_suffix(name,".jpeg")||file_suffix(name,".gif")||file_suffix(name,".tga")||
        file_suffix(name,".wav")||file_suffix(name,".mp3")||file_suffix(name,".avi");
}
static int media_load(QDesktop *q,int volume,const char *path,const char *name){
    if(!q->media_buffer||!q->storage)return -1;
    size_t bytes=0;QMedia opened;
    if(qfs_read(q->storage,volume,path,q->media_buffer,q->media_capacity,&bytes)!=0||
       qm_open(&opened,q->media_buffer,bytes)!=0){
        q->media.playing=0;q->media.kind=QM_NONE;return -1;
    }
    q->media=opened;q->photo_zoom=0;q->photo_rotation=0;q->photo_flipped=0;
    q->audio_display_us=0;q->media_volume=volume;
    int i=0;for(;path[i]&&i<QFS_PATH-1;i++)q->media_path[i]=path[i];
    q->media_path[i]=0;
    i=0;for(;name[i]&&i<(int)sizeof(q->media_name)-1;i++)q->media_name[i]=name[i];
    q->media_name[i]=0;
    q->media_status[0]=0;q->dirty=1;
    return 0;
}
static void media_album(QDesktop *q,int volume,const char *path,const char *selected,int kind){
    q->media_track_count=0;q->media_track_index=-1;q->media_track_volume=volume;
    size_t last=0;for(size_t i=0;path[i];i++)if(path[i]=='\\')last=i;
    if(!last)last=0;
    size_t k=last==0?1:last;
    for(size_t i=0;i<k;i++)q->media_track_directory[i]=path[i];
    q->media_track_directory[k]=0;
    if(q->media_track_directory[0]!='\\')return;
    static QFsEntry entries[QFS_ENTRIES];
    int count=qfs_list(q->storage,volume,q->media_track_directory,entries,QFS_ENTRIES);
    for(int i=0;i<count&&q->media_track_count<QFS_ENTRIES;i++){
        QFsEntry *e=&entries[i];
        if(e->directory||!media_supported(e->name))continue;
        int valid=kind==QM_IMAGE?
            (file_suffix(e->name,".bmp")||file_suffix(e->name,".png")||file_suffix(e->name,".jpg")||
             file_suffix(e->name,".jpeg")||file_suffix(e->name,".gif")||file_suffix(e->name,".tga")):
            (file_suffix(e->name,".wav")||file_suffix(e->name,".mp3")||file_suffix(e->name,".avi"));
        if(!valid)continue;
        if(files_equal(e->name,selected))q->media_track_index=q->media_track_count;
        q->media_tracks[q->media_track_count++]=*e;
    }
}
int q_media_next(QDesktop *q,int direction){
    if(!q||!q->storage||!q->media_track_count||!direction)return -1;
    int original=q->media_track_index;
    if(original<0)return -1;
    int start=original;
    for(int attempt=0;attempt<q->media_track_count;attempt++){
        int next=start+(direction>0?1:-1);
        if(next>=q->media_track_count){if(!q->media_repeat)return -1;next=0;}
        if(next<0){if(!q->media_repeat)return -1;next=q->media_track_count-1;}
        start=next;
        char path[QFS_PATH];
        if(qfs_join(path,sizeof(path),q->media_track_directory,q->media_tracks[start].name)!=0)continue;
        if(media_load(q,q->media_track_volume,path,q->media_tracks[start].name)!=0)continue;
        q->media_track_index=start;
        q->media.playing=q->media.kind!=QM_IMAGE;
        return 0;
    }
    media_message(q,"Could not open next item");
    return -1;
}
int q_photo_save_bmp(QDesktop *q){
    if(!q||!q->storage||q->media.kind!=QM_IMAGE||!q->photo_output)return -1;
    size_t size=0;
    if(qm_export_bmp(&q->media,q->photo_rotation,q->photo_flipped,q->photo_output,q->photo_output_capacity,&size)!=0){
        media_message(q,"Image exceeds export memory");return -1;
    }
    int volume=q->storage->home_index;
    char dir[QFS_PATH];
    if(qfs_profile_path(q->storage,"Pictures",dir,sizeof(dir))!=0)return -1;
    static QFsEntry entries[QFS_ENTRIES];
    int count=qfs_list(q->storage,volume,dir,entries,QFS_ENTRIES);
    if(count<0){
        if(qfs_mkdir(q->storage,volume,dir)!=0){media_message(q,"Pictures folder unavailable");return -1;}
        count=0;
    }
    for(int number=1;number<=999;number++){
        char filename[20]="Edited-000.bmp";
        filename[7]=(char)('0'+number/100);
        filename[8]=(char)('0'+number/10%10);
        filename[9]=(char)('0'+number%10);
        int taken=0;
        for(int i=0;i<count;i++)if(files_equal(entries[i].name,filename)){taken=1;break;}
        if(taken)continue;
        char path[QFS_PATH];
        if(qfs_join(path,sizeof(path),dir,filename)!=0)break;
        if(qfs_write(q->storage,volume,path,q->photo_output,size)==0){
            media_message(q,volume==q->storage->ram_index?"Saved BMP in Home RAM Pictures":"Saved BMP to Pictures");
            return 0;
        }
        media_message(q,"Could not save image");return -1;
    }
    media_message(q,"Pictures folder has no available filenames");
    return -1;
}

static void files_open(QDesktop *q,int index){
    if(index<0||index>=q->file_count)return;
    if(q->file_location==2){
        int restored=qtrash_restore(q->storage,q->trash_entries[index].id)==0;
        files_refresh(q);
        files_message(q,restored?"Restored to original location":"Could not restore to original location");
        return;
    }
    QFsEntry *entry=&q->files[index];
    char path[QFS_PATH];
    if(qfs_join(path,sizeof(path),q->file_path,entry->name)!=0){files_message(q,"Path too long");return;}
    q->file_selected=index;q->file_confirm=0;
    if(entry->directory){
        files_go(q,q->file_volume,path,1);return;
    }
    if(file_suffix(entry->name,".qapp")){store_import(q,q->file_volume,path);return;}
    if(file_suffix(entry->name,".txt")||file_suffix(entry->name,".log")||file_suffix(entry->name,".md")){
        size_t length=0;
        if(!q->storage||qfs_read(q->storage,q->file_volume,path,q->notes,sizeof(q->notes)-1,&length)!=0){
            files_message(q,"Cannot open text file");return;
        }
        q->note_length=(int)length;q->note_cursor=q->note_length;q->note_scroll=0;q->note_dirty=0;
        for(int i=0;i<q->note_length;i++)if(q->notes[i]==0)q->notes[i]=' ';
        q->notes[q->note_length]=0;
        int i=0;for(;path[i];i++)q->note_path[i]=path[i];q->note_path[i]=0;
        q->note_volume=q->file_volume;
        q_open(q,Q_NOTES);return;
    }
    if(file_suffix(entry->name,".bmp")||file_suffix(entry->name,".png")||
       file_suffix(entry->name,".jpg")||file_suffix(entry->name,".jpeg")||
       file_suffix(entry->name,".gif")||file_suffix(entry->name,".tga")||
       file_suffix(entry->name,".wav")||file_suffix(entry->name,".mp3")||
       file_suffix(entry->name,".avi")){
        if(media_load(q,q->file_volume,path,entry->name)!=0){
            files_message(q,"Unsupported media file or memory limit");return;
        }
        media_album(q,q->file_volume,path,entry->name,q->media.kind);
        q_open(q,q->media.kind==QM_IMAGE?Q_PHOTOS:Q_MEDIA);
        return;
    }
    files_message(q,"No associated viewer for this format");
}
static void files_select(QDesktop *q,int index){
    if(index<0||index>=q->file_count)return;
    if(q->file_click_index==index&&q->file_click_elapsed_us<420000u){
        q->file_click_index=-1;
        q->file_click_elapsed_us=0;
        files_open(q,index);
        return;
    }
    q->file_selected=index;
    for(int i=0;i<QFS_ENTRIES;i++)q->file_checked[i]=0;
    q->file_checked[index]=1;q->file_checked_count=1;
    q->file_confirm=0;
    q->file_click_index=index;
    q->file_click_elapsed_us=0;
    q->dirty=1;
}
static void files_parent(QDesktop *q){
    if(q->file_location==2){
        int volume=q->storage->home_index;
        char home[QFS_PATH];
        if(qfs_profile_path(q->storage,"",home,sizeof(home))==0)files_go(q,volume,home,1);
        return;
    }
    if(q->file_location)return;
    int end=0;while(q->file_path[end])end++;
    if(end<=1)return;
    while(end>1&&q->file_path[end-1]!='\\')end--;
    if(end>1)end--;
    q->file_path[end]=0;
    files_go(q,q->file_volume,q->file_path,1);
}
static int files_equal(const char *a,const char *b){
    for(;*a&&*b;a++,b++){
        char x=*a,y=*b;
        if(x>='A'&&x<='Z')x=(char)(x+32);
        if(y>='A'&&y<='Z')y=(char)(y+32);
        if(x!=y)return 0;
    }
    return *a==*b;
}
static void files_begin(QDesktop *q,int operation){
    if(operation==3&&(q->file_selected<0||q->file_selected>=q->file_count))return;
    q->file_operation=operation;
    q->file_input_length=0;
    if(operation==3){
        const char *name=q->files[q->file_selected].name;
        while(name[q->file_input_length]&&q->file_input_length<70){
            q->file_input[q->file_input_length]=name[q->file_input_length];
            q->file_input_length++;
        }
    }
    q->file_input[q->file_input_length]=0;
    q->file_click_index=-1;
    q->dirty=1;
}
static void files_finish(QDesktop *q){
    if(!q->storage||!q->file_input_length)return;
    for(int i=0;i<q->file_count;i++)if(files_equal(q->files[i].name,q->file_input)&&
        !(q->file_operation==3&&i==q->file_selected)){
        files_message(q,"A file with this name already exists");
        return;
    }
    char target[QFS_PATH],old[QFS_PATH];
    if(qfs_join(target,sizeof(target),q->file_path,q->file_input)!=0){
        files_message(q,"Invalid file name");
        return;
    }
    int ok=0;
    if(q->file_operation==1)ok=qfs_mkdir(q->storage,q->file_volume,target)==0;
    else if(q->file_operation==2)ok=qfs_write(q->storage,q->file_volume,target,"",0)==0;
    else if(q->file_operation==3&&q->file_selected>=0&&
        qfs_join(old,sizeof(old),q->file_path,q->files[q->file_selected].name)==0)
        ok=qfs_rename(q->storage,q->file_volume,old,target)==0;
    if(ok)files_refresh(q);
    else files_message(q,"Could not create or rename item");
}
static QFsEntry operation_entries[QFS_ENTRIES];
static void files_save_notes(QDesktop *q){
    if(!q->storage){files_message(q,"No storage available");return;}
    if(!q->note_path[0]||q->note_volume<0){
        int volume=q->storage->home_index;
        if(volume<0||volume>=q->storage->count){files_message(q,"No writable storage available");return;}
        char path[QFS_PATH],filename[32];
        int found=0;
        char directory[QFS_PATH];
        if(qfs_profile_path(q->storage,volume==q->storage->ram_index?"":"Documents",directory,sizeof(directory))!=0)return;
        int count=qfs_list(q->storage,volume,directory,operation_entries,QFS_ENTRIES);
        for(int i=1;i<1000;i++){
            int k=0;const char *prefix="Note ";
            for(int j=0;prefix[j];j++)filename[k++]=prefix[j];
            int number=i,place=k;
            do{filename[k++]=(char)('0'+number%10);number/=10;}while(number);
            for(int start=place,end=k-1;start<end;start++,end--){char c=filename[start];filename[start]=filename[end];filename[end]=c;}
            const char *ext=".txt";for(int j=0;ext[j];j++)filename[k++]=ext[j];filename[k]=0;
            int used=0;
            for(int j=0;j<count;j++)if(files_equal(operation_entries[j].name,filename)){used=1;break;}
            if(used)continue;
            if(qfs_join(path,sizeof(path),directory,filename)!=0)break;
            if(qfs_write(q->storage,volume,path,q->notes,(size_t)q->note_length)==0){
                int j=0;for(;path[j];j++)q->note_path[j]=path[j];q->note_path[j]=0;
                q->note_volume=volume;found=1;break;
            }
            break;
        }
        if(!found){files_message(q,"Could not create note");return;}
        q->note_dirty=0;files_message(q,volume==q->storage->ram_index?"Saved to Home RAM":"Saved to Documents");return;
    }
    if(qfs_write(q->storage,q->note_volume,q->note_path,q->notes,(size_t)q->note_length)!=0)
        files_message(q,"Could not save note");
    else{q->note_dirty=0;files_message(q,"Saved");}
}
static void notes_insert(QDesktop *q,char value){
    if(q->note_length>=(int)sizeof(q->notes)-1)return;
    for(int i=q->note_length;i>=q->note_cursor;i--)q->notes[i+1]=q->notes[i];
    q->notes[q->note_cursor++]=value;q->note_length++;q->note_dirty=1;q->dirty=1;
}
static void notes_delete(QDesktop *q,int forward){
    if((!forward&&q->note_cursor==0)||(forward&&q->note_cursor==q->note_length))return;
    if(!forward)q->note_cursor--;
    for(int i=q->note_cursor;i<q->note_length;i++)q->notes[i]=q->notes[i+1];
    q->note_length--;q->note_dirty=1;q->dirty=1;
}
static void notes_vertical(QDesktop *q,int direction){
    int start=q->note_cursor;
    while(start>0&&q->notes[start-1]!='\n')start--;
    int column=q->note_cursor-start;
    if(direction<0){
        if(!start)return;
        int previous=start-1,beg=previous;
        while(beg>0&&q->notes[beg-1]!='\n')beg--;
        q->note_cursor=beg+clamp(column,0,previous-beg);
    }else{
        int end=q->note_cursor;
        while(end<q->note_length&&q->notes[end]!='\n')end++;
        if(end==q->note_length)return;
        int next=end+1,stop=next;
        while(stop<q->note_length&&q->notes[stop]!='\n')stop++;
        q->note_cursor=next+clamp(column,0,stop-next);
    }
    q->dirty=1;
}
static void draw_notes(QDesktop *q,QWindow *w,int x,int y,int cw,uint32_t ink,uint32_t muted){
    surface(q,x,y+30,cw,w->h-158,17,inner_material(q),q->theme?74:138);
    int row=0,advance=0,cursor_row=0,cursor_x=0;
    int max_width=cw-47;
    for(int i=0;i<=q->note_length;i++){
        if(i==q->note_cursor){cursor_row=row;cursor_x=advance;}
        char c=i<q->note_length?q->notes[i]:0;
        int width=c&&c!='\n'?q_font_glyph(1,(unsigned char)c)->advance:0;
        if(c=='\n'||!c||advance+width>max_width){
            if(c=='\n'||advance+width>max_width){row++;advance=0;}
            if(!c)break;
            if(c=='\n')continue;
        }
        advance+=width;
    }
    int visible=clamp((w->h-205)/26,3,22);
    if(cursor_row<q->note_scroll)q->note_scroll=cursor_row;
    if(cursor_row>=q->note_scroll+visible)q->note_scroll=cursor_row-visible+1;
    int line=0,at=0,px=0;
    char display[256];
    if(!q->note_length)text(q,x+23,y+73,"Write something...",muted,1);
    for(int i=0;i<=q->note_length;i++){
        char c=i<q->note_length?q->notes[i]:0;
        int width=c&&c!='\n'?q_font_glyph(1,(unsigned char)c)->advance:0;
        if(c=='\n'||!c||px+width>max_width||at==255){
            if(line>=q->note_scroll&&line<q->note_scroll+visible&&at){
                display[at]=0;
                string_limit(q,x+23,y+73+(line-q->note_scroll)*26,display,ink,1,max_width,1);
            }
            at=0;px=0;
            if(!c)break;
            line++;
            if(c=='\n')continue;
        }
        display[at++]=c;px+=width;
    }
    if(cursor_row>=q->note_scroll&&cursor_row<q->note_scroll+visible){
        int cx=x+23+cursor_x,cy=y+54+(cursor_row-q->note_scroll)*26;
        rectangle(q,cx,cy,2,23,accent(q));
    }
    surface(q,x+cw-200,y+2,94,28,9,inner_material(q),q->theme?172:157);
    text(q,x+cw-178,y+22,"New",ink,0);
    surface(q,x+cw-98,y+2,96,28,9,inner_material(q),q->theme?172:157);
    text(q,x+cw-74,y+22,"Save",ink,0);
    if(q->file_status[0])string_limit(q,x+8,y+w->h-112,q->file_status,muted,0,cw-16,1);
    else text(q,x+8,y+w->h-112,q->note_dirty?"Unsaved changes":"",muted,0);
}
void q_attach_network(QDesktop *q,QNetwork *network){
    if(q){q->network=network;q->dirty=1;}
}
static void terminal_log(QDesktop *q,const char *message){
    for(int i=0;i<6;i++)for(int j=0;j<96;j++)q->terminal_lines[i][j]=q->terminal_lines[i+1][j];
    int n=0;for(;message&&message[n]&&n<95;n++)q->terminal_lines[6][n]=message[n];
    q->terminal_lines[6][n]=0;
    if(q->terminal_count<7)q->terminal_count++;
    q->dirty=1;
}
static void boot_diagnose(QDesktop *q){
    if(!q||!q->storage){terminal_log(q,"Storage unavailable");return;}
    QStorage *s=q->storage;
    terminal_log(q,"Quartzie boot diagnostics");
    int target=-1;QFsEntry probe[1];
    for(int i=0;i<s->ram_index;i++)if(qfs_list(s,i,"\\EFI\\BOOT",probe,1)>=0){target=i;break;}
    if(target<0)terminal_log(q,"EFI system files: not found");
    else{
        uint8_t *buffer=q->photo_output?q->photo_output:q->media_buffer;
        size_t capacity=q->photo_output?q->photo_output_capacity:q->media_capacity;
        if(buffer&&qupd_health(s,target,buffer,capacity)==0)terminal_log(q,"EFI image: valid structure and available digest");
        else terminal_log(q,"EFI image: FAILED integrity check");
    }
    terminal_log(q,qupd_target(s,1)>=0?"Previous firmware: recoverable":"Previous firmware: no usable backup");
    terminal_log(q,s->home_index==s->ram_index?"Home: temporary memory":"Home: persistent disk");
    char home[QFS_PATH];
    if(qfs_profile_path(s,"",home,sizeof(home))==0&&
       qfs_list(s,s->home_index,home,probe,1)>=0)terminal_log(q,"Active profile folder: accessible");
    else terminal_log(q,"Active profile folder: missing");
    terminal_log(q,q->network&&q->network->count?"Network hardware: detected":"Network hardware: not detected");
    terminal_log(q,"Desktop renderer: running");
    q_open(q,Q_TERMINAL);
}
static void terminal_run(QDesktop *q){
    char *command=q->command;
    while(*command==' ')command++;
    char *argument=command;
    while(*argument&&*argument!=' ')argument++;
    if(*argument){*argument++=0;while(*argument==' ')argument++;}
    if(!*command)return;
    if(files_equal(command,"clear")){q->terminal_count=0;for(int i=0;i<7;i++)q->terminal_lines[i][0]=0;return;}
    if(files_equal(command,"help")){terminal_log(q,"dir cd pwd drives drive mkdir touch del type");terminal_log(q,"open tasks netinfo diagnose clear help echo");return;}
    if(files_equal(command,"diagnose")){boot_diagnose(q);return;}
    if(files_equal(command,"netinfo")){
        if(!q->network||!q->network->count){terminal_log(q,"No network adapter detected");return;}
        for(int i=0;i<q->network->count;i++){
            QNetAdapter *net=&q->network->adapters[i];
            terminal_log(q,net->mac);
            terminal_log(q,net->link_known?(net->link_present?"Link connected":"Link disconnected"):"Link state unknown");
        }
        return;
    }
    if(files_equal(command,"pwd")){terminal_log(q,q->terminal_path);return;}
    if(files_equal(command,"echo")){terminal_log(q,argument);return;}
    if(files_equal(command,"open")){
        const char *apps[Q_APPS]={"files","notes","terminal","calculator","settings","about","store","photos","media","tasks"};
        for(int i=0;i<Q_APPS;i++)if(files_equal(apps[i],argument)){q_open(q,i);return;}
        terminal_log(q,"Unknown application");return;
    }
    if(!q->storage){terminal_log(q,"No storage available");return;}
    if(files_equal(command,"drives")){
        for(int i=0;i<q->storage->count;i++)terminal_log(q,q->storage->volumes[i].label);
        return;
    }
    if(files_equal(command,"drive")){
        if(argument[0]>='0'&&argument[0]<='9'&&!argument[1]){
            int target=argument[0]-'0';
            if(target<q->storage->count){
                q->terminal_volume=target;q->terminal_path[0]='\\';q->terminal_path[1]=0;
                terminal_log(q,q->storage->volumes[target].label);return;
            }
        }
        terminal_log(q,"Invalid drive index");return;
    }
    if(files_equal(command,"dir")||files_equal(command,"ls")){
        int n=qfs_list(q->storage,q->terminal_volume,q->terminal_path,operation_entries,QFS_ENTRIES);
        if(n<0){terminal_log(q,"Cannot read folder");return;}
        if(!n){terminal_log(q,"Folder is empty");return;}
        for(int i=0;i<n;i++){char label[96];int at=0;
            if(operation_entries[i].directory){const char *prefix="<DIR> ";for(int j=0;prefix[j];j++)label[at++]=prefix[j];}
            for(int j=0;operation_entries[i].name[j]&&at<95;j++)label[at++]=operation_entries[i].name[j];
            label[at]=0;terminal_log(q,label);
        }
        return;
    }
    if(files_equal(command,"cd")){
        if(files_equal(argument,"..")){
            int end=0;while(q->terminal_path[end])end++;
            while(end>1&&q->terminal_path[end-1]!='\\')end--;
            if(end>1)end--;
            q->terminal_path[end]=0;terminal_log(q,q->terminal_path);return;
        }
        if(!argument[0]){terminal_log(q,q->terminal_path);return;}
        char next[QFS_PATH];QFsEntry entries[1];
        if(qfs_join(next,sizeof(next),q->terminal_path,argument)!=0||
            qfs_list(q->storage,q->terminal_volume,next,entries,1)<0){terminal_log(q,"Directory not found");return;}
        int i=0;for(;next[i];i++)q->terminal_path[i]=next[i];q->terminal_path[i]=0;
        terminal_log(q,q->terminal_path);return;
    }
    char path[QFS_PATH];
    if(!argument[0]||qfs_join(path,sizeof(path),q->terminal_path,argument)!=0){
        terminal_log(q,"Invalid path or missing name");return;
    }
    if(files_equal(command,"mkdir")){terminal_log(q,qfs_mkdir(q->storage,q->terminal_volume,path)==0?"Folder created":"Cannot create folder");return;}
    if(files_equal(command,"touch")){terminal_log(q,qfs_write(q->storage,q->terminal_volume,path,"",0)==0?"File created":"Cannot create file");return;}
    if(files_equal(command,"del")||files_equal(command,"rm")){terminal_log(q,qfs_delete(q->storage,q->terminal_volume,path)==0?"Deleted":"Cannot delete item");return;}
    if(files_equal(command,"type")||files_equal(command,"cat")){
        char data[2048];size_t length=0;
        if(qfs_read(q->storage,q->terminal_volume,path,data,sizeof(data)-1,&length)!=0){terminal_log(q,"Cannot read file");return;}
        int index=0;
        while(index<(int)length){
            char line[96];int n=0;
            while(index<(int)length&&data[index]!='\n'&&n<95){if(data[index]>=32&&data[index]<127)line[n++]=data[index];index++;}
            if(index<(int)length&&data[index]=='\n')index++;
            line[n]=0;terminal_log(q,line);
        }
        return;
    }
    terminal_log(q,"Unknown command");
}


static int files_contains(const char *name,const char *query){
    if(!*query)return 1;
    for(int i=0;name[i];i++){
        int j=0;
        while(query[j]&&name[i+j]){
            char a=name[i+j],b=query[j];
            if(a>='A'&&a<='Z')a=(char)(a+32);
            if(b>='A'&&b<='Z')b=(char)(b+32);
            if(a!=b)break;
            j++;
        }
        if(!query[j])return 1;
    }
    return 0;
}
static const char *files_extension(const char *name){
    const char *last=0;for(const char *p=name;*p;p++)if(*p=='.')last=p+1;return last?last:"";
}
static int files_compare(QDesktop *q,int a,int b){
    QFsEntry *left=&q->files[a],*right=&q->files[b];
    if(left->directory!=right->directory)return left->directory?-1:1;
    int order=0;
    if(q->file_sort==1){
        const char *x=files_extension(left->name),*y=files_extension(right->name);
        while(*x&&*y){
            char c=*x++,d=*y++;
            if(c>='A'&&c<='Z')c=(char)(c+32);
            if(d>='A'&&d<='Z')d=(char)(d+32);
            if(c!=d){order=c<d?-1:1;break;}
        }
        if(!order&&*x!=*y)order=*x?1:-1;
    }else if(q->file_sort==2&&left->size!=right->size)
        order=left->size<right->size?-1:1;
    else if(q->file_sort==3&&left->modified!=right->modified)
        order=left->modified<right->modified?-1:1;
    if(!order){
        const char *x=left->name,*y=right->name;
        while(*x&&*y){
            char c=*x++,d=*y++;
            if(c>='A'&&c<='Z')c=(char)(c+32);
            if(d>='A'&&d<='Z')d=(char)(d+32);
            if(c!=d){order=c<d?-1:1;break;}
        }
        if(!order&&*x!=*y)order=*x?1:-1;
    }
    return q->file_desc?-order:order;
}
static int files_columns(QDesktop *q,QWindow *w){
    int area=w->w-46-(q->file_nav?188:0)-(q->file_details?188:0);
    if(q->file_view>=4)return 1;
    int pitch=q->file_view==0?164:q->file_view==1?134:q->file_view==2?108:85;
    return clamp(area/pitch,1,10);
}
static int files_item_height(QDesktop *q){
    static const int sizes[]={151,127,102,83,37,38,74,70};
    int size=sizes[q->file_view%8];
    return q->file_compact?size*4/5:size;
}
static int files_page(QDesktop *q,QWindow *w){
    return clamp((w->h-245)/files_item_height(q),1,30)*files_columns(q,w);
}
static void files_rebuild(QDesktop *q){
    q->file_visible_count=0;
    for(int i=0;i<q->file_count;i++){
        if(!q->file_show_hidden&&q->files[i].hidden)continue;
        if(!files_contains(q->files[i].name,q->file_search))continue;
        int j=q->file_visible_count++;
        while(j>0&&files_compare(q,i,q->file_order[j-1])<0){
            q->file_order[j]=q->file_order[j-1];j--;
        }
        q->file_order[j]=i;
    }
    q->file_scroll=0;q->file_click_index=-1;q->dirty=1;
}
static void files_history(QDesktop *q){
    if(q->file_history_index>=0&&q->file_history_index<q->file_history_count){
        int k=q->file_history_index;
        if(q->file_history_locations[k]==q->file_location&&q->file_history_volumes[k]==q->file_volume&&files_equal(q->file_history_paths[k],q->file_path))return;
    }
    q->file_history_count=q->file_history_index+1;
    if(q->file_history_count>=12){
        for(int i=1;i<12;i++){
            q->file_history_volumes[i-1]=q->file_history_volumes[i];
            q->file_history_locations[i-1]=q->file_history_locations[i];
            for(int j=0;j<QFS_PATH;j++)q->file_history_paths[i-1][j]=q->file_history_paths[i][j];
        }
        q->file_history_count=11;
    }
    int k=q->file_history_count++;
    q->file_history_index=k;q->file_history_volumes[k]=q->file_volume;
    q->file_history_locations[k]=q->file_location;
    for(int j=0;j<QFS_PATH;j++)q->file_history_paths[k][j]=q->file_path[j];
}
static void files_go(QDesktop *q,int volume,const char *path,int remember){
    if(!q->storage||volume<0||volume>=q->storage->count)return;
    char destination[QFS_PATH];int n=0;
    for(;path[n]&&n<QFS_PATH-1;n++)destination[n]=path[n];
    if(path[n]){files_message(q,"Path too long");return;}
    destination[n]=0;
    QFsEntry test[1];
    if(qfs_list(q->storage,volume,destination,test,1)<0){files_message(q,"Folder not found");return;}
    q->file_volume=volume;q->file_location=0;
    for(int i=0;i<=n;i++)q->file_path[i]=destination[i];
    q->file_search_length=0;q->file_search[0]=0;q->file_search_active=0;
    q->file_address_active=0;q->file_menu=0;
    files_refresh(q);if(remember)files_history(q);
}
static void files_this_pc(QDesktop *q,int remember){
    q->file_location=1;q->file_menu=0;q->file_submenu=0;
    q->file_search[0]=0;q->file_search_length=0;
    q->file_search_active=0;q->file_address_active=0;
    q->file_selected=-1;q->file_checked_count=0;q->file_click_index=-1;
    for(int i=0;i<QFS_ENTRIES;i++)q->file_checked[i]=0;
    if(remember)files_history(q);
    q->dirty=1;
}
static void files_back(QDesktop *q,int forward){
    int index=q->file_history_index+(forward?1:-1);
    if(index<0||index>=q->file_history_count)return;
    q->file_history_index=index;
    if(q->file_history_locations[index]==1)files_this_pc(q,0);
    else if(q->file_history_locations[index]==2){q->file_location=2;files_refresh(q);}
    else files_go(q,q->file_history_volumes[index],q->file_history_paths[index],0);
}
static const char *files_type(const QFsEntry *entry){
    if(entry->directory)return "File folder";
    const char *ext=files_extension(entry->name);
    if(files_equal(ext,"txt")||files_equal(ext,"md")||files_equal(ext,"log"))return "Text document";
    if(files_equal(ext,"png")||files_equal(ext,"jpg")||files_equal(ext,"jpeg")||files_equal(ext,"bmp")||files_equal(ext,"gif")||files_equal(ext,"tga"))return "Image";
    if(files_equal(ext,"wav")||files_equal(ext,"mp3"))return "Audio";
    if(files_equal(ext,"avi")||files_equal(ext,"mp4"))return "Video";
    if(files_equal(ext,"exe"))return "Application";
    if(files_equal(ext,"apk"))return "Android package";
    if(files_equal(ext,"qapp"))return "Quartzie package";
    return "File";
}
static int files_icon(const QFsEntry *entry){
    if(entry->directory)return QI_FOLDER;
    const char *kind=files_type(entry);
    if(files_equal(kind,"Image"))return QI_IMAGE;
    if(files_equal(kind,"Audio")||files_equal(kind,"Video"))return QI_MEDIA_PLAYER;
    if(files_equal(kind,"Android package")||files_equal(kind,"Quartzie package"))return QI_COMPRESSED;
    return QI_TEXT;
}
static void files_display_name(QDesktop *q,const QFsEntry *entry,char *out){
    int n=0;for(;entry->name[n]&&n<71;n++)out[n]=entry->name[n];out[n]=0;
    if(!q->file_extensions&&!entry->directory){
        int dot=-1;for(int i=1;i<n;i++)if(out[i]=='.')dot=i;
        if(dot>0)out[dot]=0;
    }
}
static void files_size(const QFsEntry *entry,char *out){
    if(entry->directory){out[0]=0;return;}
    uint64_t n=entry->size;const char *unit="B";
    if(n>=1073741824ull){n=(n+536870912ull)/1073741824ull;unit="GB";}
    else if(n>=1048576ull){n=(n+524288ull)/1048576ull;unit="MB";}
    else if(n>=1024ull){n=(n+512ull)/1024ull;unit="KB";}
    char reverse[24];int at=0;do{reverse[at++]=(char)('0'+n%10);n/=10;}while(n&&at<23);
    int j=0;while(at)out[j++]=reverse[--at];
    out[j++]=' ';for(int i=0;unit[i];i++)out[j++]=unit[i];out[j]=0;
}

static void files_button(QDesktop *q,int x,int y,int w,const char *label,int active){
    if(active)surface(q,x,y,w,33,7,colors(q)->selected,q->theme?180:156);
    string_limit(q,x+10,y+23,label,active?accent(q):foreground(q),0,w-15,1);
}
static void files_popup(QDesktop *q,int x,int y,int w,int n,const char *const *labels,int selected){
    surface(q,x,y,w,n*35+12,10,q->theme?0x272b32:0xe4eff9,247);
    for(int i=0;i<n;i++){
        if(i==selected)surface(q,x+5,y+6+i*35,w-10,33,6,colors(q)->selected,185);
        string_limit(q,x+12,y+30+i*35,labels[i],foreground(q),0,w-22,1);
    }
}
static void draw_files(QDesktop *q,QWindow *w,int x,int y,int cw,uint32_t ink,uint32_t muted){
    int left=x-17,top=y,full=cw+34;
    int side=q->file_nav?188:0;
    int main_x=left+side+9,main_w=full-side-16-(q->file_details?188:0);
    int sidebar_y=top+103,footer=w->y+w->h-31;
    surface(q,left,top,full,44,7,inner_material(q),q->theme?115:139);
    const char *navigation[]={"<",">","^","Refresh"};
    const int nav_x[]={0,40,80,120},nav_w[]={35,35,35,70};
    for(int i=0;i<4;i++)files_button(q,left+nav_x[i],top+6,nav_w[i],navigation[i],0);
    int search_w=clamp(full/4,144,225),address_x=left+195;
    int address_w=full-203-search_w-8;
    if(address_w<72)address_w=72;
    surface(q,address_x,top+6,address_w,34,6,q->theme?0x333d4b:0xf0f7ff,218);
    char location[QFS_PATH+38];int p=0;
    const char *volume=q->storage&&q->storage->count?q->storage->volumes[q->file_volume].label:"Drives";
    if(q->file_location==1)volume="This PC";
    if(q->file_location==2)volume="Recycle Bin";
    while(volume[p]&&p<25){location[p]=volume[p];p++;}
    if(!q->file_location){
        location[p++]=' ';location[p++]='>';
        for(int i=0;q->file_path[i]&&p<QFS_PATH+29;i++)location[p++]=q->file_path[i]=='\\'?'/':q->file_path[i];
    }
    location[p]=0;
    string_limit(q,address_x+11,top+29,q->file_address_active?q->file_address:location,ink,0,address_w-18,1);
    surface(q,left+full-search_w,top+6,search_w,34,6,q->theme?0x333d4b:0xf0f7ff,213);
    string_limit(q,left+full-search_w+11,top+29,q->file_search_length?q->file_search:"Search this folder",q->file_search_length?ink:muted,0,search_w-18,1);
    thin_rule(q,left,top+50,full,35);
    const char *tools[]={"New v","Cut","Copy","Paste","Rename","Delete","Sort v","View v","..."};
    const int tx[]={5,83,131,186,249,326,407,500,595},tw[]={74,45,52,55,75,76,85,88,45};
    if(q->file_location==2){
        files_button(q,left+5,top+57,100,"Restore",0);
        files_button(q,left+113,top+57,138,"Delete forever",0);
        files_button(q,left+259,top+57,126,"Empty Bin",0);
        files_button(q,left+393,top+57,95,"Refresh",0);
    }else for(int i=0;i<9;i++)if(tx[i]+tw[i]<=full)
        files_button(q,left+tx[i],top+57,tw[i],tools[i],q->file_menu==((i==0)?1:(i==6)?2:(i==7)?3:(i==8)?4:0));
    thin_rule(q,left,top+97,full,31);
    int canvas_y=top+109,canvas_h=footer-canvas_y;
    rectangle(q,left,canvas_y,full,canvas_h,q->theme?0x191b20:0xf9fcff);
    if(q->file_nav){
        surface(q,left,sidebar_y,side-6,footer-sidebar_y,8,inner_material(q),q->theme?117:143);
        const char *shortcuts[]={"Home","Gallery","Desktop","Documents","Pictures","Music","Videos","This PC","Recycle Bin"};
        const int icons[]={QI_FILE_EXPLORER,QI_IMAGE,QI_FOLDER,QI_FOLDER,QI_IMAGE,QI_MEDIA_PLAYER,QI_MEDIA_PLAYER,QI_FILE_EXPLORER,QI_FOLDER};
        for(int i=0;i<9;i++){
            int py=sidebar_y+12+i*36+(i>=2?12:0)+(i>=7?18:0);
            char home[QFS_PATH];
            if(q->storage)qfs_profile_path(q->storage,"",home,sizeof(home));
            else home[0]=0;
            int chosen=(i==0&&q->storage&&q->file_volume==q->storage->home_index&&files_equal(q->file_path,home))||
                (i==7&&q->file_location==1)||(i==8&&q->file_location==2)||
                (i>=2&&i<=6&&q->storage&&q->file_volume==q->storage->home_index&&files_contains(q->file_path,shortcuts[i]));
            if(chosen)surface(q,left+5,py-2,side-17,33,6,colors(q)->selected,137);
            sprite(q,left+22,py+13,icons[i],25);
            string_limit(q,left+41,py+20,shortcuts[i],ink,0,side-49,1);
        }
        thin_rule(q,left+13,sidebar_y+358,side-31,44);
        int count=q->storage?q->storage->count:0;
        for(int i=0;i<count&&i<5;i++){
            int py=sidebar_y+371+i*35;
            if(py+30>=footer)break;
            if(!q->file_location&&q->file_volume==i)surface(q,left+5,py-1,side-17,33,6,colors(q)->selected,131);
            sprite(q,left+22,py+14,QI_FILE_EXPLORER,25);
            string_limit(q,left+41,py+21,q->storage->volumes[i].label,ink,0,side-49,1);
        }
    }
    if(q->file_details){
        int dx=left+full-183;
        surface(q,dx,canvas_y+3,181,canvas_h-4,8,inner_material(q),q->theme?131:143);
        text(q,dx+13,canvas_y+33,"Details",ink,1);
        if(q->file_selected>=0&&q->file_selected<q->file_count){
            QFsEntry *entry=&q->files[q->file_selected];
            sprite(q,dx+90,canvas_y+105,files_icon(entry),64);
            char name[72],size[32];files_display_name(q,entry,name);files_size(entry,size);
            string_limit(q,dx+13,canvas_y+165,name,ink,0,155,2);
            string_limit(q,dx+13,canvas_y+205,files_type(entry),muted,0,155,2);
            if(!entry->directory)text(q,dx+13,canvas_y+248,size,muted,0);
        }else text(q,dx+13,canvas_y+78,"Select an item",muted,0);
    }
    if(q->file_location==1){
        text(q,main_x+12,canvas_y+32,"Devices and drives",ink,1);
        if(q->storage)for(int i=0;i<q->storage->count;i++){
            int dx=main_x+8+(i%2)*225,dy=canvas_y+53+(i/2)*90;
            if(dx+210>main_x+main_w)break;
            surface(q,dx,dy,210,76,9,inner_material(q),q->theme?131:133);
            sprite(q,dx+32,dy+37,QI_FILE_EXPLORER,43);
            string_limit(q,dx+62,dy+37,q->storage->volumes[i].label,ink,0,139,1);
            text(q,dx+62,dy+59,"Open drive",muted,0);
        }
    }else{
        int cols=files_columns(q,w),item_h=files_item_height(q);
        int page=files_page(q,w),pitch=cols==1?main_w-10:(main_w-14)/cols;
        int size=q->file_view==0?82:q->file_view==1?64:q->file_view==2?48:q->file_view==3?35:30;
        if(q->file_view==5){
            surface(q,main_x+4,canvas_y+2,main_w-5,33,5,inner_material(q),q->theme?123:148);
            text(q,main_x+42,canvas_y+25,"Name",muted,0);
            text(q,main_x+main_w/2,canvas_y+25,"Type",muted,0);
            text(q,main_x+main_w-100,canvas_y+25,"Size",muted,0);
            canvas_y+=37;
        }
        for(int slot=0;slot<page;slot++){
            int shown=q->file_scroll+slot;
            if(shown>=q->file_visible_count)break;
            int index=q->file_order[shown];QFsEntry *entry=&q->files[index];
            int ix=main_x+6+(slot%cols)*pitch,iy=canvas_y+(slot/cols)*item_h,iw=pitch-5;
            if(iy+item_h>footer-5)break;
            if(q->file_checked[index]||index==q->file_selected)
                surface(q,ix,iy,iw,item_h-4,6,colors(q)->selected,q->theme?154:143);
            if(q->file_checkboxes){
                rectangle(q,ix+6,iy+6,14,14,q->theme?0x8094aa:0x96aec3);
                rectangle(q,ix+8,iy+8,10,10,q->theme?0x191b20:0xf9fcff);
                if(q->file_checked[index])text(q,ix+6,iy+18,"x",accent(q),0);
            }
            char name[72],size_label[32];files_display_name(q,entry,name);files_size(entry,size_label);
            if(q->file_view>=4){
                sprite(q,ix+19,iy+item_h/2-2,files_icon(entry),30);
                string_limit(q,ix+42,iy+item_h/2+5,name,ink,0,
                    q->file_view==5?main_w/2-46:iw-48,1);
                if(q->file_view==5){
                    string_limit(q,main_x+main_w/2,iy+item_h/2+5,files_type(entry),muted,0,main_w/2-117,1);
                    text(q,main_x+main_w-98,iy+item_h/2+5,size_label,muted,0);
                }else if(q->file_view>=6)
                    string_limit(q,ix+42,iy+item_h/2+25,files_type(entry),muted,0,iw-50,1);
            }else{
                sprite(q,ix+iw/2,iy+size/2+6,files_icon(entry),size);
                int nx=text_width(name,0)<=iw-12?ix+(iw-text_width(name,0))/2:ix+6;
                string_limit(q,nx,iy+size+26,name,ink,0,iw-12,2);
            }
        }
        if(!q->file_visible_count)
            text(q,main_x+30,canvas_y+59,q->file_search_length?"No matching items":"This folder is empty",muted,1);
    }
    thin_rule(q,left,footer-8,full,42);
    char count[38],rev[16];int at=0,n=0;
    uint64_t value=q->file_location&&q->storage?(uint64_t)q->storage->count:(uint64_t)q->file_visible_count;
    do{rev[n++]=(char)('0'+value%10);value/=10;}while(value&&n<15);
    while(n)count[at++]=rev[--n];
    const char *suffix=" items";for(int i=0;suffix[i];i++)count[at++]=suffix[i];count[at]=0;
    text(q,left+9,footer+13,count,muted,0);
    if(q->file_checked_count)text(q,left+107,footer+13,"Selected",muted,0);
    if(q->file_status[0])string_limit(q,left+222,footer+13,q->file_status,muted,0,full-233,1);
    if(q->file_operation){
        int dw=clamp(full-96,310,530),dx=left+(full-dw)/2,dy=top+135;
        pane(q,dx,dy,dw,154,13,material(q),q->theme?239:213);
        text(q,dx+18,dy+35,q->file_operation==1?"New folder":q->file_operation==2?"New text document":"Rename",ink,1);
        surface(q,dx+17,dy+51,dw-34,43,7,inner_material(q),197);
        string_limit(q,dx+27,dy+79,q->file_input_length?q->file_input:"Enter a name",q->file_input_length?ink:muted,0,dw-52,1);
        text(q,dx+18,dy+128,"Enter  Save         Esc  Cancel",muted,0);
    }
    if(q->file_menu){
        const char *const new_items[]={"Folder","Text document"};
        const char *const sort_items[]={"Name","Type","Size","Date modified","Ascending","Descending"};
        const char *const view_items[]={"Extra large icons","Large icons","Medium icons","Small icons","List","Details","Tiles","Content","Show >"};
        const char *const show_items[]={"Navigation pane","Compact view","Item check boxes","File extensions","Hidden items"};
        const char *const more_items[]={"Select all","Select none","Invert selection","Properties","Refresh"};
        int mx=left+(q->file_menu==1?5:q->file_menu==2?407:q->file_menu==3?500:595),my=top+94;
        if(q->file_menu==1)files_popup(q,mx,my,190,2,new_items,-1);
        else if(q->file_menu==2)files_popup(q,mx,my,178,6,sort_items,q->file_sort);
        else if(q->file_menu==3){
            mx=clamp(mx,left,left+full-439);
            files_popup(q,mx,my,226,9,view_items,q->file_view);
            if(q->file_submenu){
                int sub_y=my+155;
                files_popup(q,mx+228,sub_y,202,5,show_items,-1);
                for(int i=0;i<5;i++){
                    int active=i==0?q->file_nav:i==1?q->file_compact:i==2?q->file_checkboxes:i==3?q->file_extensions:q->file_show_hidden;
                    if(active)text(q,mx+409,sub_y+30+i*35,"*",accent(q),0);
                }
            }
        }else if(q->file_menu==4)files_popup(q,clamp(mx,left,left+full-193),my,188,5,more_items,-1);
    }
}
static void draw_media_canvas(QDesktop *q,int x,int y,int w,int h){
    rectangle(q,x,y,w,h,0x101827);
    QMedia *m=&q->media;
    if((m->kind!=QM_IMAGE&&m->kind!=QM_VIDEO)||!m->width||!m->height)return;
    int source_w=m->width,source_h=m->height;
    if(q->render_app==Q_PHOTOS)qm_image_dimensions(m,q->photo_rotation,&source_w,&source_h);
    int dw=w-28,dh=h-22;
    if(q->render_app==Q_PHOTOS&&q->photo_zoom){
        dw=source_w*(q->photo_zoom==2?2:1);
        dh=source_h*(q->photo_zoom==2?2:1);
    }else if((int64_t)dw*source_h>(int64_t)dh*source_w)dw=(int)((int64_t)dh*source_w/source_h);
    else dh=(int)((int64_t)dw*source_h/source_w);
    if(dw<1||dh<1)return;
    int px=x+(w-dw)/2,py=y+(h-dh)/2;
    int startx=clamp(px>x?px:x,q->clip_left,q->clip_right);
    int endx=clamp(px+dw<x+w?px+dw:x+w,q->clip_left,q->clip_right);
    int starty=clamp(py>y?py:y,q->clip_top,q->clip_bottom);
    int endy=clamp(py+dh<y+h?py+dh:y+h,q->clip_top,q->clip_bottom);
    for(int sy=starty;sy<endy;sy++){
        int iy=(int)((int64_t)(sy-py)*source_h/dh);
        for(int sx=startx;sx<endx;sx++){
            int ix=(int)((int64_t)(sx-px)*source_w/dw);
            q->pixels[(size_t)sy*q->width+sx]=q->render_app==Q_PHOTOS?
                qm_image_pixel(m,q->photo_rotation,q->photo_flipped,ix,iy):qm_pixel(m,ix,iy);
        }
    }
}
static void draw_viewer(QDesktop *q,int app,QWindow *w,int x,int y,int cw,uint32_t ink,uint32_t muted){
    string_limit(q,x,y+26,q->media_name[0]?q->media_name:"No file selected",ink,1,cw-10,1);
    int height=w->h-223;
    if(height<110)height=110;
    if(app==Q_PHOTOS||q->media.kind==QM_VIDEO)draw_media_canvas(q,x,y+45,cw,height);
    else{
        surface(q,x,y+45,cw,height,18,inner_material(q),q->theme?180:160);
        int center=y+45+height/2;
        for(int i=0;i<84;i++){
            int pos=x+20+(cw-40)*i/84;
            uint64_t count=qm_audio_count(&q->media);
            uint64_t sample=count?(count*(uint64_t)i)/84:0;
            int level=count?(int)qm_audio_sample(&q->media,sample)-128:0;
            int bar=8+(level<0?-level:level)*height/350;
            rectangle(q,pos,center-bar,3,bar*2,accent(q));
        }
    }
    if(app==Q_MEDIA){
        surface(q,x,y+60+height,101,38,12,colors(q)->selected,q->theme?165:150);
        text(q,x+21,y+86+height,q->media.playing?"Pause":"Play",ink,1);
        uint64_t duration=qm_duration(&q->media);
        int track=cw-300;
        if(duration&&track>60){
            int sx=x+117,sy=y+70+height;
            rectangle(q,sx,sy,track,8,q->theme?0x395269:0xa3bdd0);
            int fill=(int)((q->media.elapsed_us>duration?duration:q->media.elapsed_us)*(uint64_t)track/duration);
            if(fill>0)rectangle(q,sx,sy,fill,8,accent(q));
            circular(q,sx+fill,sy+4,6,accent(q),255);
            uint64_t seconds=q->media.elapsed_us/1000000u;
            if(seconds>5999)seconds=5999;
            char time[8];
            time[0]=(char)('0'+seconds/600);time[1]=(char)('0'+seconds/60%10);
            time[2]=':';time[3]=(char)('0'+seconds/10%6);time[4]=(char)('0'+seconds%10);time[5]=0;
            text(q,sx,sy+31,time,muted,0);
        }
    }else{
        const char *labels[]={"Fit","100%","200%"};
        for(int i=0;i<3;i++){
            int bx=x+i*83;
            surface(q,bx,y+60+height,78,38,11,i==q->photo_zoom?colors(q)->selected:inner_material(q),q->theme?177:148);
            text(q,bx+14,y+86+height,labels[i],ink,0);
        }
    }
    if(app==Q_PHOTOS){
        const char *actions[]={"Rotate","Flip","Save BMP","Previous","Next","Set wallpaper"};
        const int offsets[]={0,88,161,277,365,458};
        for(int i=0;i<6;i++){
            int bx=x+offsets[i],width=i==5?143:i==2?107:i==1?69:84;
            if(bx+width>x+cw-2)break;
            surface(q,bx,y+104+height,width-2,36,10,inner_material(q),q->theme?158:148);
            text(q,bx+9,y+128+height,actions[i],ink,0);
        }
    }else{
        const char *actions[]={"Previous","Next","Repeat","Vol -","Vol +",q->media_muted?"Unmute":"Mute"};
        const int offsets[]={0,88,165,245,311,378};
        for(int i=0;i<6;i++){
            if(x+offsets[i]+67>x+cw)break;
            surface(q,x+offsets[i],y+104+height,76,36,10,i==2&&q->media_repeat?colors(q)->selected:inner_material(q),q->theme?173:151);
            text(q,x+offsets[i]+8,y+128+height,actions[i],ink,0);
        }
        char volume_label[20]="Volume: 000%";
        volume_label[8]=(char)('0'+q->media_volume_level/100);
        volume_label[9]=(char)('0'+q->media_volume_level/10%10);
        volume_label[10]=(char)('0'+q->media_volume_level%10);
        text(q,x+cw-125,y+128+height,volume_label,muted,0);
    }
    text(q,x+cw-166,y+86+height,"Browse files",muted,0);
    if(q->media_status[0])string_limit(q,x+5,y+w->h-70,q->media_status,muted,0,cw-10,1);
}

static const char *settings_pages[Q_SETTINGS_PAGES]={"Home","System","Personalization","Apps","Storage","About","Network","Accounts","Update & Recovery"};
static int settings_sidebar_width(int width){return clamp(width/4,175,220);}
static int settings_find(const char *haystack,const char *needle){
    if(!needle[0])return 1;
    for(int start=0;haystack[start];start++){
        int n=0;
        while(needle[n]&&haystack[start+n]){
            char a=haystack[start+n],b=needle[n];
            if(a>='A'&&a<='Z')a+=32;
            if(b>='A'&&b<='Z')b+=32;
            if(a!=b)break;
            n++;
        }
        if(!needle[n])return 1;
    }
    return 0;
}
static int settings_result(QDesktop *q,int page){
    if(settings_find(settings_pages[page],q->settings_search))return 1;
    const char *keywords[Q_SETTINGS_PAGES]={
        "welcome quick settings desktop",
        "display resolution motion animation performance",
        "theme light dark accent color wallpaper background",
        "installed applications open launch store photos media",
        "drives volumes home files disk",
        "device version hardware system information",
        "network ethernet adapter mac address link cable",
        "users profiles accounts pin password lock login",
        "install update recovery rollback integrity diagnostics repair"
    };
    return settings_find(keywords[page],q->settings_search);
}
static void settings_navigate(QDesktop *q,int page){
    if(page<0||page>=Q_SETTINGS_PAGES)return;
    q->settings_page=page;
    q->settings_search_length=0;
    q->settings_search[0]=0;
    q->dirty=1;
    q->hover_only=0;
}
static void settings_motion(QDesktop *q,int enabled){
    enabled=enabled!=0;
    if(q->animations_enabled==enabled)return;
    q->animations_enabled=enabled;
    q->scene_tween_pending=0;
    q->scene_tween_active=0;
    q->scene_elapsed_us=0;
    q->scene_paint_us=0;
    q->drag_cache_valid=0;
    q->dock_cache_valid=0;
    q->glass_cached_app=-1;
    q->hover_only=0;
    q->damage_valid=0;
    for(int i=0;i<Q_APPS;i++){
        q->windows[i].tween_kind=0;
        q->windows[i].tween_step=0;
        if(!enabled)q->dock_lift[i]=q->dock_pulse[i]=q->menu_lift[i]=0;
    }
    if(!enabled)q->start_lift=q->start_pulse=0;
    q->dirty=1;
    q_save_preferences(q);
}
static void settings_number(char *out,int value){
    char reverse[12];int n=0,j=0;
    if(value<0)value=0;
    do{reverse[n++]=(char)('0'+value%10);value/=10;}while(value&&n<11);
    while(n)out[j++]=reverse[--n];
    out[j]=0;
}
static void settings_resolution(QDesktop *q,char *out){
    char w[12],h[12];settings_number(w,q->width);settings_number(h,q->height);
    int i=0,j=0;
    while(w[j])out[i++]=w[j++];
    out[i++]=' ';out[i++]='x';out[i++]=' ';
    j=0;while(h[j])out[i++]=h[j++];
    out[i]=0;
}
static void settings_switch(QDesktop *q,int x,int y,int on){
    surface(q,x,y,54,29,15,on?accent(q):colors(q)->rule,on?221:142);
    circular(q,x+(on?39:15),y+14,10,on?0xffffff:foreground(q),255);
}
static void settings_card(QDesktop *q,int x,int y,int w,int h,const char *heading,const char *description,int arrow){
    if(w<20||h<20)return;
    surface(q,x,y,w,h,16,inner_material(q),q->theme?126:160);
    text(q,x+19,y+31,heading,foreground(q),1);
    string_limit(q,x+19,y+55,description,secondary(q),0,w-(arrow?68:36),1);
    if(arrow){
        int right=x+w-27,mid=y+h/2;
        segment(q,right-5,mid-6,right+1,mid,secondary(q));
        segment(q,right+1,mid,right-5,mid+6,secondary(q));
    }
}
static void settings_icon(QDesktop *q,int x,int y,int page){
    uint32_t ink=page==q->settings_page?accent(q):secondary(q);
    circular(q,x,y,13,ink,48);
    const char *letters[Q_SETTINGS_PAGES]={"H","S","P","A","D","i","N","U","R"};
    text(q,x-5,y+5,letters[page],ink,0);
}

static void profile_message(QDesktop *q,const char *value){
    int i=0;for(;value[i]&&i<95;i++)q->profile_status[i]=value[i];
    q->profile_status[i]=0;q->dirty=1;
}
static void update_message(QDesktop *q,const char *value){
    int i=0;for(;value[i]&&i<95;i++)q->update_status[i]=value[i];
    q->update_status[i]=0;q->dirty=1;
}
static void profile_refresh(QDesktop *q){
    if(!q->storage)return;
    q->profile_count=qprofiles_list(q->storage,q->profiles,QPROFILE_LIMIT);
    if(q->profile_count<0)q->profile_count=0;
    int current_found=0;
    for(int i=0;i<q->profile_count;i++)if(files_equal(q->profiles[i].name,q->storage->profile))current_found=1;
    if(!current_found&&q->profile_count<QPROFILE_LIMIT){
        QProfile *extra=&q->profiles[q->profile_count++];
        int i=0;for(;q->storage->profile[i]&&i<24;i++)extra->name[i]=q->storage->profile[i];
        extra->name[i]=0;extra->has_pin=0;
    }
    for(int i=0;i<q->profile_count;i++)
        if(files_equal(q->profiles[i].name,q->storage->profile))q->profile_target=i;
}
static void profile_activate(QDesktop *q,const char *name,const char *pin){
    if(!q->storage||qprofiles_switch(q->storage,name,pin)!=0){
        profile_message(q,"Incorrect PIN or account unavailable");return;
    }
    for(int i=0;i<Q_APPS;i++){q->windows[i].open=0;q->windows[i].minimized=0;q->windows[i].desktop=0;}
    q->active=-1;q->desktop=0;q->note_length=0;q->notes[0]=0;q->note_path[0]=0;
    q->note_dirty=0;q->media.playing=0;q->media.kind=QM_NONE;q->media_track_count=0;
    q->file_clip_count=0;q->terminal_count=0;q->locked=0;
    q->settings_loaded=0;q->theme=0;q->accent=0;q->wallpaper_mode=0;q->animations_enabled=1;
    wallpaper(q);
    q_attach_storage(q,q->storage,q->media_buffer,q->media_capacity);
    q->scene_tween_active=0;q->scene_tween_pending=0;q->hover_only=0;
    q->dock_cache_valid=0;q->drag_cache_valid=0;q->glass_cached_app=-1;
    q->pin_input_len=0;q->pin_input[0]=0;
    profile_message(q,"");
    q->dirty=1;
}
void q_lock(QDesktop *q){
    if(!q||!q->storage)return;
    profile_refresh(q);
    q->locked=1;q->pin_input_len=0;q->pin_input[0]=0;
    q->scene_tween_active=0;q->scene_tween_pending=0;
    close_popups(q);q->dirty=1;q->hover_only=0;
}
static void profile_unlock(QDesktop *q){
    if(q->profile_target<0||q->profile_target>=q->profile_count)return;
    profile_activate(q,q->profiles[q->profile_target].name,q->pin_input);
}
static void profile_edit_submit(QDesktop *q){
    if(q->profile_edit==1){
        if(qprofiles_create(q->storage,q->profile_input)==0){
            profile_refresh(q);
            profile_message(q,"Account created. Select it on the lock screen");
        }else profile_message(q,"Could not create account");
    }else if(q->profile_edit==2){
        if(qprofiles_set_pin(q->storage,q->profile_input)==0)profile_message(q,"PIN updated");
        else profile_message(q,"PIN must contain at least four characters");
        profile_refresh(q);
    }
    q->profile_edit=0;q->profile_input_len=0;q->profile_input[0]=0;q->dirty=1;
}
static void update_check(QDesktop *q){
    if(!q->storage||!q->media_buffer){update_message(q,"Storage unavailable");return;}
    int volume=-1;char path[QFS_PATH];
    q->update_found=0;
    if(qupd_find(q->storage,&volume,path,sizeof(path))!=0){
        update_message(q,"Place QUARTZIE.UPD on a FAT drive or in Downloads");return;
    }
    size_t length=0;QUpdate package={0};
    if(qfs_read(q->storage,volume,path,q->media_buffer,q->media_capacity,&length)!=0||
       qupd_verify(q->media_buffer,length,&package)!=0){
        update_message(q,"Update file failed integrity or EFI verification");return;
    }
    q->update_source_volume=volume;
    int j=0;for(;path[j]&&j<QFS_PATH-1;j++)q->update_source_path[j]=path[j];
    q->update_source_path[j]=0;
    q->update_target=qupd_target(q->storage,0);q->update_found=1;q->update_confirm=0;
    update_message(q,q->update_target>=0?"Verified update. Select Install to continue":
        "Verified update. Writable EFI system disk required");
}
static void update_install(QDesktop *q){
    if(!q->update_found||q->update_target<0){update_message(q,"Check for an update on a writable EFI disk");return;}
    if(q->update_confirm!=1){q->update_confirm=1;update_message(q,"Select Install again to replace EFI firmware");return;}
    q->update_confirm=0;
    if(qupd_apply(q->storage,q->update_target,q->update_source_volume,q->update_source_path,
                  q->media_buffer,q->media_capacity)==0){
        q->update_found=0;update_message(q,"Installed and verified. Reboot to start the new version");
    }else update_message(q,"Install failed. Check backup and writable EFI disk");
}
static void boot_diagnose(QDesktop *q);
static void update_integrity(QDesktop *q){
    boot_diagnose(q);
    update_message(q,"Boot diagnostics opened in Terminal");
}
static void update_rollback(QDesktop *q){
    if(!q->storage||!q->media_buffer)return;
    int target=qupd_target(q->storage,1);
    if(target<0){update_message(q,"No writable EFI disk with a previous version");return;}
    if(q->update_confirm!=2){q->update_confirm=2;update_message(q,"Select Roll Back again to restore previous EFI");return;}
    q->update_confirm=0;
    int ok=qupd_rollback(q->storage,target,q->media_buffer,q->media_capacity)==0;
    update_message(q,ok?"Previous firmware restored. Reboot to use it":"Rollback failed. Use recovery media");
}
static void update_clear_backup(QDesktop *q){
    if(!q->storage)return;
    int target=qupd_target(q->storage,1);
    if(target<0){update_message(q,"No saved firmware backup");return;}
    if(q->update_confirm!=3){q->update_confirm=3;update_message(q,"Select Discard Backup again to confirm");return;}
    q->update_confirm=0;
    update_message(q,qupd_clear_backup(q->storage,target)==0?
        "Previous firmware backup discarded":"Could not discard backup");
}
static void update_restore_settings(QDesktop *q){
    if(!q->storage)return;
    char original[QFS_PATH],backup[QFS_PATH];
    if(qfs_profile_path(q->storage,"QUARTZIE.CFG",original,sizeof(original))!=0||
       qfs_profile_path(q->storage,"QUARTZIE.BAK",backup,sizeof(backup))!=0)return;
    uint8_t settings[10];size_t len=0;
    if(qfs_read(q->storage,q->storage->home_index,backup,settings,sizeof(settings),&len)!=0||
       len!=10||settings[0]!='Q'||settings[1]!='Z'||settings[2]!='P'||settings[3]!='R'||
       settings[4]!=1||settings[5]>1||settings[6]>7||settings[7]>1||settings[8]>3){
        update_message(q,"No valid settings backup");return;
    }
    if(qfs_write(q->storage,q->storage->home_index,original,settings,len)!=0){
        update_message(q,"Could not restore settings backup");return;
    }
    q->theme=settings[5];q->accent=settings[6];q->animations_enabled=settings[7];
    q->wallpaper_mode=settings[8];
    if(q->wallpaper_mode==3){if(wallpaper_saved(q)!=0){q->wallpaper_mode=0;wallpaper(q);}}
    else wallpaper(q);
    q->dock_cache_valid=0;q->drag_cache_valid=0;q->glass_cached_app=-1;q->dirty=1;
    update_message(q,"Previous appearance settings restored");
}
static void draw_lock_screen(QDesktop *q){
    if(!q->locked)return;
    for(int y=0;y<q->height;y++)for(int x=0;x<q->width;x++){
        size_t at=(size_t)y*q->width+x;
        q->pixels[at]=blend(q->wallpaper[at],0x071522,165);
    }
    int x=q->width/2-225,y=q->height/2-180;
    pane(q,x,y,450,362,22,material(q),224);
    text(q,x+33,y+55,"Quartzie",foreground(q),2);
    const char *name=q->storage?q->storage->profile:"Default";
    if(q->profile_target>=0&&q->profile_target<q->profile_count)name=q->profiles[q->profile_target].name;
    string_limit(q,x+36,y+111,name,foreground(q),2,365,1);
    text(q,x+36,y+155,q->profile_count>1?"Previous user  <       >  Next user":"",secondary(q),0);
    surface(q,x+35,y+186,380,49,12,inner_material(q),205);
    if(q->pin_input_len){
        char marks[26];int n=0;
        for(;n<q->pin_input_len;n++)marks[n]='*';
        marks[n]=0;text(q,x+53,y+218,marks,foreground(q),1);
    }else text(q,x+53,y+218,"Account PIN",secondary(q),0);
    surface(q,x+35,y+250,380,49,12,colors(q)->selected,195);
    text(q,x+175,y+282,"Unlock",foreground(q),1);
    if(q->profile_status[0])string_limit(q,x+35,y+330,q->profile_status,secondary(q),0,382,1);
}

static void draw_settings(QDesktop *q,QWindow *w,int x,int y,int cw){
    int side=settings_sidebar_width(cw);
    int left=x+side+25;
    int width=cw-side-25;
    int nav_bottom=w->h-82;
    surface(q,x,y+11,side,nav_bottom-11,19,inner_material(q),q->theme?131:127);
    app_icon(q,x+38,y+47,Q_SETTINGS,40,foreground(q));
    text(q,x+68,y+42,"Quartzie",foreground(q),1);
    text(q,x+69,y+64,"Local device",secondary(q),0);
    surface(q,x+14,y+82,side-28,36,11,material(q),q->theme?153:176);
    string_limit(q,x+27,y+105,q->settings_search_length?q->settings_search:"Find a setting",q->settings_search_length?foreground(q):secondary(q),0,side-51,1);
    for(int i=0;i<Q_SETTINGS_PAGES;i++){
        int yy=y+128+i*(w->h<600?35:43);
        if(q->settings_page==i){
            surface(q,x+10,yy,side-20,38,10,colors(q)->selected,q->theme?182:167);
            rectangle(q,x+11,yy+8,3,22,accent(q));
        }
        settings_icon(q,x+32,yy+19,i);
        text(q,x+55,yy+25,settings_pages[i],foreground(q),0);
    }
    if(w->h>=700)text(q,x+21,y+w->h-113,"Quartzie 0.44.0",secondary(q),0);
    if(q->settings_search_length){
        text(q,left,y+39,"Search results",foreground(q),2);
        int row=0;
        for(int i=0;i<Q_SETTINGS_PAGES;i++)if(settings_result(q,i)){
            int yy=y+76+row*75;
            if(yy+67>y+w->h-76)break;
            const char *subtitles[Q_SETTINGS_PAGES]={"Overview","Display and performance","Colors and wallpapers","Installed applications","Volumes and drives","Device information","Network interfaces","User profiles and screen lock","Firmware installation and recovery"};
            settings_card(q,left,yy,width,66,settings_pages[i],subtitles[i],1);
            row++;
        }
        if(!row)text(q,left+8,y+116,"No matching settings",secondary(q),1);
        return;
    }
    int page=q->settings_page;
    text(q,left,y+39,settings_pages[page],foreground(q),2);
    const char *subtitles[Q_SETTINGS_PAGES]={
        "Your space, your settings",
        "Display and performance",
        "Make Quartzie yours",
        "Open your applications",
        "Manage local storage",
        "Device information",
        "Network adapters and link status",
        "User accounts and sign-in",
        "Updates, integrity and recovery"
    };
    text(q,left,y+69,subtitles[page],secondary(q),0);
    if(page==0){
        surface(q,left,y+92,width,105,18,colors(q)->selected,q->theme?171:174);
        app_icon(q,left+52,y+143,Q_SETTINGS,62,foreground(q));
        text(q,left+103,y+140,"Quartzie",foreground(q),2);
        text(q,left+104,y+168,"Ready for your next idea",secondary(q),0);
        const int targets[4]={1,2,3,4};
        const char *descriptions[4]={"Display and motion","Colors and appearance","Your apps","Files and drives"};
        int gap=12,tile=(width-gap)/2;
        int tile_y=w->h<550?201:210,step=w->h<550?93:102,tile_h=w->h<550?80:91;
        for(int i=0;i<4;i++){
            int bx=left+(i%2)*(tile+gap),by=y+tile_y+(i/2)*step;
            settings_card(q,bx,by,tile,tile_h,settings_pages[targets[i]],descriptions[i],1);
        }
    }else if(page==1){
        char resolution[32];settings_resolution(q,resolution);
        settings_card(q,left,y+92,width,77,"Display resolution",resolution,0);
        settings_card(q,left,y+181,width,82,"Window animations",q->animations_enabled?"On":"Off",0);
        settings_switch(q,left+width-74,y+207,q->animations_enabled);
        settings_card(q,left,y+275,width,77,"Personalization","Theme and accent colors",1);
        if(w->h>=560)settings_card(q,left,y+364,width,77,"About","System details",1);
    }else if(page==2){
        surface(q,left,y+92,width,123,18,inner_material(q),q->theme?124:166);
        text(q,left+20,y+125,"Choose your mode",foreground(q),1);
        int bx=left+19,button=(width-48)/2;
        for(int i=0;i<2;i++){
            int xx=bx+i*(button+10),selected=q->theme==i;
            surface(q,xx,y+144,button,52,13,selected?colors(q)->selected:material(q),selected?201:121);
            if(selected)rectangle(q,xx+13,y+159,4,22,accent(q));
            text(q,xx+27,y+176,i?"Dark":"Light",foreground(q),1);
        }
        surface(q,left,y+230,width,116,18,inner_material(q),q->theme?124:166);
        text(q,left+20,y+263,"Accent color",foreground(q),1);
        const uint32_t accents[8]={0x5dcfe3,0x9e9efa,0x77d5af,0xf4ae85,0xef709a,0xe8ca70,0x69a6ff,0xadaee7};
        for(int i=0;i<8;i++){
            int cx=left+44+(i%4)*61,cy=y+307+(i/4)*44;
            circular(q,cx,cy,18,accents[i],255);
            if(i==q->accent){
                circular(q,cx,cy,8,0xffffff,226);
                circular(q,cx,cy,4,accents[i],255);
            }
        }
        text(q,left+4,y+400,"Background",foreground(q),1);
        const char *styles[4]={"Quartz","Sunset","Mint","My photo"};
        for(int i=0;i<4;i++){
            int xx=left+i*118,yy=y+416;
            if(xx+112>left+width)break;
            surface(q,xx,yy,112,47,11,q->wallpaper_mode==i?colors(q)->selected:inner_material(q),q->theme?188:157);
            text(q,xx+10,yy+30,styles[i],foreground(q),0);
        }
        if(w->h>=570)settings_card(q,left,y+473,width,58,"Set a photo as wallpaper","Open Photos and select Set wallpaper",1);
    }else if(page==3){
        const int apps[5]={Q_FILES,Q_NOTES,Q_PHOTOS,Q_MEDIA,Q_STORE};
        const char *descriptions[5]={"Browse your drives","Edit text files","View pictures","Play audio and video","Explore available apps"};
        int row_h=clamp((w->h-230)/5,45,64);
        for(int i=0;i<5;i++){
            int yy=y+92+i*(row_h+9);
            surface(q,left,yy,width,row_h,14,inner_material(q),q->theme?125:158);
            app_icon(q,left+31,yy+row_h/2,apps[i],32,foreground(q));
            text(q,left+62,yy+27,names[apps[i]],foreground(q),1);
            if(row_h>=58)string_limit(q,left+62,yy+48,descriptions[i],secondary(q),0,width-109,1);
            segment(q,left+width-33,yy+row_h/2-5,left+width-27,yy+row_h/2,secondary(q));
            segment(q,left+width-27,yy+row_h/2,left+width-33,yy+row_h/2+5,secondary(q));
        }
    }else if(page==4){
        int count=q->storage?q->storage->count:0;
        if(!count){
            settings_card(q,left,y+92,width,84,"No drives available","Open Files to browse storage",1);
        }else{
            text(q,left+3,y+109,"Available drives",secondary(q),0);
            int max_rows=w->h>=550?4:3;
            int rows=count<max_rows?count:max_rows;
            for(int i=0;i<rows;i++){
                const char *label=q->storage->volumes[i].label;
                const char *description=i==q->storage->ram_index?"Temporary session storage":i==q->storage->home_index?"Persistent user files":"Mounted volume";
                char detail[80];
                uint64_t total=0,free_bytes=0;
                if(i!=q->storage->ram_index&&qfs_space(q->storage,i,&total,&free_bytes)==0){
                    char amount[20],limit[20];
                    settings_number(amount,(int)clamp((int)(free_bytes/1048576u),0,999999));
                    settings_number(limit,(int)clamp((int)(total/1048576u),0,999999));
                    int at=0;
                    for(int j=0;amount[j]&&at<75;j++)detail[at++]=amount[j];
                    const char *part=" MiB free of ";
                    for(int j=0;part[j]&&at<75;j++)detail[at++]=part[j];
                    for(int j=0;limit[j]&&at<75;j++)detail[at++]=limit[j];
                    const char *end=" MiB";
                    for(int j=0;end[j]&&at<75;j++)detail[at++]=end[j];
                    detail[at]=0;description=detail;
                }
                settings_card(q,left,y+124+i*79,width,69,label,description,1);
            }
        }
        text(q,left+5,y+w->h-109,"Select a drive to browse it in Files.",secondary(q),0);
    }else if(page==7){
        int compact=w->h<600,create_y=compact?322:446,lock_y=compact?375:499;
        int list_y=compact?177:211,list_count=compact?3:5;
        settings_card(q,left,y+92,width,compact?66:76,"Signed in",q->storage?q->storage->profile:"Default",0);
        text(q,left+5,y+(compact?169:196),"Accounts",foreground(q),1);
        for(int i=0;i<q->profile_count&&i<list_count;i++){
            int yy=y+list_y+i*(compact?44:43);
            surface(q,left+2,yy,width-4,38,11,q->profile_target==i?colors(q)->selected:inner_material(q),165);
            string_limit(q,left+19,yy+26,q->profiles[i].name,foreground(q),0,width-120,1);
            text(q,left+width-90,yy+25,q->profiles[i].has_pin?"PIN":"No PIN",secondary(q),0);
        }
        surface(q,left+2,y+create_y,165,41,11,inner_material(q),175);
        text(q,left+15,y+create_y+26,"Create account",foreground(q),0);
        surface(q,left+174,y+create_y,165,41,11,inner_material(q),175);
        text(q,left+188,y+create_y+26,"Set account PIN",foreground(q),0);
        surface(q,left+2,y+lock_y,176,41,11,colors(q)->selected,185);
        text(q,left+27,y+lock_y+26,"Lock screen",foreground(q),0);
        if(q->profile_edit){
            surface(q,left+187,y+lock_y,width-189,41,11,inner_material(q),185);
            char obscured[25];int k=0;
            if(q->profile_edit==2&&q->profile_input_len){
                for(;k<q->profile_input_len;k++)obscured[k]='*';
                obscured[k]=0;
            }
            string_limit(q,left+198,y+lock_y+26,q->profile_input_len?(q->profile_edit==2?obscured:q->profile_input):"Type and press Enter",foreground(q),0,width-208,1);
        }
        if(q->profile_status[0])string_limit(q,left+7,y+(compact?440:560),q->profile_status,secondary(q),0,width-12,1);
    }else if(page==8){
        const char *actions[]={"Check for updates","Install verified update","Run boot diagnostics","Roll back firmware","Restore appearance settings","Discard rollback backup"};
        const char *descs[]={"Search FAT drives and Downloads","Write to a writable EFI system disk","Inspect boot EFI, storage, accounts and fallback","Restore the saved previous EFI","Recover the last saved configuration","Allow installation of a further update"};
        int row=clamp((w->h-175)/6,49,76);
        for(int i=0;i<6;i++)settings_card(q,left,y+92+i*row,width,row-8,actions[i],descs[i],1);
        if(q->update_status[0])string_limit(q,left+5,y+w->h-57,q->update_status,secondary(q),0,width-10,1);
    }else if(page==6){
        text(q,left+5,y+112,"Detected network interfaces",foreground(q),1);
        if(!q->network||!q->network->count)
            settings_card(q,left,y+139,width,80,"No network adapter","No UEFI SNP device is available",0);
        else for(int i=0;i<q->network->count&&i<4;i++){
            QNetAdapter *adapter=&q->network->adapters[i];
            int top=y+139+i*97;
            settings_card(q,left,top,width,87,adapter->mac,
                adapter->link_known?(adapter->link_present?"Cable connected":"Cable disconnected"):"Link status unavailable",0);
        }
        text(q,left+5,y+w->h-104,"Hardware link and MAC information",secondary(q),0);
    }else if(page==5){
        surface(q,left,y+92,width,122,18,colors(q)->selected,q->theme?160:169);
        app_icon(q,left+49,y+151,Q_ABOUT,59,foreground(q));
        text(q,left+95,y+145,"Quartzie",foreground(q),2);
        text(q,left+96,y+173,"Version 0.44.0",secondary(q),0);
        char resolution[32];settings_resolution(q,resolution);
        settings_card(q,left,y+227,width,74,"Architecture","x86-64 / UEFI",0);
        settings_card(q,left,y+313,width,74,"Current display",resolution,0);
        if(w->h>=580)settings_card(q,left,y+399,width,74,"Interface","Quartz glass",0);
    }
}
static void settings_click(QDesktop *q,QWindow *w,int x,int y){
    int bx=w->x+32,by=w->y+60,cw=w->w-64;
    int side=settings_sidebar_width(cw);
    int left=bx+side+25,width=cw-side-25;
    for(int i=0;i<Q_SETTINGS_PAGES;i++){
        if(hit(x,y,bx+10,by+128+i*(w->h<600?35:43),side-20,38)){settings_navigate(q,i);return;}
    }
    if(hit(x,y,bx+14,by+82,side-28,36)){q->dirty=1;return;}
    if(q->settings_search_length){
        int row=0;
        for(int i=0;i<Q_SETTINGS_PAGES;i++)if(settings_result(q,i)){
            int yy=by+76+row*75;
            if(yy+67>by+w->h-76)break;
            if(hit(x,y,left,yy,width,66)){settings_navigate(q,i);return;}
            row++;
        }
        return;
    }
    switch(q->settings_page){
        case 0:{
            const int pages[4]={1,2,3,4};
            int tile=(width-12)/2;
            int tile_y=w->h<550?201:210,step=w->h<550?93:102,tile_h=w->h<550?80:91;
            for(int i=0;i<4;i++)if(hit(x,y,left+(i%2)*(tile+12),by+tile_y+(i/2)*step,tile,tile_h)){
                settings_navigate(q,pages[i]);return;
            }
            break;
        }
        case 1:
            if(hit(x,y,left,by+181,width,82)){settings_motion(q,!q->animations_enabled);return;}
            if(hit(x,y,left,by+275,width,77)){settings_navigate(q,2);return;}
            if(w->h>=560&&hit(x,y,left,by+364,width,77)){settings_navigate(q,5);return;}
            break;
        case 2:{
            int button=(width-48)/2;
            for(int i=0;i<2;i++)if(hit(x,y,left+19+i*(button+10),by+144,button,52)){q_set_theme(q,i);return;}
            for(int i=0;i<8;i++){
                int cx=left+44+(i%4)*61,cy=by+307+(i/4)*44;
                if(hit(x,y,cx-22,cy-22,44,44)){q_set_accent(q,i);return;}
            }
            for(int i=0;i<4;i++)if(hit(x,y,left+i*118,by+416,112,47)){
                if(q_wallpaper_mode(q,i)!=0)profile_message(q,"No saved photo wallpaper");
                return;
            }
            if(w->h>=570&&hit(x,y,left,by+473,width,58)){q_open(q,Q_PHOTOS);return;}
            break;
        }
        case 3:{
            const int apps[5]={Q_FILES,Q_NOTES,Q_PHOTOS,Q_MEDIA,Q_STORE};
            int row_h=clamp((w->h-230)/5,45,64);
            for(int i=0;i<5;i++)if(hit(x,y,left,by+92+i*(row_h+9),width,row_h)){
                q_open(q,apps[i]);return;
            }
            break;
        }
        case 7:{
            int compact=w->h<600,create_y=compact?322:446,lock_y=compact?375:499;
            if(hit(x,y,left+2,by+create_y,165,41)){
                q->profile_edit=1;q->profile_input_len=0;q->profile_input[0]=0;q->dirty=1;return;
            }
            if(hit(x,y,left+174,by+create_y,165,41)){
                q->profile_edit=2;q->profile_input_len=0;q->profile_input[0]=0;q->dirty=1;return;
            }
            if(hit(x,y,left+2,by+lock_y,176,41)){q_lock(q);return;}
            for(int i=0;i<q->profile_count&&i<(compact?3:5);i++)
                if(hit(x,y,left+2,by+(compact?177:211)+i*(compact?44:43),width-4,38)){
                q_lock(q);q->profile_target=i;return;
            }
            break;
        }
        case 8:{
            int row=clamp((w->h-175)/6,49,76);
            for(int i=0;i<6;i++)if(hit(x,y,left,by+92+i*row,width,row-8)){
                if(i==0)update_check(q);
                if(i==1)update_install(q);
                if(i==2)update_integrity(q);
                if(i==3)update_rollback(q);
                if(i==4)update_restore_settings(q);
                if(i==5)update_clear_backup(q);
                return;
            }
            break;
        }
        case 4:{
            int count=q->storage?q->storage->count:0;
            int max_rows=w->h>=550?4:3;
            int rows=count<max_rows?count:max_rows;
            for(int i=0;i<rows;i++)if(hit(x,y,left,by+124+i*79,width,69)){
                q->file_volume=i;q->file_path[0]='\\';q->file_path[1]=0;
                files_refresh(q);q_open(q,Q_FILES);return;
            }
            if(!count&&hit(x,y,left,by+92,width,84)){q_open(q,Q_FILES);return;}
            break;
        }
    }
}
static void window_content(QDesktop *q,int app,QWindow *w){
    int x=w->x+32,y=w->y+60,cw=w->w-64;
    uint32_t ink=foreground(q),muted=secondary(q);
    if(app==Q_FILES){
        draw_files(q,w,x,y,cw,ink,muted);
    }else if(app==Q_NOTES){
        draw_notes(q,w,x,y,cw,ink,muted);
    }else if(app==Q_TERMINAL){
        surface(q,x,y+30,cw,w->h-157,17,0x0d2237,221);
        string_limit(q,x+18,y+62,q->terminal_path,0x8edee8,0,cw-40,1);
        int spacing=clamp((w->h-345)/7,16,26);
        for(int i=0;i<q->terminal_count;i++)
            string_limit(q,x+22,y+91+i*spacing,q->terminal_lines[7-q->terminal_count+i],0xe0f5fb,0,cw-42,1);
        text(q,x+22,y+w->h-179,">",0x88d8e5,1);
        string_limit(q,x+48,y+w->h-179,q->command,0xf8feff,1,cw-66,1);
    }else if(app==Q_CALCULATOR){
        surface(q,x,y+27,322,81,16,inner_material(q),q->theme?111:144);
        const char *output=q->calc_length?q->calc:q->answer;
        string_limit(q,x+22,y+80,output,ink,2,285,1);
        const char *keys="789/456*123-C0=+";
        for(int i=0;i<16;i++){
            int bx=x+(i%4)*82,by=y+128+(i/4)*57;
            surface(q,bx,by,73,49,13,(i%4)==3?colors(q)->operator_button:inner_material(q),q->theme?174:151);
            char value[2]={keys[i],0};
            text(q,bx+(73-text_width(value,1))/2,by+32,value,ink,1);
        }
        int history_width=cw-350;
        if(history_width>80){
            surface(q,x+343,y+27,history_width+7,327,16,inner_material(q),q->theme?92:125);
            text(q,x+355,y+59,"Recent",ink,0);
            for(int i=0;i<q->calc_history_count;i++)
                string_limit(q,x+355,y+99+i*77,q->calc_history[i],secondary(q),0,history_width-17,2);
            if(!q->calc_history_count)
                string_limit(q,x+355,y+100,"No history",secondary(q),0,history_width-17,1);
        }
    }else if(app==Q_SETTINGS){
        draw_settings(q,w,x,y,cw);
    }else if(app==Q_PHOTOS||app==Q_MEDIA){
        draw_viewer(q,app,w,x,y,cw,ink,muted);
    }else if(app==Q_TASKS){
        draw_tasks(q,w,x,y,cw,ink,muted);
    }else if(app==Q_STORE){
        if(q->store_pending){
            text(q,x+4,y+45,"Install Quartzie package",ink,2);
            surface(q,x+2,y+63,cw-4,236,16,inner_material(q),q->theme?125:152);
            string_limit(q,x+19,y+103,q->store_info.name,ink,1,cw-40,1);
            string_limit(q,x+19,y+136,q->store_info.id,muted,0,cw-40,1);
            string_limit(q,x+19,y+166,q->store_info.version,muted,0,cw-40,1);
            text(q,x+19,y+196,files_equal(q->store_info.platform,"android")?"Android package":"Windows executable",ink,0);
            text(q,x+19,y+225,"SHA-256 and archive integrity verified",muted,0);
            surface(q,x+18,y+246,140,40,11,colors(q)->selected,186);
            text(q,x+51,y+272,"Install",ink,1);
            surface(q,x+169,y+246,130,40,11,inner_material(q),154);
            text(q,x+195,y+272,"Cancel",ink,0);
        }else{
            text(q,x,y+47,q->store_view?"Installed packages":"App Library",ink,1);
            surface(q,x+cw-196,y+13,192,37,10,inner_material(q),q->theme?151:170);
            text(q,x+cw-183,y+38,q->store_view?"Built-in apps":"Installed packages",ink,0);
            if(q->store_view){
                if(!store_directory(q))
                    text(q,x+12,y+117,"Attach a writable Home disk",muted,1);
                else if(!q->store_installed_count)
                    text(q,x+12,y+117,"No installed packages",muted,1);
                for(int i=0;i<q->store_installed_count&&i<5;i++){
                    int yy=y+78+i*59;
                    surface(q,x+1,yy,cw-2,53,12,inner_material(q),q->theme?139:154);
                    sprite(q,x+32,yy+26,QI_COMPRESSED,32);
                    string_limit(q,x+59,yy+32,q->store_installed[i].name,ink,0,cw-207,1);
                    surface(q,x+cw-139,yy+8,126,38,9,i==q->store_confirm_remove?colors(q)->selected:inner_material(q),154);
                    text(q,x+cw-126,yy+34,i==q->store_confirm_remove?"Confirm remove":"Remove",ink,0);
                }
            }else{
                const int apps[8]={Q_NOTES,Q_CALCULATOR,Q_FILES,Q_TERMINAL,Q_SETTINGS,Q_PHOTOS,Q_MEDIA,Q_ABOUT};
                for(int i=0;i<4;i++){
                    int app_index=apps[q->store_page*4+i];
                    int pitch=w->w>650?230:190;
                    int bx=x+(i%2)*pitch,by=y+78+(i/2)*118;
                    surface(q,bx,by,174,102,18,inner_material(q),q->theme?174:184);
                    app_icon(q,bx+39,by+39,app_index,43,ink);
                    text(q,bx+72,by+40,names[app_index],ink,0);
                    text(q,bx+72,by+69,"Open",accent(q),0);
                }
                text(q,x,y+333,q->store_page?"Previous":"Next",accent(q),1);
                text(q,x+150,y+333,q->store_page?"2 / 2":"1 / 2",muted,0);
            }
        }
        if(q->store_status[0])string_limit(q,x+5,y+w->h-111,q->store_status,muted,0,cw-10,1);
    }else{
        text(q,x,y+45,"Quartzie 0.44.0",ink,2);
        text(q,x,y+97,"A space to create.",ink,1);
        thin_rule(q,x,y+122,clamp(cw-28,1,cw),55);
        text(q,x,y+160,"Keyboard",muted,0);
        text(q,x,y+192,"F1-F6  Open applications",ink,0);
        text(q,x,y+220,"F7  Applications",ink,0);
        text(q,x,y+248,"F8  Notifications",ink,0);
        text(q,x,y+276,"F9  Control center",ink,0);
        text(q,x,y+304,"Esc  Close overlays",ink,0);
    }
}
static void prepare_window_glass(QDesktop *q,int app,QWindow *w);
static void draw_window(QDesktop *q,int app){
    QWindow *w=&q->windows[app];
    if(!w->open||w->minimized||w->desktop!=q->desktop)return;
    int previous_render_app=q->render_app;
    q->render_app=app;
    prepare_window_glass(q,app,w);
    if(w->maximized)surface(q,w->x,w->y,w->w,w->h,0,material(q),q->theme?209:149);
    else pane(q,w->x,w->y,w->w,w->h,25,material(q),q->theme?209:149);
    thin_rule(q,w->x+23,w->y+56,w->w-46,28);
    app_icon(q,w->x+41,w->y+28,app,25,foreground(q));
    text(q,w->x+61,w->y+36,app==Q_STORE?"Quartzie Store":names[app],foreground(q),1);
    surface(q,w->x+w->w-157,w->y+13,36,29,10,inner_material(q),q->theme?90:151);
    surface(q,w->x+w->w-108,w->y+13,36,29,10,inner_material(q),q->theme?90:151);
    surface(q,w->x+w->w-59,w->y+13,36,29,10,colors(q)->close_button,q->theme?185:166);
    rectangle(q,w->x+w->w-147,w->y+29,14,2,foreground(q));
    int bx=w->x+w->w-99,by=w->y+20;
    if(w->maximized){
        segment(q,bx+2,by,bx+12,by,foreground(q));
        segment(q,bx+12,by,bx+12,by+10,foreground(q));
        segment(q,bx,by+3,bx+9,by+3,foreground(q));
        segment(q,bx,by+3,bx,by+13,foreground(q));
        segment(q,bx,by+13,bx+9,by+13,foreground(q));
    }else{
        segment(q,bx,by,bx+13,by,foreground(q));
        segment(q,bx,by,bx,by+13,foreground(q));
        segment(q,bx+13,by,bx+13,by+13,foreground(q));
        segment(q,bx,by+13,bx+13,by+13,foreground(q));
    }
    segment(q,w->x+w->w-48,w->y+20,w->x+w->w-35,w->y+33,colors(q)->close_ink);
    segment(q,w->x+w->w-35,w->y+20,w->x+w->w-48,w->y+33,colors(q)->close_ink);
    window_content(q,app,w);
    q->render_app=previous_render_app;
}
static void clip_full(QDesktop *q){
    q->clip_left=0;q->clip_top=0;q->clip_right=q->width;q->clip_bottom=q->height;
}
static void clip_region(QDesktop *q,int x0,int y0,int x1,int y1){
    q->clip_left=clamp(x0,0,q->width);q->clip_top=clamp(y0,0,q->height);
    q->clip_right=clamp(x1,q->clip_left,q->width);
    q->clip_bottom=clamp(y1,q->clip_top,q->height);
}
static void draw_base(QDesktop *q,int skip){
    for(int y=q->clip_top;y<q->clip_bottom;y++){
        size_t row=(size_t)y*q->width;
        for(int x=q->clip_left;x<q->clip_right;x++)q->pixels[row+x]=q->wallpaper[row+x];
    }
    int drawn[Q_APPS]={0};
    for(int layer=0;layer<Q_APPS;layer++){
        int app=-1;
        for(int i=0;i<Q_APPS;i++)if(i!=skip&&!drawn[i]&&q->windows[i].open&&!q->windows[i].minimized&&q->windows[i].desktop==q->desktop&&(app<0||q->windows[i].z<q->windows[app].z))app=i;
        if(app<0)break;
        drawn[app]=1;draw_window(q,app);
    }
}
static void cache_dock_background(QDesktop *q){
    if(!q->dock_background||q->drag>=0||q->clip_left||q->clip_top||q->clip_right!=q->width||q->clip_bottom!=q->height)return;
    int x=dock_x(q)-52,y=dock_y(q)-53;
    for(int row=0;row<137;row++){
        size_t src=(size_t)(y+row)*q->width+x;
        size_t dst=(size_t)row*736;
        for(int col=0;col<736;col++)q->dock_background[dst+col]=q->pixels[src+col];
    }
    q->dock_cache_valid=1;
}

static int snap_menu_left(QDesktop *q,QWindow *w){return clamp(w->x+w->w-265,8,q->width-264);}
static int snap_menu_top(QDesktop *q,QWindow *w){return clamp(w->y+48,8,q->height-143);}
static void draw_snap_menu(QDesktop *q){
    int app=q->snap_menu_app;
    if(app<0||app>=Q_APPS||!q->windows[app].open||q->windows[app].desktop!=q->desktop)return;
    QWindow *w=&q->windows[app];
    int x=snap_menu_left(q,w),y=snap_menu_top(q,w);
    pane(q,x,y,256,133,17,material(q),q->theme?246:231);
    text(q,x+12,y+26,"Snap layouts",foreground(q),1);
    for(int i=0;i<6;i++){
        int bx=x+11+(i%3)*81,by=y+37+(i/3)*43;
        surface(q,bx,by,74,37,8,w->snap_zone==i+1?colors(q)->selected:inner_material(q),q->theme?178:156);
        int mid=bx+37;
        rectangle(q,mid-23,by+7,46,23,q->theme?0x52687f:0xb4c8db);
        int px=(i==1||i==3||i==5)?mid:mid-23;
        int py=(i>=4)?by+18:by+7;
        rectangle(q,px,py,23,i>=2?11:23,accent(q));
    }
}
static void draw_desktop_view(QDesktop *q){
    if(!q->desktop_view)return;
    int x=clamp((q->width-522)/2,5,q->width-522),y=dock_y(q)-173;
    pane(q,x,y,522,157,19,material(q),q->theme?230:218);
    text(q,x+17,y+30,"Desktops",foreground(q),1);
    for(int i=0;i<3;i++){
        int px=x+16+i*169;
        surface(q,px,y+43,155,98,13,i==q->desktop?colors(q)->selected:inner_material(q),q->theme?191:159);
        char name[]="Desktop 1";name[8]=(char)('1'+i);
        text(q,px+17,y+70,name,foreground(q),1);
        text(q,px+17,y+94,"Switch",secondary(q),0);
        text(q,px+17,y+121,"Move active",accent(q),0);
    }
}

static void draw_overlays(QDesktop *q){
    cache_dock_background(q);
    draw_dock(q);
    draw_snap_menu(q);
    draw_desktop_view(q);
    draw_menu(q);
    draw_notifications(q);
    draw_quick(q);
}
static void draw_scene(QDesktop *q,int skip){
    draw_base(q,skip);
    draw_overlays(q);
}
static void prepare_window_glass(QDesktop *q,int app,QWindow *w){
    if(q->drag==app&&q->glass_cached_app==app)return;
    int left=clamp(w->x,0,q->width),top=clamp(w->y,0,q->height);
    int right=clamp(w->x+w->w,0,q->width),bottom=clamp(w->y+w->h,0,q->height);
    if(left>=right||top>=bottom){q->glass_cached_app=-1;return;}
    int sx=clamp((left-24)&~3,0,q->width);
    int sy=clamp((top-24)&~3,0,q->height);
    int ex=clamp((right+27)&~3,0,q->width);
    int ey=clamp((bottom+27)&~3,0,q->height);
    int cols=(ex-sx+GLASS_SCALE-1)/GLASS_SCALE;
    int rows=(ey-sy+GLASS_SCALE-1)/GLASS_SCALE;
    if(cols<1||rows<1||cols>GLASS_SIDE||rows>GLASS_ROWS){q->glass_cached_app=-1;return;}
    glass_filter(q,sx,sy,cols,rows);
    for(int py=top;py<bottom;py++)for(int px=left;px<right;px++)
        q->blurred[(size_t)py*q->width+px]=glass_fetch(glass_final,px,py,sx,sy,cols,rows);
    glass_cache_cols=cols;glass_cache_rows=rows;q->glass_cached_app=app;
}
void q_drag_buffers(QDesktop *q,uint32_t *background){
    q->drag_base=background;
}
void q_dock_buffers(QDesktop *q,uint32_t *background){
    q->dock_background=background;
    q->dock_cache_valid=0;
}
static void drag_capture(QDesktop *q){
    int app=q->drag;
    QWindow *w=&q->windows[app];
    clip_full(q);
    draw_base(q,app);
    size_t total=(size_t)q->width*q->height;
    for(size_t i=0;i<total;i++)q->drag_base[i]=q->pixels[i];
    glass_cache_cols=(q->width+GLASS_SCALE-1)/GLASS_SCALE;
    glass_cache_rows=(q->height+GLASS_SCALE-1)/GLASS_SCALE;
    glass_filter(q,0,0,glass_cache_cols,glass_cache_rows);
    for(int i=0;i<glass_cache_cols*glass_cache_rows;i++)glass_cached[i]=glass_final[i];
    for(int y=0;y<q->height;y++)for(int x=0;x<q->width;x++)q->blurred[(size_t)y*q->width+x]=glass_fetch(glass_cached,x,y,0,0,glass_cache_cols,glass_cache_rows);
    q->glass_cached_app=app;
    draw_window(q,app);
    draw_overlays(q);
    q->drag_prev_x=w->x;q->drag_prev_y=w->y;
    q->drag_cache_app=app;q->drag_cache_valid=1;
    q->damage_valid=0;
}
static void drag_redraw(QDesktop *q){
    QWindow *w=&q->windows[q->drag];
    int pad=96;
    int x0=clamp((w->x<q->drag_prev_x?w->x:q->drag_prev_x)-pad,0,q->width);
    int y0=clamp((w->y<q->drag_prev_y?w->y:q->drag_prev_y)-pad,0,q->height);
    int x1=clamp((w->x>q->drag_prev_x?w->x:q->drag_prev_x)+w->w+pad,0,q->width);
    int y1=clamp((w->y>q->drag_prev_y?w->y:q->drag_prev_y)+w->h+pad,0,q->height);
    int dx=dock_x(q)-52,dy=dock_y(q)-53;
    int dx2=dock_x(q)+684,dy2=dock_y(q)+84;
    if(x0<dx2&&x1>dx&&y0<dy2&&y1>dy){
        x0=clamp(x0<dx-32?x0:dx-32,0,q->width);
        x1=clamp(x1>dx2+32?x1:dx2+32,0,q->width);
        y0=clamp(y0<dy-32?y0:dy-32,0,q->height);
        y1=clamp(y1>dy2+32?y1:dy2+32,0,q->height);
    }
    if(x0>=x1||y0>=y1){q->damage_valid=0;return;}
    clip_region(q,x0,y0,x1,y1);
    for(int y=y0;y<y1;y++){
        size_t row=(size_t)y*q->width;
        for(int x=x0;x<x1;x++)q->pixels[row+x]=q->drag_base[row+x];
    }
    draw_window(q,q->drag);
    draw_overlays(q);
    clip_full(q);
    q->drag_prev_x=w->x;q->drag_prev_y=w->y;
    q->damage_x=x0;q->damage_y=y0;q->damage_w=x1-x0;q->damage_h=y1-y0;
    q->damage_valid=1;
}
static uint32_t geometry_color(uint32_t base,uint32_t original,uint32_t final){
    int r=(int)(base>>16&255)+(int)(final>>16&255)-(int)(original>>16&255);
    int g=(int)(base>>8&255)+(int)(final>>8&255)-(int)(original>>8&255);
    int b=(int)(base&255)+(int)(final&255)-(int)(original&255);
    return rgb(clamp(r,0,255),clamp(g,0,255),clamp(b,0,255));
}
static void paint_window_geometry(QDesktop *q,int weight){
    int app=q->scene_tween_app;
    int ax,ay,aw,ah,bx,by,bw,bh;
    if(q->scene_tween_kind==7){
        bx=menu_x(q);by=menu_y(q);bw=510;bh=434;
        ax=bx;ay=by;aw=bw;ah=bh;
    }else{
        if(app<0||app>=Q_APPS)return;
        const QWindow *w=&q->windows[app];
        ax=w->tween_x;ay=w->tween_y;aw=w->tween_w;ah=w->tween_h;
        bx=w->x;by=w->y;bw=w->w;bh=w->h;
    }
    if(q->scene_tween_kind==1||q->scene_tween_kind==7){
        ax=bx+(bw*4)/100;
        ay=by+(bh*4)/100+12;
        aw=bw-(bw*8)/100;
        ah=bh-(bh*8)/100;
    }
    int ix=ax+(int)(((int64_t)(bx-ax)*weight)/256);
    int iy=ay+(int)(((int64_t)(by-ay)*weight)/256);
    int iw=aw+(int)(((int64_t)(bw-aw)*weight)/256);
    int ih=ah+(int)(((int64_t)(bh-ah)*weight)/256);
    if(iw<1||ih<1)return;
    int x0=q->scene_x0,y0=q->scene_y0,x1=q->scene_x1,y1=q->scene_y1;
    for(int y=y0;y<y1;y++){
        size_t row=(size_t)y*q->width;
        for(int x=x0;x<x1;x++)q->pixels[row+x]=scene_back[row+x];
    }
    int left=clamp(ix-32,x0,x1),top=clamp(iy-32,y0,y1);
    int right=clamp(ix+iw+32,x0,x1),bottom=clamp(iy+ih+32,y0,y1);
    static int map_x[Q_MAX_W];
    for(int x=left;x<right;x++){
        int sx=bx+(int)(((int64_t)(x-ix)*bw)/iw);
        if(x<ix)sx=x-ix+bx;
        else if(x>=ix+iw)sx=x-(ix+iw)+bx+bw;
        map_x[x]=sx;
    }
    for(int y=top;y<bottom;y++){
        int sy=by+(int)(((int64_t)(y-iy)*bh)/ih);
        if(y<iy)sy=y-iy+by;
        else if(y>=iy+ih)sy=y-(iy+ih)+by+bh;
        if(sy<0||sy>=q->height)continue;
        size_t row=(size_t)y*q->width;
        size_t srcrow=(size_t)sy*q->width;
        for(int x=left;x<right;x++){
            int sx=map_x[x];
            if(sx<0||sx>=q->width)continue;
            size_t dst=row+x,src=srcrow+sx;
            q->pixels[dst]=geometry_color(scene_back[dst],scene_back[src],scene_to[src]);
        }
    }
    clip_region(q,x0,y0,x1,y1);
    draw_dock(q);
    clip_full(q);
}
static void paint_scene_tween(QDesktop *q){
    int duration=(q->scene_tween_kind==1||q->scene_tween_kind==7)?110000:q->scene_tween_kind==2?125000:130000;
    int progress=clamp((int)((uint64_t)q->scene_elapsed_us*256/(uint32_t)duration),0,256);
    int weight=progress*progress*(768-2*progress)/(256*256);
    int x0=q->scene_x0,y0=q->scene_y0,x1=q->scene_x1,y1=q->scene_y1;
    if((q->scene_tween_kind==1||q->scene_tween_kind==2||q->scene_tween_kind==7)&&weight>0&&weight<256){
        paint_window_geometry(q,weight);
        q->damage_x=x0;q->damage_y=y0;q->damage_w=x1-x0;q->damage_h=y1-y0;
        q->damage_valid=1;
        return;
    }
    int shift=0;
    if(q->scene_tween_kind==5)shift=(256-weight)*10/256;
    if(q->scene_tween_kind==3||q->scene_tween_kind==4||q->scene_tween_kind==6)shift=weight*9/256;
    for(int y=y0;y<y1;y++){
        size_t row=(size_t)y*q->width;
        for(int x=x0;x<x1;x++){
            size_t p=row+x;
            uint32_t from=scene_from[p],to=scene_to[p];
            if(shift>0){
                if(q->scene_tween_kind==1||q->scene_tween_kind==5){
                    int sy=y+shift;
                    if(sy<q->height)to=scene_to[(size_t)sy*q->width+x];
                }else{
                    int sy=y-shift;
                    if(sy>=0)from=scene_from[(size_t)sy*q->width+x];
                }
            }
            q->pixels[p]=blend(from,to,weight);
        }
    }
    q->damage_x=x0;q->damage_y=y0;q->damage_w=x1-x0;q->damage_h=y1-y0;
    q->damage_valid=1;
    if(progress==256){
        q->scene_tween_active=0;
        q->drag_cache_valid=0;
        q->glass_cached_app=-1;
        clip_full(q);
        draw_scene(q,-1);
        q->damage_valid=0;
    }
}
void q_draw(QDesktop *q){
    if(!q->dirty)return;
    if(q->scene_tween_pending&&q->drag<0){
        q->scene_tween_pending=0;
        q->drag_cache_valid=0;
        q->glass_cached_app=-1;
        clip_full(q);
        draw_scene(q,-1);
        int left=q->width,top=q->height,right=0,bottom=0;
        for(int y=0;y<q->height;y++){
            size_t row=(size_t)y*q->width;
            for(int x=0;x<q->width;x++){
                size_t p=row+x;
                scene_to[p]=q->pixels[p];
                if(scene_from[p]!=scene_to[p]){
                    if(x<left)left=x;
                    if(y<top)top=y;
                    if(x+1>right)right=x+1;
                    if(y+1>bottom)bottom=y+1;
                }
            }
        }
        if(left<right&&top<bottom){
            if((q->scene_tween_kind==1||q->scene_tween_kind==2)&&q->scene_tween_app>=0&&q->scene_tween_app<Q_APPS){
                QWindow *target=&q->windows[q->scene_tween_app];
                int old=target->minimized;
                target->minimized=1;
                draw_scene(q,-1);
                size_t total=(size_t)q->width*q->height;
                for(size_t i=0;i<total;i++)scene_back[i]=q->pixels[i];
                target->minimized=old;
                for(size_t i=0;i<total;i++)q->pixels[i]=scene_from[i];
            }else if(q->scene_tween_kind==7){
                q->menu=0;
                draw_scene(q,-1);
                size_t total=(size_t)q->width*q->height;
                for(size_t i=0;i<total;i++)scene_back[i]=q->pixels[i];
                q->menu=1;
                for(size_t i=0;i<total;i++)q->pixels[i]=scene_from[i];
            }
            q->scene_x0=clamp(left-20,0,q->width);
            q->scene_y0=clamp(top-20,0,q->height);
            q->scene_x1=clamp(right+20,0,q->width);
            q->scene_y1=clamp(bottom+20,0,q->height);
            q->scene_tween_active=1;
            paint_scene_tween(q);
        }else q->scene_tween_active=0;
    }else if(q->scene_tween_active&&q->drag<0){
        paint_scene_tween(q);
    }else if(q->drag>=0&&q->drag_base){
        q->scene_tween_pending=0;q->scene_tween_active=0;
        int cached=q->drag_cache_valid&&q->drag_cache_app==q->drag;
        if(cached)drag_redraw(q);
        else drag_capture(q);
    }else if(q->hover_only){
        q->drag_cache_valid=0;
        int x0=dock_x(q)-52,x1=dock_x(q)+684;
        int y0=dock_y(q)-53,y1=dock_y(q)+84;
        if(q->menu){
            int mx=menu_x(q),my=menu_y(q);
            x0=clamp(x0<mx-42?x0:mx-42,0,q->width);
            x1=clamp(x1>mx+552?x1:mx+552,0,q->width);
            y0=clamp(y0<my-42?y0:my-42,0,q->height);
            y1=clamp(dock_y(q)+84,0,q->height);
        }
        clip_region(q,x0,y0,x1,y1);
        if(q->dock_cache_valid&&q->dock_background&&!q->menu){
            int bx=dock_x(q)-52,by=dock_y(q)-53;
            for(int row=0;row<137;row++){
                size_t dst=(size_t)(by+row)*q->width+bx;
                size_t src=(size_t)row*736;
                for(int col=0;col<736;col++)q->pixels[dst+col]=q->dock_background[src+col];
            }
            draw_dock(q);
        }else{
            draw_scene(q,-1);
        }
        q->damage_x=q->clip_left;q->damage_y=q->clip_top;
        q->damage_w=q->clip_right-q->clip_left;q->damage_h=q->clip_bottom-q->clip_top;
        q->damage_valid=1;
        clip_full(q);
    }else{
        q->drag_cache_valid=0;q->damage_valid=0;
        clip_full(q);
        draw_scene(q,-1);
    }
    q->hover_only=0;
    q->hover_last=q->hover;
    if(q->locked){clip_full(q);draw_lock_screen(q);q->damage_valid=0;}
    q->frame_count++;
    q->dirty=0;
}
static int calc_number(const char *expr,int *index,int64_t *out){
    int sign=1,digits=0;
    if(expr[*index]=='-'){sign=-1;(*index)++;}
    int64_t value=0;
    while(expr[*index]>='0'&&expr[*index]<='9'){
        if(value>100000000)return 0;
        value=value*10+expr[(*index)++]-'0';
        digits++;
    }
    if(!digits||value>1000000000)return 0;
    *out=value*sign;
    return 1;
}
static int calc_term(const char *expression,int *position,int64_t *result){
    if(!calc_number(expression,position,result))return 0;
    while(expression[*position]=='*'||expression[*position]=='/'){
        char operation=expression[(*position)++];
        int64_t right=0;
        if(!calc_number(expression,position,&right))return 0;
        if(operation=='/'&&right==0)return 0;
        int64_t value=operation=='*'?*result*right:*result/right;
        if(value>1000000000||value< -1000000000)return -1;
        *result=value;
    }
    return 1;
}
static int calc_expression(const char *expression,int64_t *result){
    int position=0;
    int status=calc_term(expression,&position,result);
    if(status!=1)return status;
    while(expression[position]=='+'||expression[position]=='-'){
        char operation=expression[position++];
        int64_t right=0;
        status=calc_term(expression,&position,&right);
        if(status!=1)return status;
        int64_t value=operation=='+'?*result+right:*result-right;
        if(value>1000000000||value< -1000000000)return -1;
        *result=value;
    }
    return expression[position]?0:1;
}
static void calc_format(char *output,int64_t value){
    char digits[24];
    int negative=value<0,at=0,index=0;
    uint64_t number=negative?(uint64_t)(-value):(uint64_t)value;
    do{digits[at++]=(char)('0'+number%10);number/=10;}while(number&&at<23);
    if(negative)output[index++]='-';
    while(at)output[index++]=digits[--at];
    output[index]=0;
}
static void calc_input(QDesktop *q,char key){
    if(key=='C'){
        q->calc_length=0;q->calc[0]=0;
        q->answer[0]='0';q->answer[1]=0;return;
    }
    if(key=='B'){
        if(q->calc_length)q->calc[--q->calc_length]=0;
        return;
    }
    if(key=='='){
        if(!q->calc_length)return;
        int64_t result=0;
        int status=calc_expression(q->calc,&result);
        if(status!=1){
            const char *err=status<0?"OVERFLOW":"ERR";
            int n=0;
            while(err[n]){q->answer[n]=err[n];n++;}
            q->answer[n]=0;
        }else{
            calc_format(q->answer,result);
            for(int i=2;i>0;i--)for(int j=0;j<64;j++)q->calc_history[i][j]=q->calc_history[i-1][j];
            char *history=q->calc_history[0];
            int at=0;
            for(int j=0;q->calc[j]&&at<44;j++)history[at++]=q->calc[j];
            history[at++]=' ';history[at++]='=';history[at++]=' ';
            for(int j=0;q->answer[j]&&at<63;j++)history[at++]=q->answer[j];
            history[at]=0;
            if(q->calc_history_count<3)q->calc_history_count++;
        }
        q->calc_length=0;q->calc[0]=0;
        return;
    }
    if(key>='0'&&key<='9'){
        if(q->calc_length<28){
            q->calc[q->calc_length++]=key;
            q->calc[q->calc_length]=0;
        }
        return;
    }
    if(key=='+'||key=='-'||key=='*'||key=='/'){
        if(!q->calc_length){
            if(q->answer[0]!='E'&&q->answer[0]!='O'){
                int i=0;
                while(q->answer[i]&&i<20){q->calc[i]=q->answer[i];i++;}
                q->calc_length=i;q->calc[i]=0;
            }
            if(!q->calc_length&&key=='-'){
                q->calc[0]='-';q->calc[1]=0;q->calc_length=1;return;
            }
        }
        int last=q->calc_length?q->calc[q->calc_length-1]:0;
        if((last=='+'||last=='-'||last=='*'||last=='/')&&key!='-'){
            q->calc[q->calc_length-1]=key;return;
        }
        if(q->calc_length<28){
            q->calc[q->calc_length++]=key;
            q->calc[q->calc_length]=0;
        }
    }
}


typedef struct {
    char path[QFS_PATH],name[72];
    int volume,directory;
} QFileClipboard;
static QFsEntry file_copy_entries[7][QFS_ENTRIES];
static QFileClipboard file_clipboard[QFS_ENTRIES];
static int files_copy(QDesktop *q,int volume,const char *source,int target_volume,const char *target,int directory,int depth){
    if(depth>=7||!q->storage)return -1;
    if(!directory){
        return qfs_copy(q->storage,volume,source,target_volume,target);
    }
    if(volume==target_volume){
        int n=0;while(source[n]&&source[n]==target[n])n++;
        if(!source[n]&&(target[n]==0||target[n]=='\\'))return -1;
    }
    if(qfs_mkdir(q->storage,target_volume,target)!=0)return -1;
    int count=qfs_list(q->storage,volume,source,file_copy_entries[depth],QFS_ENTRIES);
    if(count<0)return -1;
    for(int i=0;i<count;i++){
        char child[QFS_PATH],dest[QFS_PATH];
        QFsEntry *entry=&file_copy_entries[depth][i];
        if(qfs_join(child,sizeof(child),source,entry->name)!=0||
           qfs_join(dest,sizeof(dest),target,entry->name)!=0)return -1;
        if(files_copy(q,volume,child,target_volume,dest,entry->directory,depth+1)!=0)return -1;
    }
    return 0;
}
static int files_remove_tree(QDesktop *q,int volume,const char *path,int directory,int depth){
    if(depth>=7)return -1;
    if(directory){
        int count=qfs_list(q->storage,volume,path,file_copy_entries[depth],QFS_ENTRIES);
        if(count<0)return -1;
        for(int i=0;i<count;i++){
            char child[QFS_PATH];
            QFsEntry *entry=&file_copy_entries[depth][i];
            if(qfs_join(child,sizeof(child),path,entry->name)!=0||
               files_remove_tree(q,volume,child,entry->directory,depth+1)!=0)return -1;
        }
    }
    return qfs_delete(q->storage,volume,path);
}
static int files_destination_exists(QDesktop *q,const char *name){
    int count=qfs_list(q->storage,q->file_volume,q->file_path,file_copy_entries[0],QFS_ENTRIES);
    if(count<0)return 1;
    for(int i=0;i<count;i++)if(files_equal(file_copy_entries[0][i].name,name))return 1;
    return 0;
}
static void files_paste(QDesktop *q){
    if(!q->file_clip_count){files_message(q,"Clipboard is empty");return;}
    if(!q->storage||q->file_location){files_message(q,"Choose a folder first");return;}
    int failures=0,completed=0,remaining=0;
    for(int item=0;item<q->file_clip_count;item++){
        QFileClipboard *clip=&file_clipboard[item];
        char target[QFS_PATH],name[72];
        int identical=clip->volume==q->file_volume;
        if(identical){
            int slash=0,length=0;
            for(int i=0;clip->path[i];i++)if(clip->path[i]=='\\')slash=i;
            while(q->file_path[length])length++;
            if(slash==0)identical=length==1&&q->file_path[0]=='\\';
            else{
                identical=length==slash;
                for(int i=0;identical&&i<slash;i++)
                    if(q->file_path[i]!=clip->path[i])identical=0;
            }
        }
        int count=0;
        for(;clip->name[count]&&count<71;count++)name[count]=clip->name[count];
        name[count]=0;
        if(files_destination_exists(q,name)){
            if(q->file_clip_cut){
                if(identical){completed++;continue;}
                failures++;goto keep;
            }
            int attempt=1,found=0;
            while(attempt<100){
                char candidate[72];int at=0;
                const char *prefix="Copy ";
                for(int i=0;prefix[i];i++)candidate[at++]=prefix[i];
                int number=attempt;
                char digits[4];int len=0;
                do{digits[len++]=(char)('0'+number%10);number/=10;}while(number&&len<3);
                while(len)candidate[at++]=digits[--len];
                candidate[at++]=' ';candidate[at++]='-';candidate[at++]=' ';
                for(int i=0;clip->name[i]&&at<71;i++)candidate[at++]=clip->name[i];
                candidate[at]=0;
                if(!files_destination_exists(q,candidate)){
                    for(int i=0;i<=at;i++){name[i]=candidate[i];}
                    found=1;break;
                }
                attempt++;
            }
            if(!found){failures++;goto keep;}
        }
        if(qfs_join(target,sizeof(target),q->file_path,name)!=0){
            failures++;goto keep;
        }
        if(files_copy(q,clip->volume,clip->path,q->file_volume,target,clip->directory,0)!=0){
            files_remove_tree(q,q->file_volume,target,clip->directory,0);
            failures++;goto keep;
        }
        if(q->file_clip_cut&&files_remove_tree(q,clip->volume,clip->path,clip->directory,0)!=0){
            failures++;goto keep;
        }
        completed++;
        continue;
keep:
        if(q->file_clip_cut)file_clipboard[remaining++]=*clip;
    }
    if(q->file_clip_cut)q->file_clip_count=remaining;
    files_refresh(q);
    if(failures)files_message(q,"Some items could not be copied or moved");
    else if(completed)files_message(q,q->file_clip_cut?"Move complete":"Copy complete");
}
static void files_clip(QDesktop *q,int cut){
    if(q->file_location||!q->storage)return;
    int count=0;
    for(int i=0;i<q->file_count;i++){
        if(!q->file_checked[i]&&!(q->file_checked_count==0&&i==q->file_selected))continue;
        QFileClipboard *clip=&file_clipboard[count];
        if(qfs_join(clip->path,sizeof(clip->path),q->file_path,q->files[i].name)!=0)continue;
        int j=0;for(;q->files[i].name[j]&&j<71;j++)clip->name[j]=q->files[i].name[j];
        clip->name[j]=0;clip->volume=q->file_volume;clip->directory=q->files[i].directory;
        count++;
    }
    if(!count){files_message(q,"Select files first");return;}
    q->file_clip_count=count;q->file_clip_cut=cut;
    files_message(q,cut?"Ready to move":"Copied to clipboard");
}
static void files_delete_selected(QDesktop *q){
    if(q->file_location==1||!q->storage)return;
    if(!q->file_checked_count&&q->file_selected<0){files_message(q,"Select items first");return;}
    if(q->file_confirm!=1){
        q->file_confirm=1;
        files_message(q,q->file_location==2||q->file_volume!=q->storage->home_index?
            "Select Delete again to permanently remove":"Select Delete again to move to Recycle Bin");
        return;
    }
    int failures=0;
    for(int i=0;i<q->file_count;i++)if(q->file_checked[i]||(!q->file_checked_count&&i==q->file_selected)){
        if(q->file_location==2){
            if(qtrash_remove(q->storage,q->trash_entries[i].id)!=0)failures++;
        }else{
            char path[QFS_PATH];
            if(qfs_join(path,sizeof(path),q->file_path,q->files[i].name)!=0||
               (q->file_volume==q->storage->home_index?
               qtrash_put(q->storage,q->file_volume,path,q->files[i].directory):
               files_remove_tree(q,q->file_volume,path,q->files[i].directory,0))!=0)failures++;
        }
    }
    int bin=q->file_location==2;
    files_refresh(q);
    files_message(q,failures?"Some selected items could not be deleted":
        bin||q->file_volume!=q->storage->home_index?"Permanently deleted":"Moved to Recycle Bin");
}
static void files_shortcut(QDesktop *q,int index){
    if(!q->storage)return;
    if(index==7){files_this_pc(q,1);return;}
    if(index==8){q->file_location=2;q->file_menu=0;q->file_submenu=0;files_refresh(q);files_history(q);return;}
    if(index==1){q_open(q,Q_PHOTOS);return;}
    int volume=q->storage->home_index;
    char root[QFS_PATH];
    if(qfs_profile_path(q->storage,"",root,sizeof(root))!=0)return;
    if(index==0){files_go(q,volume,root,1);return;}
    const char *folders[]={"","","Desktop","Documents","Pictures","Music","Videos"};
    char target[QFS_PATH];
    if(qfs_join(target,sizeof(target),root,folders[index])!=0)return;
    QFsEntry item[1];
    if(qfs_list(q->storage,volume,target,item,1)<0)
        if(qfs_mkdir(q->storage,volume,target)!=0){files_message(q,"Cannot open this folder");return;}
    files_go(q,volume,target,1);
}
static int files_click(QDesktop *q,QWindow *w,int x,int y,int focused){
    int left=w->x+15,top=w->y+60,full=w->w-30;
    int side=q->file_nav?188:0;
    int main_x=left+side+9,main_w=full-side-16-(q->file_details?188:0);
    int canvas_y=top+109,footer=w->y+w->h-31;
    if(!focused)q->file_click_index=-1;
    if(q->file_operation){
        int dw=clamp(full-96,310,530),dx=left+(full-dw)/2,dy=top+135;
        if(hit(x,y,dx,dy,dw,154))return 1;
        q->file_operation=0;q->dirty=1;return 1;
    }
    if(q->file_menu){
        int mx=left+(q->file_menu==1?5:q->file_menu==2?407:q->file_menu==3?500:595);
        int my=top+94,group=q->file_menu;
        if(group==3)mx=clamp(mx,left,left+full-439);
        if(group==4)mx=clamp(mx,left,left+full-193);
        if(group==3&&q->file_submenu&&hit(x,y,mx+228,my+155,202,187)){
            int option=(y-(my+155)-6)/35;
            if(option==0)q->file_nav=!q->file_nav;
            if(option==1)q->file_compact=!q->file_compact;
            if(option==2)q->file_checkboxes=!q->file_checkboxes;
            if(option==3)q->file_extensions=!q->file_extensions;
            if(option==4)q->file_show_hidden=!q->file_show_hidden;
            q->file_submenu=0;q->file_menu=0;
            files_rebuild(q);return 1;
        }
        int width=group==1?190:group==2?178:group==3?226:188;
        int height=group==1?82:group==2?222:group==3?327:187;
        if(hit(x,y,mx,my,width,height)){
            int option=(y-my-6)/35;
            if(group==1){
                q->file_menu=0;
                if(option>=0&&option<2)files_begin(q,option+1);
            }else if(group==2){
                q->file_menu=0;
                if(option>=0&&option<4)q->file_sort=option;
                if(option==4)q->file_desc=0;
                if(option==5)q->file_desc=1;
                files_rebuild(q);
            }else if(group==3){
                if(option==8){q->file_submenu=!q->file_submenu;q->dirty=1;}
                else{
                    if(option>=0&&option<8)q->file_view=option;
                    q->file_submenu=0;q->file_menu=0;
                    files_rebuild(q);
                }
            }else{
                q->file_menu=0;
                if(option==0||option==1||option==2){
                    int all=option==0?1:option==1?0:2;
                    q->file_checked_count=0;
                    for(int i=0;i<q->file_count;i++){
                        int visible=0;
                        for(int k=0;k<q->file_visible_count;k++)if(q->file_order[k]==i){visible=1;break;}
                        if(!visible){q->file_checked[i]=0;continue;}
                        q->file_checked[i]=all==2?!q->file_checked[i]:all;
                        if(q->file_checked[i])q->file_checked_count++;
                    }
                    if(all==0)q->file_selected=-1;
                }else if(option==3)q->file_details=1;
                else if(option==4)files_refresh(q);
                q->dirty=1;
            }
            return 1;
        }
        q->file_menu=0;q->dirty=1;
        return 1;
    }
    if(hit(x,y,left,top,full,46)){
        if(hit(x,y,left,top+4,35,37)){files_back(q,0);return 1;}
        if(hit(x,y,left+40,top+4,35,37)){files_back(q,1);return 1;}
        if(hit(x,y,left+80,top+4,35,37)){files_parent(q);return 1;}
        if(hit(x,y,left+120,top+4,70,37)){files_refresh(q);return 1;}
        int search_w=clamp(full/4,144,225),address_x=left+195,address_w=full-203-search_w-8;
        if(hit(x,y,address_x,top+6,address_w,34)){
            q->file_address_active=1;q->file_search_active=0;q->file_address_length=0;
            for(int i=0;q->file_path[i]&&i<QFS_PATH-1;i++)q->file_address[q->file_address_length++]=q->file_path[i];
            q->file_address[q->file_address_length]=0;q->dirty=1;return 1;
        }
        if(hit(x,y,left+full-search_w,top+6,search_w,34)){
            q->file_search_active=1;q->file_address_active=0;q->dirty=1;return 1;
        }
        q->file_address_active=0;q->file_search_active=0;return 1;
    }
    if(hit(x,y,left,top+51,full,47)){
        if(q->file_location==2){
            if(hit(x,y,left+5,top+57,100,33)){
                if(q->file_selected>=0)files_open(q,q->file_selected);
                else files_message(q,"Select an item to restore");
                return 1;
            }
            if(hit(x,y,left+113,top+57,138,33)){files_delete_selected(q);return 1;}
            if(hit(x,y,left+259,top+57,126,33)){
                if(q->file_confirm!=2){q->file_confirm=2;files_message(q,"Select Empty Bin again to confirm");return 1;}
                int ok=qtrash_empty(q->storage)==0;
                files_refresh(q);files_message(q,ok?"Recycle Bin emptied":"Could not empty Recycle Bin");
                return 1;
            }
            if(hit(x,y,left+393,top+57,95,33)){files_refresh(q);return 1;}
            return 1;
        }
        const int tx[]={5,83,131,186,249,326,407,500,595},tw[]={74,45,52,55,75,76,85,88,45};
        for(int i=0;i<9;i++)if(tx[i]+tw[i]<=full&&hit(x,y,left+tx[i],top+57,tw[i],33)){
            if(i==0||i==6||i==7||i==8){q->file_menu=i==0?1:i==6?2:i==7?3:4;q->file_submenu=0;q->dirty=1;}
            else if(i==1)files_clip(q,1);
            else if(i==2)files_clip(q,0);
            else if(i==3)files_paste(q);
            else if(i==4)files_begin(q,3);
            else if(i==5)files_delete_selected(q);
            return 1;
        }
        return 1;
    }
    if(q->file_nav&&hit(x,y,left,top+103,182,footer-(top+103))){
        int sy=top+103;
        for(int i=0;i<9;i++){
            int py=sy+12+i*36+(i>=2?12:0)+(i>=7?18:0);
            if(hit(x,y,left+4,py-2,176,34)){files_shortcut(q,i);return 1;}
        }
        if(q->storage)for(int i=0;i<q->storage->count&&i<5;i++){
            int py=sy+371+i*35;
            if(py+30<footer&&hit(x,y,left+4,py,176,33)){files_go(q,i,"\\",1);return 1;}
        }
        return 1;
    }
    if(q->file_location==1){
        if(q->storage)for(int i=0;i<q->storage->count;i++){
            int dx=main_x+8+(i%2)*225,dy=canvas_y+53+(i/2)*90;
            if(dx+210>main_x+main_w)break;
            if(hit(x,y,dx,dy,210,76)){files_go(q,i,"\\",1);return 1;}
        }
        return 1;
    }
    if(q->file_view==5)canvas_y+=37;
    int cols=files_columns(q,w),item_h=files_item_height(q);
    int pitch=cols==1?main_w-10:(main_w-14)/cols,page=files_page(q,w);
    for(int slot=0;slot<page;slot++){
        int shown=q->file_scroll+slot;
        if(shown>=q->file_visible_count)break;
        int ix=main_x+6+(slot%cols)*pitch,iy=canvas_y+(slot/cols)*item_h;
        if(iy+item_h>footer-5)break;
        if(hit(x,y,ix,iy,pitch-5,item_h-4)){
            int selected=q->file_order[shown];
            if(q->file_checkboxes&&hit(x,y,ix+4,iy+4,20,20)){
                q->file_checked[selected]=!q->file_checked[selected];
                q->file_checked_count+=q->file_checked[selected]?1:-1;
                q->file_selected=selected;q->file_click_index=-1;q->dirty=1;return 1;
            }
            files_select(q,selected);return 1;
        }
    }
    if(hit(x,y,main_x,canvas_y,main_w,footer-canvas_y)){
        q->file_selected=-1;q->file_click_index=-1;q->file_checked_count=0;
        for(int i=0;i<QFS_ENTRIES;i++)q->file_checked[i]=0;
        q->dirty=1;return 1;
    }
    return 0;
}
void q_click(QDesktop *q,int x,int y){
    q->hover_only=0;
    if(q->locked){
        int bx=q->width/2-225,by=q->height/2-180;
        if(q->profile_count>1&&hit(x,y,bx+33,by+130,390,42)){
            q->profile_target=(q->profile_target+(x<bx+225?q->profile_count-1:1))%q->profile_count;
            q->pin_input_len=0;q->pin_input[0]=0;profile_message(q,"");return;
        }
        if(hit(x,y,bx+35,by+250,380,49))profile_unlock(q);
        return;
    }
    if(q->desktop_view){
        int vx=clamp((q->width-522)/2,5,q->width-522),vy=dock_y(q)-173;
        for(int i=0;i<3;i++)if(hit(x,y,vx+16+i*169,vy+43,155,98)){
            if(y>=vy+101&&q->active>=0)q_move_to_desktop(q,q->active,i);
            else q_switch_desktop(q,i);
            return;
        }
        q->desktop_view=0;q->dirty=1;
    }
    if(q->snap_menu_app>=0){
        QWindow *sw=&q->windows[q->snap_menu_app];
        int sx=snap_menu_left(q,sw),sy=snap_menu_top(q,sw);
        for(int i=0;i<6;i++)if(hit(x,y,sx+11+(i%3)*81,sy+37+(i/3)*43,74,37)){
            q_snap_window(q,q->snap_menu_app,i+1);return;
        }
        q->snap_menu_app=-1;q->dirty=1;
    }
    int dock=q_dock_hit(q,x,y);
    if(dock==13){
        q->desktop_view=!q->desktop_view;q->snap_menu_app=-1;q->dirty=1;return;
    }
    if(dock==10){
        int was=q->menu;
        close_popups(q);
        q->menu=!was;
        q->menu_hover=-1;
        if(q->animations_enabled)q->start_pulse=256;
        begin_scene_tween(q,q->menu?7:6);
        q->dirty=1;
        return;
    }
    if(dock>=0&&dock<Q_DOCK_APPS){
        QWindow *w=&q->windows[dock];
        if(w->open&&!w->minimized&&q->active==dock)minimize_window(q,dock);
        else q_open(q,dock);
        return;
    }
    if(dock==11){int was=q->notifications;close_popups(q);q->notifications=!was;begin_scene_tween(q,q->notifications?5:6);q->dirty=1;return;}
    if(dock==12){int was=q->quick;close_popups(q);q->quick=!was;begin_scene_tween(q,q->quick?5:6);q->dirty=1;return;}
    if(q->menu){
        int mx=menu_x(q),my=menu_y(q);
        if(hit(x,y,mx,my,510,434)){
            for(int slot=0;slot<Q_APPS;slot++){
                int app=menu_app_at(q,slot);
                if(app<0)break;
                if(hit(x,y,mx+26+slot%4*116,my+112+slot/4*103,107,94)){q_open(q,app);return;}
            }
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
        if(hit(x,y,bx+19,by+80,298,72)){q_set_theme(q,!q->theme);return;}
        if(hit(x,y,bx+19,by+164,298,68)){
            if(x>=bx+178&&x<=bx+295)q_set_accent(q,clamp((x-(bx+162))/32,0,3));
            else q_set_accent(q,(q->accent+1)%4);
            q->dirty=1;return;
        }
        if(hit(x,y,bx+19,by+243,298,43)){q_open(q,Q_SETTINGS);return;}
        if(hit(x,y,bx,by,336,293))return;
        q->quick=0;q->dirty=1;
    }
    int chosen=-1;
    for(int i=0;i<Q_APPS;i++)if(q->windows[i].open&&!q->windows[i].minimized&&q->windows[i].desktop==q->desktop&&hit(x,y,q->windows[i].x,q->windows[i].y,q->windows[i].w,q->windows[i].h)&&
        (chosen<0||q->windows[i].z>q->windows[chosen].z))chosen=i;
    if(chosen>=0&&chosen<Q_APPS){
        QWindow *w=&q->windows[chosen];
        if(hit(x,y,w->x+w->w-59,w->y+13,36,29)){close_window(q,chosen);return;}
        if(hit(x,y,w->x+w->w-108,w->y+13,36,29)){maximize_window(q,chosen);return;}
        if(hit(x,y,w->x+w->w-157,w->y+13,36,29)){minimize_window(q,chosen);return;}
        int files_was_focused=q->active==Q_FILES;
        q_open(q,chosen);
        if((!w->maximized||q->pressed)&&hit(x,y,w->x,w->y,w->w-175,55)){
            q->drag=chosen;q->drag_x=x-w->x;q->drag_y=y-w->y;
            q->drag_start_x=x;q->drag_start_y=y;q->drag_moved=0;
            return;
        }
        int bx=w->x+32,by=w->y+60;
        if(chosen==Q_STORE){
            int cw=w->w-64;
            if(q->store_pending){
                if(hit(x,y,bx+18,by+246,140,40)){store_install(q);return;}
                if(hit(x,y,bx+169,by+246,130,40)){q->store_pending=0;store_message(q,"Installation cancelled");return;}
                return;
            }
            if(hit(x,y,bx+cw-196,by+13,192,37)){
                q->store_view=!q->store_view;q->store_confirm_remove=-1;
                if(q->store_view)store_refresh(q);
                q->dirty=1;return;
            }
            if(q->store_view){
                for(int i=0;i<q->store_installed_count&&i<5;i++)
                    if(hit(x,y,bx+cw-139,by+78+i*59+8,126,38)){store_remove(q,i);return;}
                q->store_confirm_remove=-1;q->dirty=1;return;
            }
            const int entries[8]={Q_NOTES,Q_CALCULATOR,Q_FILES,Q_TERMINAL,Q_SETTINGS,Q_PHOTOS,Q_MEDIA,Q_ABOUT};
            if(hit(x,y,bx,by+302,140,45)){q->store_page=1-q->store_page;q->dirty=1;return;}
            for(int i=0;i<4;i++)if(hit(x,y,bx+(i%2)*(w->w>650?230:190),by+78+(i/2)*118,174,102)){
                q_open(q,entries[q->store_page*4+i]);return;
            }
        }else if(chosen==Q_SETTINGS){
            settings_click(q,w,x,y);
        }else if(chosen==Q_TASKS){
            tasks_click(q,w,x,y);
        }else if(chosen==Q_FILES){
            files_click(q,w,x,y,files_was_focused);
        }else if(chosen==Q_NOTES){
            int cw=w->w-64;
            if(hit(x,y,bx+cw-98,by+2,96,28)){files_save_notes(q);return;}
            if(hit(x,y,bx+cw-200,by+2,94,28)){
                if(q->note_dirty){files_message(q,"Save changes before New");return;}
                q->note_length=0;q->notes[0]=0;q->note_cursor=0;q->note_scroll=0;
                q->note_path[0]=0;q->note_volume=-1;q->file_status[0]=0;q->dirty=1;return;
            }
        }else if(chosen==Q_PHOTOS||chosen==Q_MEDIA){
            int view_h=w->h-223;
            if(view_h<110)view_h=110;
            if(hit(x,y,bx+w->w-64-176,by+60+view_h,176,38)){q_open(q,Q_FILES);return;}
            if(chosen==Q_PHOTOS){
                for(int i=0;i<3;i++)if(hit(x,y,bx+i*83,by+60+view_h,78,38)){q->photo_zoom=i;q->dirty=1;return;}
                const int offsets[]={0,88,161,277,365,458};
                for(int i=0;i<6;i++)if(hit(x,y,bx+offsets[i],by+104+view_h,i==5?143:i==2?107:i==1?69:84,36)){
                    if(i==0)q->photo_rotation=(q->photo_rotation+1)&3;
                    if(i==1)q->photo_flipped=!q->photo_flipped;
                    if(i==2)q_photo_save_bmp(q);
                    if(i==3)q_media_next(q,-1);
                    if(i==4)q_media_next(q,1);
                    if(i==5)media_message(q,q_wallpaper_photo(q)==0?"Wallpaper saved to this account":"Could not save wallpaper");
                    q->dirty=1;return;
                }
            }
            if(chosen==Q_MEDIA){
                const int offsets[]={0,88,165,245,311,378};
                for(int i=0;i<6;i++)if(hit(x,y,bx+offsets[i],by+104+view_h,76,36)){
                    if(i==0)q_media_next(q,-1);
                    if(i==1)q_media_next(q,1);
                    if(i==2)q->media_repeat=!q->media_repeat;
                    if(i==3)q->media_volume_level=clamp(q->media_volume_level-10,0,100);
                    if(i==4)q->media_volume_level=clamp(q->media_volume_level+10,0,100);
                    if(i==5)q->media_muted=!q->media_muted;
                    q->dirty=1;return;
                }
                int track=w->w-64-300;
                if(track>60&&hit(x,y,bx+110,by+65+view_h,track+14,20)){
                    uint64_t duration=qm_duration(&q->media);
                    if(duration){
                        int pos=clamp(x-(bx+117),0,track);
                        qm_seek(&q->media,duration*(uint64_t)pos/(uint64_t)track);
                        q->dirty=1;return;
                    }
                }
                if(hit(x,y,bx,by+60+view_h,101,38)){
                    if(!q->media.playing&&q->media.elapsed_us>=qm_duration(&q->media))qm_seek(&q->media,0);
                    q->media.playing=!q->media.playing;q->dirty=1;return;
                }
            }
        }else if(chosen==Q_CALCULATOR){
            const char *keys="789/456*123-C0=+";
            for(int i=0;i<16;i++)if(hit(x,y,bx+(i%4)*82,by+128+(i/4)*57,73,49)){
                calc_input(q,keys[i]);q->dirty=1;break;
            }
        }
        return;
    }
    q->active=-1;q->dirty=1;
}
void q_press(QDesktop *q,int x,int y){
    q->pressed=1;q_click(q,x,y);
    if(q->drag>=0){q->scene_tween_active=0;q->scene_tween_pending=0;}
}
void q_release(QDesktop *q){
    q->pressed=0;
    if(q->drag>=0){
        int app=q->drag;
        q->drag=-1;
        if(q->drag_moved&&!q->windows[app].maximized){
            int zone=0;
            if(q->mouse_x<=16)zone=q->mouse_y<70?3:q->mouse_y>q->height-70?5:1;
            else if(q->mouse_x>=q->width-17)zone=q->mouse_y<70?4:q->mouse_y>q->height-70?6:2;
            if(zone)q_snap_window(q,app,zone);
            else if(q->mouse_y<=16)maximize_window(q,app);
        }
        q->dirty=1;q->drag_cache_valid=0;q->glass_cached_app=-1;
    }
    q->drag=-1;
}
void q_move(QDesktop *q,int x,int y){
    q->mouse_x=clamp(x,0,q->width-1);q->mouse_y=clamp(y,0,q->height-1);
    if(q->pressed&&q->drag>=0){
        QWindow *w=&q->windows[q->drag];
        int dx=q->mouse_x-q->drag_start_x,dy=q->mouse_y-q->drag_start_y;
        if(dx>8||dx< -8||dy>8||dy< -8)q->drag_moved=1;
        if(w->maximized||w->snap_zone){
            if(!q->drag_moved)return;
            int anchor_x=(int)((int64_t)(q->drag_start_x-w->x)*w->restore_w/w->w);
            w->w=w->restore_w;w->h=w->restore_h;
            w->x=clamp(q->mouse_x-anchor_x,0,q->width-w->w);
            w->y=12;w->maximized=0;w->snap_zone=0;
            q->drag_x=anchor_x;q->drag_y=28;
            q->dirty=1;q->hover_only=0;
            q->drag_cache_valid=0;q->glass_cached_app=-1;
        }
        int new_x=clamp(q->mouse_x-q->drag_x,0,q->width-w->w);
        int max_y=q->height-44;
        if(max_y<12)max_y=12;
        int new_y=clamp(q->mouse_y-q->drag_y,12,max_y);
        if(new_x!=w->x||new_y!=w->y){w->x=new_x;w->y=new_y;q->dirty=1;}
    }
    if(q->drag>=0)return;
    int snap=-1;
    for(int i=0;i<Q_APPS;i++)if(q->windows[i].open&&!q->windows[i].minimized&&q->windows[i].desktop==q->desktop){
        QWindow *w=&q->windows[i];
        if(hit(q->mouse_x,q->mouse_y,w->x+w->w-108,w->y+13,36,29)&&(snap<0||w->z>q->windows[snap].z))snap=i;
        if(i==q->snap_menu_app&&hit(q->mouse_x,q->mouse_y,snap_menu_left(q,w),snap_menu_top(q,w),256,133))snap=i;
    }
    if(snap!=q->snap_menu_app){q->snap_menu_app=snap;q->dirty=1;q->hover_only=0;}
    int hover=-1;
    int dock=q_dock_hit(q,q->mouse_x,q->mouse_y);
    int start_hover=dock==10;
    if(dock>=0&&dock<Q_DOCK_APPS)hover=dock;
    int menu_hover=-1;
    if(q->menu){
        int mx=menu_x(q),my=menu_y(q);
        for(int slot=0;slot<Q_APPS;slot++){
            int app=menu_app_at(q,slot);
            if(app<0)break;
            if(hit(q->mouse_x,q->mouse_y,mx+26+(slot%4)*116,my+112+(slot/4)*103,107,94)){
                menu_hover=app;break;
            }
        }
    }
    if(hover!=q->hover||start_hover!=q->start_hover||menu_hover!=q->menu_hover){
        if(!q->dirty&&q->drag<0)q->hover_only=1;
        else q->hover_only=0;
        q->start_hover=start_hover;q->hover=hover;q->menu_hover=menu_hover;q->dirty=1;
    }
}
void q_key(QDesktop *q,uint16_t ch,uint16_t scan){
    q->hover_only=0;
    if(q->locked){
        if(ch=='\r'){profile_unlock(q);return;}
        if(ch==8&&q->pin_input_len){q->pin_input[--q->pin_input_len]=0;q->dirty=1;return;}
        if((scan==3||scan==4)&&q->profile_count>1){
            q->profile_target=(q->profile_target+(scan==3?1:q->profile_count-1))%q->profile_count;
            q->pin_input_len=0;q->pin_input[0]=0;q->dirty=1;return;
        }
        if(ch>=32&&ch<=126&&q->pin_input_len<24){
            q->pin_input[q->pin_input_len++]=(char)ch;q->pin_input[q->pin_input_len]=0;q->dirty=1;
        }
        return;
    }
    if(q->active==Q_SETTINGS&&q->profile_edit){
        if(ch==27){q->profile_edit=0;q->dirty=1;return;}
        if(ch=='\r'){profile_edit_submit(q);return;}
        if(ch==8&&q->profile_input_len)q->profile_input[--q->profile_input_len]=0;
        else if(ch>=32&&ch<=126&&q->profile_input_len<24){
            q->profile_input[q->profile_input_len++]=(char)ch;q->profile_input[q->profile_input_len]=0;
        }
        q->dirty=1;return;
    }
    if(ch==12){q_lock(q);return;}
    if(q->active==Q_FILES&&q->file_operation){
        if(ch==27||scan==23){q->file_operation=0;q->dirty=1;return;}
        if(ch==8){if(q->file_input_length)q->file_input[--q->file_input_length]=0;q->dirty=1;return;}
        if(ch=='\r'){files_finish(q);q->dirty=1;return;}
        if(ch>=32&&ch<=126&&ch!='/'&&ch!='\\'&&ch!=':'&&q->file_input_length<70){q->file_input[q->file_input_length++]=(char)ch;q->file_input[q->file_input_length]=0;q->dirty=1;}
        return;
    }
    if(q->active==Q_FILES){
        if(q->file_search_active||q->file_address_active){
            int address=q->file_address_active;
            char *value=address?q->file_address:q->file_search;
            int *length=address?&q->file_address_length:&q->file_search_length;
            int limit=address?QFS_PATH-1:(int)sizeof(q->file_search)-1;
            if(scan==23||ch==27){
                q->file_search_active=0;q->file_address_active=0;q->dirty=1;return;
            }
            if(ch=='\r'){
                if(address)files_go(q,q->file_volume,*value?value:"\\",1);
                else q->file_search_active=0;
                q->dirty=1;return;
            }
            if(ch==8){
                if(*length>0)value[--*length]=0;
            }else if(ch>=32&&ch<=126&&*length<limit){
                value[(*length)++]=(char)ch;value[*length]=0;
            }
            if(!address)files_rebuild(q);
            q->dirty=1;return;
        }
        if(q->file_menu&&(scan==23||ch==27)){q->file_menu=0;q->dirty=1;return;}
        if(ch==1){
            q->file_checked_count=0;
            for(int i=0;i<q->file_count;i++)q->file_checked[i]=0;
            for(int j=0;j<q->file_visible_count;j++){
                int i=q->file_order[j];q->file_checked[i]=1;q->file_checked_count++;
            }
            q->dirty=1;return;
        }
        if(ch==3){files_clip(q,0);return;}
        if(ch==24){files_clip(q,1);return;}
        if(ch==22){files_paste(q);return;}
        if(scan==8){files_delete_selected(q);return;}
    }
    if(ch==9){cycle_window(q);return;}
    if((scan==23||ch==27)&&q->desktop_view){q->desktop_view=0;q->dirty=1;return;}
    if(scan==24){q_switch_desktop(q,(q->desktop+2)%3);return;}
    if(scan==25){q_switch_desktop(q,(q->desktop+1)%3);return;}
    if((scan==23||ch==27)&&q->active==Q_SETTINGS&&q->settings_search_length&&!q->menu&&!q->notifications&&!q->quick){
        q->settings_search_length=0;q->settings_search[0]=0;q->dirty=1;return;
    }
    if(scan==23||ch==27){int had=q->menu||q->notifications||q->quick;close_popups(q);if(had)begin_scene_tween(q,6);q->dirty=1;return;}
    if(scan>=20&&scan<=22){
        int app=q->active;
        if(app>=0&&app<Q_APPS&&q->windows[app].open){
            if(scan==20)minimize_window(q,app);
            else if(scan==21)maximize_window(q,app);
            else close_window(q,app);
        }
        return;
    }
    if(scan>=11&&scan<=16){q_open(q,(int)scan-11);return;}
    if(scan==17){
        int was=q->menu;
        close_popups(q);
        q->menu=!was;
        begin_scene_tween(q,q->menu?7:6);
        q->dirty=1;return;
    }
    if(scan==18){q->notifications=!q->notifications;q->menu=0;q->quick=0;begin_scene_tween(q,q->notifications?5:6);q->dirty=1;return;}
    if(scan==19){q->quick=!q->quick;q->menu=0;q->notifications=0;begin_scene_tween(q,q->quick?5:6);q->dirty=1;return;}
    if(q->menu){
        if(ch=='\r'){
            int app=q->menu_hover>=0&&menu_match(q,q->menu_hover)?q->menu_hover:menu_app_at(q,0);
            if(app>=0)q_open(q,app);
            return;
        }
        if(ch==8){
            if(q->menu_search_length)q->menu_search[--q->menu_search_length]=0;
            q->menu_hover=-1;q->dirty=1;return;
        }
        if(scan==1||scan==2){
            int slot=menu_slot_of(q,q->menu_hover);
            int count=0;
            while(menu_app_at(q,count)>=0)count++;
            if(count){
                slot=scan==2?(slot+1)%count:(slot<=0?count-1:slot-1);
                q->menu_hover=menu_app_at(q,slot);q->dirty=1;
            }
            return;
        }
        if(ch>='1'&&ch<'1'+Q_APPS&&!q->menu_search_length){q_open(q,ch-'1');return;}
        if(ch>=32&&ch<=126&&q->menu_search_length<(int)sizeof(q->menu_search)-1){
            q->menu_search[q->menu_search_length++]=(char)ch;
            q->menu_search[q->menu_search_length]=0;
            q->menu_hover=-1;q->dirty=1;
        }
        return;
    }
    if(q->active==Q_SETTINGS){
        if(ch==8&&q->settings_search_length){
            q->settings_search[--q->settings_search_length]=0;
            q->dirty=1;return;
        }
        if(ch==27&&q->settings_search_length){
            q->settings_search_length=0;q->settings_search[0]=0;q->dirty=1;return;
        }
        if(ch=='\r'&&q->settings_search_length){
            for(int i=0;i<Q_SETTINGS_PAGES;i++)if(settings_result(q,i)){settings_navigate(q,i);break;}
            return;
        }
        if(ch>=32&&ch<=126&&q->settings_search_length<(int)sizeof(q->settings_search)-1){
            q->settings_search[q->settings_search_length++]=(char)ch;
            q->settings_search[q->settings_search_length]=0;
            q->dirty=1;return;
        }
    }
    if(q->active==Q_NOTES){
        if(ch==19){files_save_notes(q);return;}
        if(scan==3&&q->note_cursor<q->note_length){q->note_cursor++;q->dirty=1;return;}
        if(scan==4&&q->note_cursor>0){q->note_cursor--;q->dirty=1;return;}
        if(scan==1||scan==2){notes_vertical(q,scan==1?-1:1);return;}
        if(scan==5){while(q->note_cursor>0&&q->notes[q->note_cursor-1]!='\n')q->note_cursor--;q->dirty=1;return;}
        if(scan==6){while(q->note_cursor<q->note_length&&q->notes[q->note_cursor]!='\n')q->note_cursor++;q->dirty=1;return;}
        if(scan==8){notes_delete(q,1);return;}
    }
    if(ch=='\r'){
        if(q->active==Q_FILES){
            q->file_click_index=-1;
            files_open(q,q->file_selected);return;
        }
        if(q->active==Q_CALCULATOR){calc_input(q,'=');q->dirty=1;return;}
        if(q->active==Q_TERMINAL){
            if(q->command_length)terminal_run(q);
            q->command_length=0;q->command[0]=0;
        }else if(q->active==Q_NOTES)notes_insert(q,'\n');
        q->dirty=1;return;
    }
    if(ch==8){
        if(q->active==Q_FILES){files_parent(q);return;}
        if(q->active==Q_NOTES)notes_delete(q,0);
        else if(q->active==Q_TERMINAL&&q->command_length)q->command[--q->command_length]=0;
        else if(q->active==Q_CALCULATOR)calc_input(q,'B');
        q->dirty=1;return;
    }
    if(q->active==Q_FILES&&(scan==1||scan==2||scan==3||scan==4||scan==5||scan==6)){
        int slot=-1;
        for(int i=0;i<q->file_visible_count;i++)
            if(q->file_order[i]==q->file_selected){slot=i;break;}
        int cols=files_columns(q,&q->windows[Q_FILES]);
        if(scan==5)slot=0;
        else if(scan==6)slot=q->file_visible_count-1;
        else slot+=scan==1?-cols:scan==2?cols:scan==3?1:-1;
        if(q->file_visible_count>0){
            slot=clamp(slot,0,q->file_visible_count-1);
            q->file_selected=q->file_order[slot];q->file_checked_count=1;
            for(int i=0;i<q->file_count;i++)q->file_checked[i]=i==q->file_selected;
            int page=files_page(q,&q->windows[Q_FILES]);
            if(slot<q->file_scroll)q->file_scroll=(slot/cols)*cols;
            if(slot>=q->file_scroll+page)q->file_scroll=((slot/cols)-(page/cols)+1)*cols;
            q->file_click_index=-1;q->dirty=1;
        }
        return;
    }
    if(ch<32||ch>126)return;
    if(q->menu&&ch>='1'&&ch<'1'+Q_APPS){q_open(q,ch-'1');return;}
    if(q->active==Q_NOTES){notes_insert(q,(char)ch);}
    else if(q->active==Q_TERMINAL&&q->command_length<120){q->command[q->command_length++]=(char)ch;q->command[q->command_length]=0;q->dirty=1;}
    else if(q->active==Q_CALCULATOR){calc_input(q,(char)ch);q->dirty=1;}
}
