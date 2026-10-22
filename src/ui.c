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
    int inv=256-alpha;
    return rgb(((a>>16&255)*inv+(b>>16&255)*alpha)>>8,
               ((a>>8&255)*inv+(b>>8&255)*alpha)>>8,
               ((a&255)*inv+(b&255)*alpha)>>8);
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
static uint32_t *glass_source,*glass_horizontal,*glass_final,*glass_cached,*glass_cached_full;
static uint32_t *scene_from,*scene_to,*scene_back;
void q_ui_set_scratch(uint32_t *storage){
    glass_source=storage;
    glass_horizontal=glass_source+GLASS_SIDE*GLASS_ROWS;
    glass_final=glass_horizontal+GLASS_SIDE*GLASS_ROWS;
    glass_cached=glass_final+GLASS_SIDE*GLASS_ROWS;
    glass_cached_full=glass_cached+GLASS_SIDE*GLASS_ROWS;
    scene_from=glass_cached_full+Q_MAX_W*Q_MAX_H;
    scene_to=scene_from+Q_MAX_W*Q_MAX_H;
    scene_back=scene_to+Q_MAX_W*Q_MAX_H;
}
#else
static uint32_t glass_source[GLASS_SIDE * GLASS_ROWS];
static uint32_t glass_horizontal[GLASS_SIDE * GLASS_ROWS];
static uint32_t glass_final[GLASS_SIDE * GLASS_ROWS];
static uint32_t glass_cached[GLASS_SIDE * GLASS_ROWS];
static uint32_t glass_cached_full[Q_MAX_W * Q_MAX_H];

