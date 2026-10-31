#ifndef QUARTZIE_PACKAGE_H
#define QUARTZIE_PACKAGE_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    char id[80],name[81],version[65],platform[16];
    const uint8_t *payload;
    size_t payload_size;
} QPackage;
int qpkg_verify(const uint8_t *data,size_t length,QPackage *info);
#endif
