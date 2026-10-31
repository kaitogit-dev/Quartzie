#ifndef QUARTZIE_UPDATE_H
#define QUARTZIE_UPDATE_H
#include "storage.h"
#include <stddef.h>
#include <stdint.h>
#define QUPD_LIMIT (8u*1024u*1024u)
typedef struct {
    char version[17];
    uint32_t size;
    uint8_t hash[32];
} QUpdate;
int qupd_verify(const uint8_t *archive,size_t length,QUpdate *out);
int qupd_firmware_valid(const uint8_t *data,size_t length);
int qupd_find(QStorage *s,int *volume,char *path,size_t capacity);
int qupd_target(QStorage *s,int require_backup);
int qupd_apply(QStorage *s,int target,int source,const char *path,uint8_t *buffer,size_t capacity);
int qupd_rollback(QStorage *s,int target,uint8_t *buffer,size_t capacity);
int qupd_health(QStorage *s,int target,uint8_t *buffer,size_t capacity);
int qupd_clear_backup(QStorage *s,int target);
#endif
