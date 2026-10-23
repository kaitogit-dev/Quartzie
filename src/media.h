#ifndef QUARTZIE_MEDIA_H
#define QUARTZIE_MEDIA_H
#include <stddef.h>
#include <stdint.h>
#define QM_FRAMES 192
enum {QM_NONE=0,QM_IMAGE=1,QM_AUDIO=2,QM_VIDEO=3};
typedef struct {
    const uint8_t *data;
    size_t length;
    int kind,width,height,topdown,bpp;
    int frame_count,fps,playing,frame;
    uint32_t frame_start[QM_FRAMES],frame_size[QM_FRAMES];
    size_t pixels_offset;
    uint32_t sample_rate,channels,sample_bits,audio_start,audio_size;
    uint64_t elapsed_us;
} QMedia;
int qm_open(QMedia *media,const void *data,size_t length);
uint32_t qm_pixel(const QMedia *media,int x,int y);
int qm_advance(QMedia *media,uint32_t microseconds);
uint8_t qm_audio_sample(const QMedia *media,uint64_t index);
uint64_t qm_audio_count(const QMedia *media);
#endif
