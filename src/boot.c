#include "efi.h"
#include "ui.h"
#include "ps2.h"
#include "ntcore.h"
#include "winapi.h"
#include "fileboot.h"
static uint32_t backbuffer[Q_MAX_W*Q_MAX_H];
static uint32_t wallpaper_buffer[Q_MAX_W*Q_MAX_H];
static uint32_t frosted_buffer[Q_MAX_W*Q_MAX_H];
static uint32_t drag_base_buffer[Q_MAX_W*Q_MAX_H];
static uint32_t dock_base_buffer[659*137];
static QDesktop desktop;
static QNtKernel native_kernel;
static uint8_t executable_buffer[1024*1024];
extern const uint8_t q_winapp_data[];
extern const size_t q_winapp_size;
static EFI_GRAPHICS_OUTPUT_PROTOCOL *video;
static uint32_t *screen;
static uint32_t scanline;
static int pixel_format;
static int cursor_old_x=-100,cursor_old_y=-100;
static uint32_t cursor_underlay[27*31];
static uint16_t cursor_shadows[27*31],cursor_edges[27*31],cursor_fills[27*31];
static uint8_t cursor_covered[27*31];
static void cursor_draw(int x,int y);
static EFI_SIMPLE_POINTER_PROTOCOL *pointer_relative;
static EFI_ABSOLUTE_POINTER_PROTOCOL *pointer_absolute;
static QPs2Mouse *pointer_ps2;
static uint64_t pointer_next_ps2_tick;
static uint64_t clock_ticks_per_us=1;
static int pointer_x,pointer_y,pointer_rem_x,pointer_rem_y,pointer_pressed;
static uint64_t read_clock(void){
    unsigned int low,high;
    __asm__ volatile("rdtsc":"=a"(low),"=d"(high));
    return ((uint64_t)high<<32)|low;
}
static void pointer_sample(void *context){
    (void)context;
    int moved=0;
    if(pointer_absolute&&pointer_absolute->GetState&&pointer_absolute->Mode){
        EFI_ABSOLUTE_POINTER_STATE state;
        if(pointer_absolute->GetState(pointer_absolute,&state)==0){
            EFI_ABSOLUTE_POINTER_MODE *mode=pointer_absolute->Mode;
            if(mode->AbsoluteMaxX>mode->AbsoluteMinX&&mode->AbsoluteMaxY>mode->AbsoluteMinY){
                uint64_t px=state.CurrentX>=mode->AbsoluteMinX?state.CurrentX-mode->AbsoluteMinX:0;
                uint64_t py=state.CurrentY>=mode->AbsoluteMinY?state.CurrentY-mode->AbsoluteMinY:0;
                uint64_t max_x=mode->AbsoluteMaxX-mode->AbsoluteMinX;
                uint64_t max_y=mode->AbsoluteMaxY-mode->AbsoluteMinY;
                if(px>max_x)px=max_x;
                if(py>max_y)py=max_y;
                pointer_x=(int)(px*(uint64_t)(desktop.width-1)/max_x);
                pointer_y=(int)(py*(uint64_t)(desktop.height-1)/max_y);
                pointer_pressed=(state.ActiveButtons&1)!=0;
                moved=1;
            }
        }
    }
    if(!moved&&pointer_relative&&pointer_relative->GetState){
        EFI_SIMPLE_POINTER_STATE state;
        if(pointer_relative->GetState(pointer_relative,&state)==0){
            uint64_t sx=pointer_relative->Mode?pointer_relative->Mode->ResolutionX:0;
            uint64_t sy=pointer_relative->Mode?pointer_relative->Mode->ResolutionY:0;
            if(sx<=100)sx=100;
            if(sy<=100)sy=100;
            if(sx>1000000)sx=1000000;
            if(sy>1000000)sy=1000000;
            int64_t mx=(int64_t)state.RelativeMovementX*100+pointer_rem_x;
            int64_t my=(int64_t)state.RelativeMovementY*100+pointer_rem_y;
            pointer_x+=(int)(mx/(int64_t)sx);
            pointer_y+=(int)(my/(int64_t)sy);
            pointer_rem_x=(int)(mx%(int64_t)sx);
            pointer_rem_y=(int)(my%(int64_t)sy);
            pointer_pressed=state.LeftButton!=0;
            moved=1;
        }
    }
    if(!moved&&pointer_ps2){
        uint64_t now=read_clock();
        if(now>=pointer_next_ps2_tick){
            pointer_next_ps2_tick=now+clock_ticks_per_us*12000;
            int dx=0,dy=0,buttons=0;
            if(q_ps2_poll(pointer_ps2,&dx,&dy,&buttons)){
                pointer_x+=dx;
                pointer_y+=dy;
                pointer_pressed=(buttons&1)!=0;
                moved=1;
            }
        }
    }
    if(!moved)return;
    if(pointer_x<0)pointer_x=0;
    if(pointer_y<0)pointer_y=0;
    if(pointer_x>=desktop.width)pointer_x=desktop.width-1;
    if(pointer_y>=desktop.height)pointer_y=desktop.height-1;
    if(cursor_old_x!=pointer_x||cursor_old_y!=pointer_y)cursor_draw(pointer_x,pointer_y);
}

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
        cursor_covered[k]=(cursor_shadows[k]||cursor_edges[k]||cursor_fills[k])?1:0;
    }
}
static void cursor_hide(void){
    if(cursor_old_x<0)return;
    for(int yy=0;yy<31;yy++)for(int xx=0;xx<27;xx++){
        int k=yy*27+xx;
        if(!cursor_covered[k])continue;
        int px=cursor_old_x+xx,py=cursor_old_y+yy;
        if((unsigned)px<(unsigned)desktop.width&&(unsigned)py<(unsigned)desktop.height)
            screen[(size_t)py*scanline+px]=cursor_underlay[k];
    }
    cursor_old_x=-100;cursor_old_y=-100;
}
static void cursor_draw(int x,int y){
    const int cw=27,ch=31;
    cursor_hide();
    for(int yy=0;yy<ch;yy++)for(int xx=0;xx<cw;xx++){
        if(!cursor_covered[yy*cw+xx])continue;
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
    cursor_draw(pointer_x,pointer_y);
}
static void present_damage(int x,int y,int width,int height){
    int x0=x<0?0:x,y0=y<0?0:y;
    int x1=x+width>desktop.width?desktop.width:x+width;
    int y1=y+height>desktop.height?desktop.height:y+height;
    int touches_cursor=cursor_old_x>=0&&cursor_old_x<x1&&cursor_old_x+27>x0&&cursor_old_y<y1&&cursor_old_y+31>y0;
    if(touches_cursor)cursor_hide();
    for(int py=y0;py<y1;py++){
        size_t offset=(size_t)py*desktop.width;
        volatile uint32_t *dst=(volatile uint32_t *)(screen+(size_t)py*scanline);
        for(int px=x0;px<x1;px++){
            uint32_t c=desktop.pixels[offset+px];
            if(pixel_format==0)c=((c&255)<<16)|(c&0x00ff00)|((c>>16)&255);
            dst[px]=c;
        }
    }
    if(touches_cursor)cursor_draw(pointer_x,pointer_y);
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
    q_dock_buffers(&desktop,dock_base_buffer);
    q_draw(&desktop);present();
    q_enable_animations(&desktop);
    if(w==1920&&h==1080)serial_text("QUARTZIE-MODE-1920x1080\n");
    else serial_text("QUARTZIE-MODE-FALLBACK\n");
    qnt_init(&native_kernel);
    size_t executable_size=0;
    int has_disk_app=qefi_read_app(image,bs,executable_buffer,sizeof(executable_buffer),&executable_size)==0;
    if(has_disk_app)serial_text("QUARTZIE-WIN32-FILE\n");
    const uint8_t *app=has_disk_app?executable_buffer:q_winapp_data;
    size_t app_size=has_disk_app?executable_size:q_winapp_size;
    if(qwin_execute(&native_kernel,bs,app,app_size,serial_char)==QNT_OK)
        serial_text("QUARTZIE-WIN32-READY\n");
    else serial_text("QUARTZIE-WIN32-FAILED\n");
    serial_text("QUARTZIE-DESKTOP-READY\n");
    EFI_SIMPLE_POINTER_PROTOCOL *mouse=0;
    EFI_ABSOLUTE_POINTER_PROTOCOL *absolute=0;
    bs->LocateProtocol((EFI_GUID *)&MOUSE_GUID,0,(void **)&mouse);
    bs->LocateProtocol((EFI_GUID *)&ABS_MOUSE_GUID,0,(void **)&absolute);
    pointer_absolute=absolute;
    pointer_relative=mouse;
    pointer_x=desktop.mouse_x;
    pointer_y=desktop.mouse_y;
    QPs2Mouse ps2={0};
    int ps2_active=0,ever_moved=0,ps2_logged=0;
    if(!mouse&&!absolute){
        ps2_active=q_ps2_enable();
        if(ps2_active)pointer_ps2=&ps2;
        serial_text(ps2_active?"QUARTZIE-PS2-READY\n":"QUARTZIE-PS2-FAILED\n");
    }
    uint8_t left=0;
    uint64_t tick_before=read_clock();
    bs->Stall(10000);
    uint64_t tick_after=read_clock();
    clock_ticks_per_us=(tick_after-tick_before)/10000;
    if(!clock_ticks_per_us)clock_ticks_per_us=1;
    uint64_t last_tick=read_clock();
    for(;;){
        pointer_sample(0);
        if(pointer_x!=desktop.mouse_x||pointer_y!=desktop.mouse_y){
            q_move(&desktop,pointer_x,pointer_y);
            if(!ever_moved){serial_text("QUARTZIE-POINTER-MOVED\n");ever_moved=1;}
        }
        if(pointer_pressed&&!left)q_press(&desktop,desktop.mouse_x,desktop.mouse_y);
        if(!pointer_pressed&&left)q_release(&desktop);
        left=(uint8_t)pointer_pressed;
        if(q_ps2_bytes_seen&&!ps2_logged){serial_text("QUARTZIE-PS2-RAW\n");ps2_logged=1;}
        if(table->ConIn){
            for(int n=0;n<12;n++){
                EFI_INPUT_KEY key;
                if(table->ConIn->ReadKeyStroke(table->ConIn,&key)!=0)break;
                q_key(&desktop,key.UnicodeChar,key.ScanCode);
            }
        }
        if(cursor_old_x!=pointer_x||cursor_old_y!=pointer_y)cursor_draw(pointer_x,pointer_y);
        qnt_tick(&native_kernel);
        uint64_t now=read_clock();
        uint64_t delta=(now-last_tick)/clock_ticks_per_us;
        last_tick=now;
        if(delta>100000)delta=100000;
        q_tick_timed(&desktop,(uint32_t)delta);
        if(desktop.dirty){
            q_draw(&desktop);
            if(desktop.damage_valid)present_damage(desktop.damage_x,desktop.damage_y,desktop.damage_w,desktop.damage_h);
            else present();
        }else if(cursor_old_x!=pointer_x||cursor_old_y!=pointer_y)cursor_draw(pointer_x,pointer_y);
        bs->Stall(desktop.drag>=0?200:desktop.scene_tween_active?300:750);
    }
}
