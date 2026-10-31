#ifndef QUARTZIE_TRASH_H
#define QUARTZIE_TRASH_H
#include "storage.h"
#define QTRASH_LIMIT 96
typedef struct {
    char id[9],path[QFS_PATH],name[72];
    uint64_t size;
    int directory;
} QTrashEntry;
int qtrash_put(QStorage *s,int volume,const char *path,int directory);
int qtrash_list(QStorage *s,QTrashEntry *out,int capacity);
int qtrash_restore(QStorage *s,const char *id);
int qtrash_remove(QStorage *s,const char *id);
int qtrash_empty(QStorage *s);
#endif
