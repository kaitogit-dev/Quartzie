#ifndef QUARTZIE_CODECS_H
#define QUARTZIE_CODECS_H
#include <stdint.h>
#include <stddef.h>
void qc_memory(void *memory,size_t size);
int qc_image(const uint8_t *data,size_t length,uint32_t **pixels,int *width,int *height);
int qc_mp3(const uint8_t *data,size_t length,uint8_t **samples,size_t *count,uint32_t *rate);
#endif