static uint32_t scene_from[Q_MAX_W*Q_MAX_H];
static uint32_t scene_to[Q_MAX_W*Q_MAX_H];
static uint32_t scene_back[Q_MAX_W*Q_MAX_H];
void q_ui_set_scratch(uint32_t *storage){(void)storage;}
#endif
static int glass_cache_cols,glass_cache_rows;
static const int glass_weight[7]={1,2,3,4,3,2,1};
static void glass_filter(QDesktop *q,int sx,int sy,int cols,int rows){
    for(int yy=0;yy<rows;yy++){
        int y=clamp(sy+yy*GLASS_SCALE,0,q->height-1);
        int y1=clamp(y+2,0,q->height-1);
        for(int xx=0;xx<cols;xx++){
            int x=clamp(sx+xx*GLASS_SCALE,0,q->width-1);
            int x1=clamp(x+2,0,q->width-1);
            const uint32_t *source=q->render_topbar?q->wallpaper:q->pixels;
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
        int r=0,g=0,b=0;
        for(int k=-3;k<=3;k++){
            uint32_t c=glass_source[(size_t)yy*cols+clamp(xx+k,0,cols-1)];
            int weight=glass_weight[k+3];
            r+=(int)(c>>16&255)*weight;
            g+=(int)(c>>8&255)*weight;
            b+=(int)(c&255)*weight;
        }
        glass_horizontal[(size_t)yy*cols+xx]=rgb(r/16,g/16,b/16);
    }
    for(int yy=0;yy<rows;yy++)for(int xx=0;xx<cols;xx++){
        int r=0,g=0,b=0;
        for(int k=-3;k<=3;k++){
            uint32_t c=glass_horizontal[(size_t)clamp(yy+k,0,rows-1)*cols+xx];
            int weight=glass_weight[k+3];
            r+=(int)(c>>16&255)*weight;
            g+=(int)(c>>8&255)*weight;
            b+=(int)(c&255)*weight;
        }
        glass_final[(size_t)yy*cols+xx]=rgb(r/16,g/16,b/16);
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
    return rgb(((int)(a>>16&255)*w00+(int)(b>>16&255)*w10+(int)(c>>16&255)*w01+(int)(d>>16&255)*w11)/16,
        ((int)(a>>8&255)*w00+(int)(b>>8&255)*w10+(int)(c>>8&255)*w01+(int)(d>>8&255)*w11)/16,
        ((int)(a&255)*w00+(int)(b&255)*w10+(int)(c&255)*w01+(int)(d&255)*w11)/16);
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
    int bw=q->width;
    for(int py=y0;py<y1;py++)for(int px=x0;px<x1;px++){
        int dx=px-x,dy=py-y;
        int cover=round_cover(dx,dy,w,h,radius);
        if(!cover)continue;
        size_t at=(size_t)py*bw+px;
        uint32_t backdrop=cached?glass_cached_full[at]:glass_fetch(glass_final,px,py,sx,sy,cols,rows);
        int sheen=dy<h/2?18*(h/2-dy)/(h/2+1):0;
        int material=clamp(alpha-44+sheen,12,190);
        uint32_t c=blend(backdrop,tint,material);
        int inner=round_cover(dx-1,dy-1,w-2,h-2,radius>1?radius-1:0);
        int rim=cover-inner;
        if(q->theme){
            if(dy<2)c=blend(c,colors(q)->highlight,28);
            if(dx<2||dy<2)c=blend(c,colors(q)->rim,14);
            if(dx>w-3||dy>h-3)c=blend(c,0x050b14,40);
            if(rim>0)c=blend(c,colors(q)->rim,clamp(rim/5,0,48));
        }else{
            if(dy<2)c=blend(c,colors(q)->highlight,100);
            if(dx<2||dy<2)c=blend(c,colors(q)->rim,47);
            if(dx>w-3||dy>h-3)c=blend(c,0x557099,17);
            if(rim>0)c=blend(c,colors(q)->rim,clamp(rim/3,0,72));
        }
        q->pixels[at]=blend(q->pixels[at],c,cover);
    }
}
static void pane(QDesktop *q,int x,int y,int w,int h,int r,uint32_t tint,int alpha){
    soft_shadow(q,x,y,w,h,r,32,40);
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
    else if(app==Q_NOTES)icon_id=QI_TEXT;
    else if(app==Q_TERMINAL)icon_id=QI_CMD;
    else if(app==Q_SETTINGS)icon_id=QI_SETTINGS;
    else if(app==Q_CALCULATOR)icon_id=QI_CALCULATOR;
    else if(app==Q_ABOUT)icon_id=QI_ABOUT;
    if(icon_id>=0)sprite(q,cx,cy,icon_id,size);
    else icon(q,cx,cy,app,color);
}
static int dock_x(QDesktop *q){return (q->width-555)/2;}
static int dock_y(QDesktop *q){return q->height-89;}
static int menu_x(QDesktop *q){return (q->width-510)/2;}
static int menu_y(QDesktop *q){return clamp(q->height-607,54,q->height-100);}
static const char *names[Q_APPS]={"Files","Notes","Terminal","Calculator","Settings","About"};
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
    q->active=-1;q->drag=-1;q->hover=-1;q->hover_last=-1;q->menu_hover=-1;q->render_app=-1;q->glass_cached_app=-1;q->mouse_x=w/2;q->mouse_y=h/2;q->dirty=1;q->answer[0]='0';
    for(int i=0;i<Q_APPS;i++){
        QWindow *a=&q->windows[i];
        a->w=clamp(w-210,530,880);a->h=clamp(h-170,390,600);
        a->x=(w-a->w)/2+(i%3)*18-18;a->y=(h-a->h)/2+(i%3)*16-16;
    }
    wallpaper(q);
}
static void close_popups(QDesktop *q){
    q->menu=0;q->menu_hover=-1;q->notifications=0;q->quick=0;
    q->menu_search_length=0;q->menu_search[0]=0;
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
    q->animations_enabled=1;
}
void q_tick_timed(QDesktop *q,uint32_t elapsed_us){
    if(!q->animations_enabled)return;
    if(elapsed_us>100000)elapsed_us=100000;
    int frame_advance=0;
    if(q->scene_tween_active){
        if(q->drag>=0){
            q->scene_tween_active=0;q->scene_tween_pending=0;q->dirty=1;
        }else{
            int duration=(q->scene_tween_kind==1||q->scene_tween_kind==7)?145000:(q->scene_tween_kind==2)?165000:185000;
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
    q->windows[app].open=1;q->windows[app].minimized=0;q->windows[app].z=z+1;q->active=app;
    if(was_hidden){
        QWindow *w=&q->windows[app];
        start_tween(q,app,1,w->x+12,w->y+20,w->w-24,w->h-40);
        begin_scene_tween(q,1);
    }
    close_popups(q);q->dirty=1;
}
int q_dock_hit(QDesktop *q,int x,int y){
    int left=dock_x(q),top=dock_y(q);
    if(hit(x,y,left+11,top+8,52,53))return 10;
    for(int i=0;i<Q_APPS;i++)if(hit(x,y,left+77+i*77,top+8,61,53))return i;
    return -1;
}
static void draw_dock(QDesktop *q){
    int x=dock_x(q),y=dock_y(q);
    pane(q,x,y,555,70,25,colors(q)->dock,q->theme?190:112);
    int start_lift=q->animations_enabled?q->start_lift*5/256:0;
    if(q->menu||q->start_hover||q->start_lift>0)
        surface(q,x+7,y+7-start_lift,60,54,17,colors(q)->hover,q->theme?174:144);
    if(q->start_pulse>0)circular(q,x+37,y+36-start_lift,23,accent(q),q->start_pulse/4);
    sprite(q,x+37,y+33-start_lift,QI_START,43);
    rectangle(q,x+69,y+14,1,41,colors(q)->rule);
    for(int i=0;i<Q_APPS;i++){
        int xx=x+77+i*77;
        int lift=q->animations_enabled?q->dock_lift[i]*5/256:0;
        int pulse=q->dock_pulse[i];
        if((q->active==i&&q->windows[i].open&&!q->windows[i].minimized)||q->hover==i||q->dock_lift[i]>0){
            surface(q,xx,y+7-lift,61,54,17,colors(q)->hover,q->theme?174:128);
        }
        if(pulse>0)circular(q,xx+30,y+36-lift,22,accent(q),pulse/4);
        app_icon(q,xx+30,y+36-lift,i,40,q->theme?0xd7edfd:0xf6fcff);
        if(q->windows[i].open)circular(q,xx+30,y+59,3,q->windows[i].minimized?secondary(q):accent(q),255);
    }
    if(q->start_hover){
        int width=text_width("Start",0)+26;
        int tx=clamp(x+37-width/2,12,q->width-width-12);
        surface(q,tx,y-42,width,33,12,colors(q)->tooltip,q->theme?198:202);
        text(q,tx+13,y-21,"Start",foreground(q),0);
    }else if(q->hover>=0&&q->hover<Q_APPS){
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
        int bx=x+26+(slot%3)*159,by=y+112+(slot/3)*144;
        int lift=q->animations_enabled?q->menu_lift[app]*6/256:0;
        int selected=q->menu_hover==app||q->menu_lift[app]>0;
        surface(q,bx,by-lift,141,126,19,selected?colors(q)->selected:inner_material(q),selected?(q->theme?190:173):(q->theme?168:128));
        if(selected)thin_rule(q,bx+18,by+117-lift,105,q->theme?74:112);
        app_icon(q,bx+70,by+49-lift,app,58,q->theme?0xecfaff:0xffffff);
        int width=text_width(names[app],1);
        text(q,bx+(141-width)/2,by+104-lift,names[app],foreground(q),1);
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
    for(int i=0;i<Q_APPS;i++)if(q->windows[i].open&&!q->windows[i].minimized&&
       (q->active<0||q->windows[i].z>q->windows[q->active].z))q->active=i;
}
static void close_window(QDesktop *q,int app){
    if(q->windows[app].maximized){
        QWindow *w=&q->windows[app];
        w->x=w->restore_x;w->y=w->restore_y;w->w=w->restore_w;w->h=w->restore_h;
    }
    q->windows[app].open=0;q->windows[app].minimized=0;q->windows[app].maximized=0;
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
        w->restore_x=w->x;w->restore_y=w->y;w->restore_w=w->w;w->restore_h=w->h;
        w->x=12;w->y=12;w->w=q->width-24;w->h=q->height-24;w->maximized=1;
    }else{
        w->x=w->restore_x;w->y=w->restore_y;w->w=w->restore_w;w->h=w->restore_h;w->maximized=0;
    }
    start_tween(q,app,2,from_x,from_y,from_w,from_h);
    begin_scene_tween(q,2);
    q->dirty=1;q->hover_only=0;q->drag_cache_valid=0;
}
static void window_content(QDesktop *q,int app,QWindow *w){
    int x=w->x+32,y=w->y+60,cw=w->w-64;
    uint32_t ink=foreground(q),muted=secondary(q);
    if(app==Q_FILES){
        surface(q,x,y+30,cw,54,14,inner_material(q),q->theme?95:115);
        sprite(q,x+31,y+57,QI_FOLDER,28);
        text(q,x+54,y+63,"Home    /    Local",ink,0);
        surface(q,x,y+102,cw,133,17,inner_material(q),q->theme?100:126);
        text(q,x+24,y+139,"No volumes mounted",ink,1);
        text(q,x+24,y+169,"Connect a writable disk to browse files.",muted,0);
        surface(q,x+24,y+186,153,33,12,accent(q),145);
        text(q,x+38,y+208,"Open Notes",0x173248,0);
    }else if(app==Q_NOTES){
        surface(q,x,y+30,cw,w->h-158,17,inner_material(q),q->theme?74:138);
        string_limit(q,x+22,y+73,q->note_length?q->notes:"Write something...",q->note_length?ink:muted,1,cw-44,clamp((w->h-195)/26,1,20));
    }else if(app==Q_TERMINAL){
        surface(q,x,y+30,cw,w->h-157,17,0x0d2237,221);
        text(q,x+22,y+72,"quartzie >",0x8edee8,1);
        text(q,x+22,y+117,q->status[0]?q->status:"Type help for commands",0xe0f5fb,0);
        text(q,x+22,y+162,">",0x88d8e5,1);
        text(q,x+48,y+162,q->command,0xf8feff,1);
    }else if(app==Q_CALCULATOR){
        text(q,x,y,"Calculator",ink,2);
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
        text(q,x,y,"Quartzie 0.38.14",ink,2);
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
    QWindow *w=&q->windows[app];
    if(!w->open||w->minimized)return;
    int previous_render_app=q->render_app;
    q->render_app=app;
    pane(q,w->x,w->y,w->w,w->h,25,material(q),q->theme?209:149);
    thin_rule(q,w->x+23,w->y+56,w->w-46,28);
    app_icon(q,w->x+41,w->y+28,app,25,foreground(q));
    text(q,w->x+61,w->y+36,names[app],foreground(q),1);
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
        for(int i=0;i<Q_APPS;i++)if(i!=skip&&!drawn[i]&&q->windows[i].open&&!q->windows[i].minimized&&(app<0||q->windows[i].z<q->windows[app].z))app=i;
        if(app<0)break;
        drawn[app]=1;draw_window(q,app);
    }
}
static void cache_dock_background(QDesktop *q){
    if(!q->dock_background||q->drag>=0||q->clip_left||q->clip_top||q->clip_right!=q->width||q->clip_bottom!=q->height)return;
    int x=dock_x(q)-52,y=dock_y(q)-53;
    for(int row=0;row<137;row++){
        size_t src=(size_t)(y+row)*q->width+x;
        size_t dst=(size_t)row*659;
        for(int col=0;col<659;col++)q->dock_background[dst+col]=q->pixels[src+col];
    }
    q->dock_cache_valid=1;
}
static void draw_overlays(QDesktop *q){
    cache_dock_background(q);
    draw_dock(q);
    draw_menu(q);
    draw_notifications(q);
    draw_quick(q);
}
static void draw_scene(QDesktop *q,int skip){
    draw_base(q,skip);
    draw_overlays(q);
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
    for(int y=0;y<q->height;y++)for(int x=0;x<q->width;x++)glass_cached_full[(size_t)y*q->width+x]=glass_fetch(glass_cached,x,y,0,0,glass_cache_cols,glass_cache_rows);
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
    int dx2=dock_x(q)+607,dy2=dock_y(q)+84;
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
    int duration=(q->scene_tween_kind==1||q->scene_tween_kind==7)?145000:q->scene_tween_kind==2?165000:185000;
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
        int x0=dock_x(q)-52,x1=dock_x(q)+607;
        int y0=dock_y(q)-53,y1=dock_y(q)+84;
        if(q->menu){
            int mx=menu_x(q),my=menu_y(q);
            x0=mx-42;x1=mx+552;
            y0=my-42;y1=dock_y(q)+84;
        }
        clip_region(q,x0,y0,x1,y1);
        if(q->dock_cache_valid&&q->dock_background&&!q->menu){
            int bx=dock_x(q)-52,by=dock_y(q)-53;
            for(int row=0;row<137;row++){
                size_t dst=(size_t)(by+row)*q->width+bx;
                size_t src=(size_t)row*659;
                for(int col=0;col<659;col++)q->pixels[dst+col]=q->dock_background[src+col];
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
        int index=0;
        int64_t left=0,right=0,result=0;
        int valid=calc_number(q->calc,&index,&left);
        char op=q->calc[index];
        if(valid&&op){
            if(op!='+'&&op!='-'&&op!='*'&&op!='/')valid=0;
            else{
                index++;
                valid=calc_number(q->calc,&index,&right)&&q->calc[index]==0;
            }
        }
        if(valid&&!op)valid=q->calc[index]==0;
        if(valid){
            if(!op)result=left;
            else if(op=='+')result=left+right;
            else if(op=='-')result=left-right;
            else if(op=='*')result=left*right;
            else if(op=='/'&&right)result=left/right;
            else valid=0;
        }
        if(!valid||result>1000000000||result< -1000000000){
            const char *err=valid?"OVERFLOW":"ERR";
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
void q_click(QDesktop *q,int x,int y){
    q->hover_only=0;
    int dock=q_dock_hit(q,x,y);
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
    if(dock>=0&&dock<Q_APPS){q_open(q,dock);return;}
    if(dock==11){int was=q->notifications;close_popups(q);q->notifications=!was;begin_scene_tween(q,q->notifications?5:6);q->dirty=1;return;}
    if(dock==12){int was=q->quick;close_popups(q);q->quick=!was;begin_scene_tween(q,q->quick?5:6);q->dirty=1;return;}
    if(q->menu){
        int mx=menu_x(q),my=menu_y(q);
        if(hit(x,y,mx,my,510,434)){
            for(int slot=0;slot<Q_APPS;slot++){
                int app=menu_app_at(q,slot);
                if(app<0)break;
                if(hit(x,y,mx+26+slot%3*159,my+112+slot/3*144,141,126)){q_open(q,app);return;}
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
            if(x>=bx+178&&x<=bx+295)q->accent=clamp((x-(bx+162))/32,0,3);
            else q->accent=(q->accent+1)%4;
            q->dirty=1;return;
        }
        if(hit(x,y,bx+19,by+243,298,43)){q_open(q,Q_SETTINGS);return;}
        if(hit(x,y,bx,by,336,293))return;
        q->quick=0;q->dirty=1;
    }
    int chosen=-1;
    for(int i=0;i<Q_APPS;i++)if(q->windows[i].open&&!q->windows[i].minimized&&hit(x,y,q->windows[i].x,q->windows[i].y,q->windows[i].w,q->windows[i].h)&&
        (chosen<0||q->windows[i].z>q->windows[chosen].z))chosen=i;
    if(chosen>=0&&chosen<Q_APPS){
        QWindow *w=&q->windows[chosen];
        if(hit(x,y,w->x+w->w-59,w->y+13,36,29)){close_window(q,chosen);return;}
        if(hit(x,y,w->x+w->w-108,w->y+13,36,29)){maximize_window(q,chosen);return;}
        if(hit(x,y,w->x+w->w-157,w->y+13,36,29)){minimize_window(q,chosen);return;}
        q_open(q,chosen);
        if(!w->maximized&&hit(x,y,w->x,w->y,w->w-175,55)){
            q->drag=chosen;q->drag_x=x-w->x;q->drag_y=y-w->y;return;
        }
        int bx=w->x+32,by=w->y+60;
        if(chosen==Q_SETTINGS){
            if(hit(x,y,bx,by+36,w->w-64,91)){q_set_theme(q,!q->theme);}
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
    q->active=-1;q->dirty=1;
}
void q_press(QDesktop *q,int x,int y){
    q->pressed=1;q_click(q,x,y);
    if(q->drag>=0){q->scene_tween_active=0;q->scene_tween_pending=0;}
}
void q_release(QDesktop *q){q->pressed=0;if(q->drag>=0){q->dirty=1;q->drag_cache_valid=0;q->glass_cached_app=-1;}q->drag=-1;}
void q_move(QDesktop *q,int x,int y){
    q->mouse_x=clamp(x,0,q->width-1);q->mouse_y=clamp(y,0,q->height-1);
    if(q->pressed&&q->drag>=0){
        QWindow *w=&q->windows[q->drag];
        int new_x=clamp(q->mouse_x-q->drag_x,0,q->width-w->w);
        int max_y=q->height-44;
        if(max_y<12)max_y=12;
        int new_y=clamp(q->mouse_y-q->drag_y,12,max_y);
        if(new_x!=w->x||new_y!=w->y){w->x=new_x;w->y=new_y;q->dirty=1;}
    }
    if(q->drag>=0)return;
    int hover=-1;
    int dock=q_dock_hit(q,q->mouse_x,q->mouse_y);
    int start_hover=dock==10;
    if(dock>=0&&dock<Q_APPS)hover=dock;
    int menu_hover=-1;
    if(q->menu){
        int mx=menu_x(q),my=menu_y(q);
        for(int slot=0;slot<Q_APPS;slot++){
            int app=menu_app_at(q,slot);
            if(app<0)break;
            if(hit(q->mouse_x,q->mouse_y,mx+26+(slot%3)*159,my+112+(slot/3)*144,141,126)){
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
    if(ch=='\r'){
        if(q->active==Q_CALCULATOR){calc_input(q,'=');q->dirty=1;return;}
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
        else if(q->active==Q_CALCULATOR)calc_input(q,'B');
        q->dirty=1;return;
    }
    if(ch<32||ch>126)return;
    if(q->menu&&ch>='1'&&ch<'1'+Q_APPS){q_open(q,ch-'1');return;}
    if(q->active==Q_NOTES&&q->note_length<2047){q->notes[q->note_length++]=(char)ch;q->notes[q->note_length]=0;q->dirty=1;}
    else if(q->active==Q_TERMINAL&&q->command_length<120){q->command[q->command_length++]=(char)ch;q->command[q->command_length]=0;q->dirty=1;}
    else if(q->active==Q_CALCULATOR){calc_input(q,(char)ch);q->dirty=1;}
}
