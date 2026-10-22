#include "efi.h"
#include "ui.h"
#include "ps2.h"
#include "ntcore.h"
#include "winapi.h"
#include "fileboot.h"
#include "hostcursor.h"
#include "cursor.h"
static uint32_t *backbuffer;
static uint32_t *wallpaper_buffer;
static uint32_t *frosted_buffer;
static uint32_t *drag_base_buffer;
static uint32_t *dock_base_buffer;
static QDesktop desktop;
static QNtKernel *native_kernel;
#define Q_EXECUTABLE_CAPACITY (1024u*1024u)
static uint8_t *executable_buffer;
static void *efi_alloc(EFI_BOOT_SERVICES *boot,size_t bytes){
    if(!boot||!boot->AllocatePages||!bytes)return 0;
    uint64_t address=0;
    uint64_t pages=((uint64_t)bytes+4095u)/4096u;
    if(boot->AllocatePages(0,2,pages,&address)!=0||!address)return 0;
    return (void *)(uintptr_t)address;
}
extern const uint8_t q_winapp_data[];
extern const size_t q_winapp_size;
static EFI_GRAPHICS_OUTPUT_PROTOCOL *video;
static uint32_t *screen;
static uint32_t scanline;
static int pixel_format;
static int host_cursor;
static int cursor_old_x=-100,cursor_old_y=-100;
static uint32_t cursor_underlay[Q_CURSOR_W*Q_CURSOR_H];
static uint8_t cursor_covered[Q_CURSOR_W*Q_CURSOR_H];
static void cursor_draw(int x,int y);
static EFI_SIMPLE_POINTER_PROTOCOL *pointer_relative;
static EFI_ABSOLUTE_POINTER_PROTOCOL *pointer_absolute;
static QPs2Mouse *pointer_ps2;
static int pointer_ps2_confirmed;
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
    if(!pointer_ps2_confirmed&&pointer_absolute&&pointer_absolute->GetState&&pointer_absolute->Mode){
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
                int next_x=(int)(px*(uint64_t)(desktop.width-1)/max_x);
                int next_y=(int)(py*(uint64_t)(desktop.height-1)/max_y);
                int next_pressed=(state.ActiveButtons&1)!=0;
                if(next_x!=pointer_x||next_y!=pointer_y||next_pressed!=pointer_pressed){
                    pointer_x=next_x;pointer_y=next_y;pointer_pressed=next_pressed;
                    pointer_ps2=0;
                    moved=1;
                }
            }
        }
    }
    if(!moved&&!pointer_ps2_confirmed&&pointer_relative&&pointer_relative->GetState){
        EFI_SIMPLE_POINTER_STATE state;
        if(pointer_relative->GetState(pointer_relative,&state)==0){
            uint64_t sx=pointer_relative->Mode?pointer_relative->Mode->ResolutionX:0;
            uint64_t sy=pointer_relative->Mode?pointer_relative->Mode->ResolutionY:0;
            if(sx<=100)sx=100;
            if(sy<=100)sy=100;
            if(sx>1000000)sx=1000000;
            if(sy>1000000)sy=1000000;
            int64_t mx=(int64_t)state.RelativeMovementX*400+pointer_rem_x;
            int64_t my=(int64_t)state.RelativeMovementY*400+pointer_rem_y;
            int dx=(int)(mx/(int64_t)sx),dy=(int)(my/(int64_t)sy);
            int next_pressed=state.LeftButton!=0;
            pointer_rem_x=(int)(mx%(int64_t)sx);
            pointer_rem_y=(int)(my%(int64_t)sy);
            if(dx||dy||next_pressed!=pointer_pressed){
                pointer_x+=dx;pointer_y+=dy;pointer_pressed=next_pressed;
                pointer_ps2=0;
                moved=1;
            }
        }
    }
    if(!moved&&pointer_ps2){
        int dx=0,dy=0,buttons=0;
        if(q_ps2_poll(pointer_ps2,&dx,&dy,&buttons)){
            int next_pressed=(buttons&1)!=0;
            if(dx||dy||next_pressed!=pointer_pressed){
                pointer_x+=dx;
                pointer_y+=dy;
                pointer_pressed=next_pressed;
                pointer_ps2_confirmed=1;
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
static void cursor_prepare(void){
    for(int i=0;i<Q_CURSOR_W*Q_CURSOR_H;i++)
        cursor_covered[i]=(q_cursor_pixels[i]>>24)!=0;
}
static void cursor_hide(void){
    if(cursor_old_x<0)return;
    for(int yy=0;yy<Q_CURSOR_H;yy++)for(int xx=0;xx<Q_CURSOR_W;xx++){
        int k=yy*Q_CURSOR_W+xx;
        if(!cursor_covered[k])continue;
        int px=cursor_old_x+xx,py=cursor_old_y+yy;
        if((unsigned)px<(unsigned)desktop.width&&(unsigned)py<(unsigned)desktop.height)
            screen[(size_t)py*scanline+px]=cursor_underlay[k];
    }
    cursor_old_x=-100;cursor_old_y=-100;
}
static void cursor_draw(int x,int y){
    if(host_cursor)return;
    const int left=x-Q_CURSOR_HOT_X,top=y-Q_CURSOR_HOT_Y;
    cursor_hide();
    for(int yy=0;yy<Q_CURSOR_H;yy++)for(int xx=0;xx<Q_CURSOR_W;xx++){
        int k=yy*Q_CURSOR_W+xx;
        if(!cursor_covered[k])continue;
        int px=left+xx,py=top+yy;
        if((unsigned)px>=(unsigned)desktop.width||(unsigned)py>=(unsigned)desktop.height)continue;
        uint32_t raw=screen[(size_t)py*scanline+px];
        cursor_underlay[k]=raw;
        uint32_t col=pixel_format==0?(((raw&255)<<16)|(raw&0x00ff00)|((raw>>16)&255)):raw;
        uint32_t overlay=q_cursor_pixels[k];
        int opacity=(int)(overlay>>24);
        col=mix(col,overlay&0xffffff,opacity==255?256:opacity);
        if(pixel_format==0)col=((col&255)<<16)|(col&0x00ff00)|((col>>16)&255);
        screen[(size_t)py*scanline+px]=col;
    }
    cursor_old_x=left;cursor_old_y=top;
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
    int touches_cursor=cursor_old_x>=0&&cursor_old_x<x1&&cursor_old_x+Q_CURSOR_W>x0&&cursor_old_y<y1&&cursor_old_y+Q_CURSOR_H>y0;
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
    host_cursor=q_host_cursor_enabled();
    if(host_cursor)serial_text("QUARTZIE-HOST-CURSOR\n");
    size_t frame_bytes=(size_t)w*(size_t)h*sizeof(uint32_t);
    backbuffer=efi_alloc(bs,frame_bytes);
    wallpaper_buffer=efi_alloc(bs,frame_bytes);
    frosted_buffer=efi_alloc(bs,frame_bytes);
    drag_base_buffer=efi_alloc(bs,frame_bytes);
    dock_base_buffer=efi_alloc(bs,659u*137u*sizeof(uint32_t));
    uint32_t *ui_storage=efi_alloc(bs,(size_t)Q_UI_SCRATCH_PIXELS*sizeof(uint32_t));
    native_kernel=efi_alloc(bs,sizeof(*native_kernel));
    executable_buffer=efi_alloc(bs,Q_EXECUTABLE_CAPACITY);
    if(!backbuffer||!wallpaper_buffer||!frosted_buffer||!drag_base_buffer||
       !dock_base_buffer||!ui_storage||!native_kernel||!executable_buffer){
        serial_text("QUARTZIE-MEMORY-FAILED\n");
        return 6;
    }
    q_ui_set_scratch(ui_storage);
    cursor_prepare();
    q_init(&desktop,w,h,backbuffer,wallpaper_buffer,frosted_buffer);
    q_drag_buffers(&desktop,drag_base_buffer);
    q_dock_buffers(&desktop,dock_base_buffer);
    q_draw(&desktop);present();
    q_enable_animations(&desktop);
    if(w==1920&&h==1080)serial_text("QUARTZIE-MODE-1920x1080\n");
    else serial_text("QUARTZIE-MODE-FALLBACK\n");
    qnt_init(native_kernel);
    size_t executable_size=0;
    int has_disk_app=qefi_read_app(image,bs,executable_buffer,Q_EXECUTABLE_CAPACITY,&executable_size)==0;
    if(has_disk_app)serial_text("QUARTZIE-WIN32-FILE\n");
    const uint8_t *app=has_disk_app?executable_buffer:q_winapp_data;
    size_t app_size=has_disk_app?executable_size:q_winapp_size;
    if(qwin_execute(native_kernel,bs,app,app_size,serial_char)==QNT_OK)
        serial_text("QUARTZIE-WIN32-READY\n");
    else serial_text("QUARTZIE-WIN32-FAILED\n");
    serial_text("QUARTZIE-DESKTOP-READY\n");
    EFI_SIMPLE_POINTER_PROTOCOL *mouse=0;
    EFI_ABSOLUTE_POINTER_PROTOCOL *absolute=0;
    bs->LocateProtocol((EFI_GUID *)&MOUSE_GUID,0,(void **)&mouse);
    bs->LocateProtocol((EFI_GUID *)&ABS_MOUSE_GUID,0,(void **)&absolute);
    pointer_absolute=absolute;
    pointer_relative=mouse;
    serial_text(absolute?"QUARTZIE-ABS-POINTER\n":"QUARTZIE-NO-ABS\n");
    serial_text(mouse?"QUARTZIE-REL-POINTER\n":"QUARTZIE-NO-REL\n");
    pointer_x=desktop.mouse_x;
    pointer_y=desktop.mouse_y;
    QPs2Mouse ps2={0};
    int ps2_active=0,ps2_wait=0,ps2_tries=0,ever_moved=0,ps2_logged=0;
    int pointer_updates=0;
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
            if(++pointer_updates==16)serial_text("QUARTZIE-POINTER-CONTINUOUS\n");
        }
        if(pointer_pressed&&!left)q_press(&desktop,desktop.mouse_x,desktop.mouse_y);
        if(!pointer_pressed&&left)q_release(&desktop);
        left=(uint8_t)pointer_pressed;
        if(!ps2_active&&!ever_moved&&ps2_tries<2&&++ps2_wait>=400){
            ps2_tries++;
            ps2_active=q_ps2_enable();
            if(ps2_active){pointer_ps2=&ps2;pointer_relative=0;serial_text("QUARTZIE-PS2-READY\n");}
            ps2_wait=0;
        }
        if(q_ps2_bytes_seen&&!ps2_logged){serial_text("QUARTZIE-PS2-RAW\n");ps2_logged=1;}
        if(table->ConIn){
            for(int n=0;n<12;n++){
                EFI_INPUT_KEY key;
                if(table->ConIn->ReadKeyStroke(table->ConIn,&key)!=0)break;
                q_key(&desktop,key.UnicodeChar,key.ScanCode);
            }
        }
        if(cursor_old_x!=pointer_x||cursor_old_y!=pointer_y)cursor_draw(pointer_x,pointer_y);
        qnt_tick(native_kernel);
        uint64_t now=read_clock();
        uint64_t delta=(now-last_tick)/clock_ticks_per_us;
        last_tick=now;
        if(delta>100000)delta=100000;
        q_tick_timed(&desktop,(uint32_t)delta);
        if(desktop.dirty){
            q_draw(&desktop);
            pointer_sample(0);
            if(pointer_x!=desktop.mouse_x||pointer_y!=desktop.mouse_y)q_move(&desktop,pointer_x,pointer_y);
            if(desktop.damage_valid)present_damage(desktop.damage_x,desktop.damage_y,desktop.damage_w,desktop.damage_h);
            else present();
        }else if(cursor_old_x!=pointer_x||cursor_old_y!=pointer_y)cursor_draw(pointer_x,pointer_y);
        pointer_sample(0);
        bs->Stall(desktop.drag>=0?200:desktop.scene_tween_active?300:500);
    }
}
