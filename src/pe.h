#ifndef QUARTZIE_PE_H
#define QUARTZIE_PE_H
#include <stdint.h>

#define CM_PE_BASE 0x40000000u
#define CM_PE_LIMIT 0x003e0000u
#define CM_PE_THUNK_OFFSET 0x003e0000u
#define CM_PE_THUNK_PID 0u
#define CM_PE_THUNK_TICKS 16u
#define CM_PE_THUNK_EXIT 32u
#define CM_PE_OK 0
#define CM_PE_BAD_IMAGE 1
#define CM_PE_UNSUPPORTED_MACHINE 2
#define CM_PE_UNSUPPORTED_RUNTIME 3
#define CM_PE_IMPORT_MISSING 4
#define CM_PE_NO_SPACE 5

typedef struct {
    uint32_t entry;
    uint32_t image_size;
    uint32_t image_base;
    uint32_t import_count;
    uint32_t subsystem;
} CmPeImage;

typedef uint32_t (*CmPeResolver)(const char *, const char *);
int cm_pe_load(const uint8_t *file, uint32_t length, uint8_t *space, uint32_t capacity,
               CmPeImage *image, CmPeResolver resolver);
const char *cm_pe_error(int status);
#endif
