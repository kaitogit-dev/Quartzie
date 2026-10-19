#ifndef QUARTZIE_PE64_H
#define QUARTZIE_PE64_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    void *context;
    void *(*resolve)(void *context,const char *dll,const char *symbol,uint16_t ordinal);
} QPe64Resolver;
typedef struct {
    uint8_t *base;
    uint32_t size;
    uint32_t entry_rva;
    uint32_t imports;
} QPe64Image;
enum {
    Q_PE_OK=0,
    Q_PE_INVALID=-1,
    Q_PE_ARCH=-2,
    Q_PE_BOUNDS=-3,
    Q_PE_RELOCATION=-4,
    Q_PE_IMPORT=-5,
    Q_PE_MEMORY=-6
};
int q_pe64_load(const void *file,size_t file_size,void *memory,size_t capacity,const QPe64Resolver *resolver,QPe64Image *out);
void *q_pe64_entry(const QPe64Image *image);
void *q_pe64_export(const QPe64Image *image,const char *name);
#endif
