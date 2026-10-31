#ifndef QUARTZIE_MEDIA_H
#define QUARTZIE_MEDIA_H
#include <stddef.h>
#include <stdint.h>
#include "codecs.h"
#define QM_FRAMES 192
enum {QM_NONE=0,QM_IMAGE=1,QM_AUDIO=2,QM_VIDEO=3};
typedef struct {
    const uint8_t *data;
    size_t length;
    int kind,width,height,topdown,bpp;
    int frame_count,fps,playing,frame;
    uint32_t frame_start[QM_FRAMES],frame_size[QM_FRAMES];
    size_t pixels_offset;
    const uint8_t *rgba;
    const uint8_t *pcm8;
    int decoded_image;
    size_t pcm_count;
    uint32_t sample_rate,channels,sample_bits,audio_start,audio_size;
    uint64_t elapsed_us;
} QMedia;
int qm_open(QMedia *media,const void *data,size_t length);
uint32_t qm_pixel(const QMedia *media,int x,int y);
int qm_advance(QMedia *media,uint32_t microseconds);
uint64_t qm_duration(const QMedia *media);
void qm_seek(QMedia *media,uint64_t microseconds);
void qm_image_dimensions(const QMedia *media,int rotation,int *width,int *height);
uint32_t qm_image_pixel(const QMedia *media,int rotation,int flipped,int x,int y);
int qm_export_bmp(const QMedia *media,int rotation,int flipped,uint8_t *output,size_t capacity,size_t *written);
uint8_t qm_audio_sample(const QMedia *media,uint64_t index);
uint64_t qm_audio_count(const QMedia *media);
#endif
