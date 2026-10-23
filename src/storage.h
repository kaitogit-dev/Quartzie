#ifndef QUARTZIE_STORAGE_H
#define QUARTZIE_STORAGE_H
#include "efi.h"
#include <stddef.h>
#include <stdint.h>
#define QFS_VOLUMES 8
#define QFS_ENTRIES 48
#define QFS_PATH 192
typedef struct {
    char name[72];
    uint64_t size;
    int directory;
} QFsEntry;
typedef struct {
    EFI_HANDLE handle;
    char label[20];
    int readonly;
} QFsVolume;
typedef struct {
    EFI_BOOT_SERVICES *boot;
    QFsVolume volumes[QFS_VOLUMES];
    int count;
} QStorage;
void qfs_init(QStorage *s,EFI_HANDLE image,EFI_BOOT_SERVICES *boot);
int qfs_list(QStorage *s,int volume,const char *path,QFsEntry *entries,int limit);
int qfs_read(QStorage *s,int volume,const char *path,void *data,size_t capacity,size_t *length);
int qfs_write(QStorage *s,int volume,const char *path,const void *data,size_t length);
int qfs_mkdir(QStorage *s,int volume,const char *path);
int qfs_delete(QStorage *s,int volume,const char *path);
int qfs_join(char *out,size_t capacity,const char *directory,const char *name);
#endif
