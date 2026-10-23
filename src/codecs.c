#include "codecs.h"
int _fltused=0;
#include <stdint.h>
#include <stddef.h>
void *memcpy(void *dst,const void *src,size_t n);
void *memset(void *dst,int value,size_t n);
void *memmove(void *dst,const void *src,size_t n);
int memcmp(const void *a,const void *b,size_t n);
static int qc_abs(int value){return value<0?-value:value;}
#define abs qc_abs
static uint8_t *pool;
static size_t pool_length,pool_used;
void qc_memory(void *memory,size_t size){
    pool=(uint8_t *)memory;pool_length=size;pool_used=0;
}
static void qc_reset(void){pool_used=0;}
static void *qc_alloc(size_t n){
    if(!pool||n>pool_length||pool_used>pool_length-n)return 0;
    size_t used=(pool_used+15u)&~(size_t)15;
    if(used>pool_length||n>pool_length-used)return 0;
    pool_used=used+n;
    return pool+used;
}
static void *qc_resize(void *p,size_t old_size,size_t n){
    if(n<=old_size)return p;
    void *next=qc_alloc(n);
    if(next&&p)memcpy(next,p,old_size);
    return next;
}
static void qc_free(void *p){(void)p;}
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_SIMD
#define STBI_MAX_DIMENSIONS 4096
#define STBI_ASSERT(x) ((void)0)
#define STBI_MALLOC(n) qc_alloc(n)
#define STBI_REALLOC(p,n) qc_resize(p,0,n)
#define STBI_REALLOC_SIZED(p,old_size,n) qc_resize(p,old_size,n)
#define STBI_FREE(p) qc_free(p)
#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#define MINIMP3_ONLY_MP3
#include "vendor/minimp3.h"
void *memcpy(void *dst,const void *src,size_t n){
    unsigned char *d=(unsigned char *)dst;
    const unsigned char *s=(const unsigned char *)src;
    for(size_t i=0;i<n;i++)d[i]=s[i];
    return dst;
}
void *memset(void *dst,int value,size_t n){
    unsigned char *d=(unsigned char *)dst;
    for(size_t i=0;i<n;i++)d[i]=(unsigned char)value;
    return dst;
}
void *memmove(void *dst,const void *src,size_t n){
    unsigned char *d=(unsigned char *)dst;
    const unsigned char *s=(const unsigned char *)src;
    if(d<=s||d>=s+n)return memcpy(dst,src,n);
    while(n){n--;d[n]=s[n];}
    return dst;
}
int memcmp(const void *a,const void *b,size_t n){
    const unsigned char *x=(const unsigned char *)a,*y=(const unsigned char *)b;
    for(size_t i=0;i<n;i++)if(x[i]!=y[i])return (int)x[i]-(int)y[i];
    return 0;
}
int qc_image(const uint8_t *data,size_t length,uint32_t **pixels,int *width,int *height){
    if(!data||!pixels||!width||!height||length>0x7fffffff||length<12)return -1;
    qc_reset();
    int w=0,h=0,channels=0;
    unsigned char *rgba=stbi_load_from_memory(data,(int)length,&w,&h,&channels,4);
    if(!rgba||w<1||h<1||w>4096||h>4096)return -1;
    *pixels=(uint32_t *)(void *)rgba;
    *width=w;*height=h;
    return 0;
}
int qc_mp3(const uint8_t *data,size_t length,uint8_t **samples,size_t *count,uint32_t *rate){
    if(!data||!samples||!count||!rate||length<4)return -1;
    qc_reset();
    const size_t capacity=4u*1024u*1024u;
    uint8_t *output=(uint8_t *)qc_alloc(capacity);
    if(!output)return -1;
    mp3dec_t decoder;
    mp3dec_init(&decoder);
    mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    size_t position=0,write=0;
    uint64_t source_count=0;
    int frequency=0;
    while(position+4<=length){
        mp3dec_frame_info_t info;
        size_t remaining=length-position;
        int chunk=(int)(remaining>32768?32768:remaining);
        int samples_in_frame=mp3dec_decode_frame(&decoder,data+position,chunk,pcm,&info);
        if(info.frame_bytes<=0){position++;continue;}
        if((size_t)info.frame_bytes>remaining)return -1;
        position+=(size_t)info.frame_bytes;
        if(samples_in_frame<=0)continue;
        if(info.channels<1||info.channels>2||info.hz<8000||info.hz>96000)return -1;
        if(!frequency)frequency=info.hz;
        if(frequency!=info.hz)return -1;
        for(int i=0;i<samples_in_frame;i++){
            uint64_t ordinal=source_count+(uint64_t)i;
            if(ordinal*8000u<(uint64_t)write*(uint64_t)frequency)continue;
            if(write>=capacity)return -1;
            int value=pcm[i*info.channels];
            if(info.channels==2)value=(value+(int)pcm[i*2+1])/2;
            int eight=128+value/256;
            output[write++]=(uint8_t)(eight<0?0:eight>255?255:eight);
        }
        source_count+=(uint64_t)samples_in_frame;
    }
    if(!frequency||write<8)return -1;
    *samples=output;*count=write;*rate=8000;
    return 0;
}
