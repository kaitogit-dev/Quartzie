#include "efi.h"
#include "ui.h"
#include "ps2.h"
static uint32_t backbuffer[Q_MAX_W*Q_MAX_H];
static uint32_t wallpaper_buffer[Q_MAX_W*Q_MAX_H];
static uint32_t frosted_buffer[Q_MAX_W*Q_MAX_H];
static uint32_t drag_base_buffer[Q_MAX_W*Q_MAX_H];
static QDesktop desktop;
static EFI_GRAPHICS_OUTPUT_PROTOCOL *video;
static uint32_t *screen;
static uint32_t scanline;
static int pixel_format;
static int cursor_old_x=-100,cursor_old_y=-100;
static uint32_t cursor_underlay[27*31];
static uint16_t cursor_shadows[27*31],cursor_edges[27*31],cursor_fills[27*31];
static void serial_char(uint8_t c){
    for(int i=0;i<10000;i++){
        uint8_t flag;
        __asm__ volatile("inb %1,%0":"=a"(flag):"Nd"((uint16_t)0x3fd));
        if(flag&0x20)break;
    }
    __asm__ volatile("outb %0,%1"::"a"(c),"Nd"((uint16_t)0x3f8));
}
static void serial_init(void){
    __asm__ volatile("outb %0,%1"::"a"((uint8_t)0),"Nd"((uint16_t)0x3f9));
    __asm__ volatile("outb %0,%1"::"a"((uint8_t)0x80),"Nd"((uint16_t)0x3fb));
    __asm__ volatile("outb %0,%1"::"a"((uint8_t)1),"Nd"((uint16_t)0x3f8));
    __asm__ volatile("outb %0,%1"::"a"((uint8_t)0),"Nd"((uint16_t)0x3f9));
    __asm__ volatile("outb %0,%1"::"a"((uint8_t)3),"Nd"((uint16_t)0x3fb));
}
static void serial_text(const char *value){for(;*value;value++)serial_char((uint8_t)*value);}
static uint32_t mix(uint32_t a,uint32_t b,int alpha){
    int inv=256-alpha;
    return (((((a>>16)&255)*inv+((b>>16)&255)*alpha)>>8)<<16)|(((((a>>8)&255)*inv+((b>>8)&255)*alpha)>>8)<<8)|((((a&255)*inv)+((b&255)*alpha))>>8);
}
static int pointer_poly(int x,int y,int inner){
    static const int outer[7][2]={{1,1},{1,23},{6,18},{11,27},{15,25},{10,16},{20,16}};
    static const int white[7][2]={{3,4},{3,18},{7,14},{12,24},{13,23},{8,14},{15,14}};
    const int (*points)[2]=inner?white:outer;
    int inside=0;
    for(int i=0,j=6;i<7;j=i++){
        int xa=points[i][0]*16,xb=points[j][0]*16;
        int ya=points[i][1]*16,yb=points[j][1]*16;
        if((ya>y)==(yb>y))continue;
        int64_t left=(int64_t)(x-xa)*(yb-ya);
        int64_t right=(int64_t)(xb-xa)*(y-ya);
        if((yb>ya&&left<right)||(yb<ya&&left>right))inside=!inside;
    }
    return inside;
}
static int pointer_cover(int px,int py,int inner){
    int count=0;
    for(int iy=0;iy<2;iy++)for(int ix=0;ix<2;ix++){
        if(pointer_poly(px*16+4+ix*8,py*16+4+iy*8,inner))count++;
    }
    return count*64;
}
static void cursor_prepare(void){
    for(int y=0;y<31;y++)for(int x=0;x<27;x++){
        int k=y*27+x;
        cursor_shadows[k]=(uint16_t)pointer_cover(x-2,y-2,0);
        cursor_edges[k]=(uint16_t)pointer_cover(x,y,0);
        cursor_fills[k]=(uint16_t)pointer_cover(x,y,1);
    }
}
static void cursor_hide(void){
    if(cursor_old_x<0)return;
    for(int yy=0;yy<31;yy++)for(int xx=0;xx<27;xx++){
        int px=cursor_old_x+xx,py=cursor_old_y+yy;
        if((unsigned)px<(unsigned)desktop.width&&(unsigned)py<(unsigned)desktop.height)
            screen[(size_t)py*scanline+px]=cursor_underlay[yy*27+xx];
    }
    cursor_old_x=-100;cursor_old_y=-100;
}
static void cursor_draw(int x,int y){
    const int cw=27,ch=31;
    cursor_hide();
    for(int yy=0;yy<ch;yy++)for(int xx=0;xx<cw;xx++){
        int px=x+xx,py=y+yy;
        if((unsigned)px>=(unsigned)desktop.width||(unsigned)py>=(unsigned)desktop.height)continue;
        uint32_t raw=screen[(size_t)py*scanline+px];
        cursor_underlay[yy*cw+xx]=raw;
        uint32_t col=pixel_format==0?(((raw&255)<<16)|(raw&0x00ff00)|((raw>>16)&255)):raw;
        int shadow=cursor_shadows[yy*cw+xx];
        int edge=cursor_edges[yy*cw+xx];
        int fill=cursor_fills[yy*cw+xx];
        if(shadow)col=mix(col,0x091526,shadow/4);
        if(edge)col=mix(col,0x15273a,edge);
        if(fill)col=mix(col,0xffffff,fill);
        if(pixel_format==0)col=((col&255)<<16)|(col&0x00ff00)|((col>>16)&255);
        screen[(size_t)py*scanline+px]=col;
    }
    cursor_old_x=x;cursor_old_y=y;
}
static void present(void){
    cursor_hide();
    for(int y=0;y<desktop.height;y++){
        size_t offset=(size_t)y*desktop.width;
        volatile uint32_t *out=(volatile uint32_t *)(screen+(size_t)y*scanline);
        for(int x=0;x<desktop.width;x++){
            uint32_t c=desktop.pixels[offset+x];
            if(pixel_format==0)c=((c&255)<<16)|(c&0x00ff00)|((c>>16)&255);
            out[x]=c;
        }
    }
    cursor_draw(desktop.mouse_x,desktop.mouse_y);
}
static void present_damage(int x,int y,int width,int height){
    cursor_hide();
    int x0=x<0?0:x,y0=y<0?0:y;
    int x1=x+width>desktop.width?desktop.width:x+width;
    int y1=y+height>desktop.height?desktop.height:y+height;
    for(int py=y0;py<y1;py++){
        size_t offset=(size_t)py*desktop.width;
        volatile uint32_t *dst=(volatile uint32_t *)(screen+(size_t)py*scanline);
        for(int px=x0;px<x1;px++){
            uint32_t c=desktop.pixels[offset+px];
            if(pixel_format==0)c=((c&255)<<16)|(c&0x00ff00)|((c>>16)&255);
            dst[px]=c;
        }
    }
    cursor_draw(desktop.mouse_x,desktop.mouse_y);
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *table){
    (void)image;
    serial_init();
    serial_text("QUARTZIE-EFI-ENTRY\n");
    if(!table||!table->BootServices)return 1;
    EFI_BOOT_SERVICES *bs=table->BootServices;
    if(!bs->LocateProtocol||!bs->Stall)return 2;
    if(bs->LocateProtocol((EFI_GUID *)&GOP_GUID,0,(void **)&video)!=0||!video||!video->Mode)return 3;
    if(video->QueryMode&&video->SetMode){
        uint32_t choice=video->Mode->Mode;
        uint64_t best=~(uint64_t)0;
        for(uint32_t i=0;i<video->Mode->MaxMode;i++){
            uint64_t n=0;EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *m=0;
            if(video->QueryMode(video,i,&n,&m)!=0||!m)continue;
            if(m->HorizontalResolution<800||m->VerticalResolution<600||m->HorizontalResolution>Q_MAX_W||m->VerticalResolution>Q_MAX_H||m->PixelFormat>1)continue;
            int64_t dx=(int64_t)m->HorizontalResolution-1920,dy=(int64_t)m->VerticalResolution-1080;
            uint64_t diff=(uint64_t)(dx*dx+dy*dy);
            if(diff<best){best=diff;choice=i;}
        }
        if(choice!=video->Mode->Mode)video->SetMode(video,choice);
    }
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode=video->Mode->Info;
    int w=(int)mode->HorizontalResolution,h=(int)mode->VerticalResolution;
    if(w<800||h<600||w>Q_MAX_W||h>Q_MAX_H||mode->PixelsPerScanLine<(uint32_t)w||mode->PixelFormat>1)return 4;
    if(video->Mode->FrameBufferSize<(uint64_t)mode->PixelsPerScanLine*h*4)return 5;
    scanline=mode->PixelsPerScanLine;pixel_format=(int)mode->PixelFormat;
    screen=(uint32_t *)(uintptr_t)video->Mode->FrameBufferBase;
    cursor_prepare();
    q_init(&desktop,w,h,backbuffer,wallpaper_buffer,frosted_buffer);
    q_drag_buffers(&desktop,drag_base_buffer);
    q_draw(&desktop);present();
    if(w==1920&&h==1080)serial_text("QUARTZIE-MODE-1920x1080\n");
    else serial_text("QUARTZIE-MODE-FALLBACK\n");
    serial_text("QUARTZIE-DESKTOP-READY\n");
    EFI_SIMPLE_POINTER_PROTOCOL *mouse=0;
    EFI_ABSOLUTE_POINTER_PROTOCOL *absolute=0;
    bs->LocateProtocol((EFI_GUID *)&MOUSE_GUID,0,(void **)&mouse);
    bs->LocateProtocol((EFI_GUID *)&ABS_MOUSE_GUID,0,(void **)&absolute);
    QPs2Mouse ps2={0};
    int ps2_active=0,wait=0,ever_moved=0,ps2_logged=0;
    if(!mouse&&!absolute){ps2_active=q_ps2_enable();serial_text(ps2_active?"QUARTZIE-PS2-READY\n":"QUARTZIE-PS2-FAILED\n");}
    uint8_t left=0;
    for(;;){
        if(mouse&&mouse->GetState){
            EFI_SIMPLE_POINTER_STATE s;
            if(mouse->GetState(mouse,&s)==0){
                int dx=s.RelativeMovementX,dy=s.RelativeMovementY;
                uint64_t scale_x=mouse->Mode?mouse->Mode->ResolutionX:0;
                uint64_t scale_y=mouse->Mode?mouse->Mode->ResolutionY:0;
                if(scale_x>100)dx=(int)((int64_t)dx*100/(int64_t)scale_x);
                if(scale_y>100)dy=(int)((int64_t)dy*100/(int64_t)scale_y);
                if(dx||dy){q_move(&desktop,desktop.mouse_x+dx,desktop.mouse_y+dy);if(!ever_moved)serial_text("QUARTZIE-POINTER-MOVED\n");ever_moved=1;}
                if(s.LeftButton&&!left)q_press(&desktop,desktop.mouse_x,desktop.mouse_y);
                if(!s.LeftButton&&left)q_release(&desktop);
                left=s.LeftButton;
            }
        }else if(absolute&&absolute->GetState&&absolute->Mode){
            EFI_ABSOLUTE_POINTER_STATE s;
            if(absolute->GetState(absolute,&s)==0){
                EFI_ABSOLUTE_POINTER_MODE *m=absolute->Mode;
                if(m->AbsoluteMaxX>m->AbsoluteMinX&&m->AbsoluteMaxY>m->AbsoluteMinY){
                    int x=(int)((s.CurrentX-m->AbsoluteMinX)*(uint64_t)(w-1)/(m->AbsoluteMaxX-m->AbsoluteMinX));
                    int y=(int)((s.CurrentY-m->AbsoluteMinY)*(uint64_t)(h-1)/(m->AbsoluteMaxY-m->AbsoluteMinY));
                    if(x!=desktop.mouse_x||y!=desktop.mouse_y){if(!ever_moved)serial_text("QUARTZIE-POINTER-MOVED\n");ever_moved=1;}
                    q_move(&desktop,x,y);
                }
                int pressed=(s.ActiveButtons&1)!=0;
                if(pressed&&!left)q_press(&desktop,desktop.mouse_x,desktop.mouse_y);
                if(!pressed&&left)q_release(&desktop);
                left=(uint8_t)pressed;
            }
        }
        if(!ps2_active&&!ever_moved&&++wait>=220){
            ps2_active=q_ps2_enable();
            if(ps2_active)serial_text("QUARTZIE-PS2-READY\n");
            wait=0;
        }
        if(ps2_active){
            for(int n=0;n<12;n++){
                int dx=0,dy=0,buttons=0;
                if(!q_ps2_poll(&ps2,&dx,&dy,&buttons))break;
                if(dx||dy){
                    q_move(&desktop,desktop.mouse_x+dx,desktop.mouse_y+dy);
                    if(!ever_moved)serial_text("QUARTZIE-POINTER-MOVED\n");
                    ever_moved=1;
                }
                if((buttons&1)&&!left)q_press(&desktop,desktop.mouse_x,desktop.mouse_y);
                if(!(buttons&1)&&left)q_release(&desktop);
                left=(uint8_t)(buttons&1);
            }
        }
        if(q_ps2_bytes_seen&&!ps2_logged){serial_text("QUARTZIE-PS2-RAW\n");ps2_logged=1;}
        if(table->ConIn){
            for(int n=0;n<12;n++){
                EFI_INPUT_KEY key;
                if(table->ConIn->ReadKeyStroke(table->ConIn,&key)!=0)break;
                q_key(&desktop,key.UnicodeChar,key.ScanCode);
            }
        }
        if(cursor_old_x!=desktop.mouse_x||cursor_old_y!=desktop.mouse_y)cursor_draw(desktop.mouse_x,desktop.mouse_y);
        if(desktop.dirty){
            q_draw(&desktop);
            if(desktop.damage_valid)present_damage(desktop.damage_x,desktop.damage_y,desktop.damage_w,desktop.damage_h);
            else present();
        }else if(cursor_old_x!=desktop.mouse_x||cursor_old_y!=desktop.mouse_y)cursor_draw(desktop.mouse_x,desktop.mouse_y);
        bs->Stall(desktop.drag>=0?1000:3000);
    }
}
