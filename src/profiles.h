#ifndef QUARTZIE_PROFILES_H
#define QUARTZIE_PROFILES_H
#include "storage.h"
#define QPROFILE_LIMIT 8
typedef struct {char name[25];int has_pin;} QProfile;
int qprofiles_load(QStorage *s);
int qprofiles_list(QStorage *s,QProfile *out,int capacity);
int qprofiles_create(QStorage *s,const char *name);
int qprofiles_switch(QStorage *s,const char *name,const char *pin);
int qprofiles_set_pin(QStorage *s,const char *pin);
int qprofiles_check_pin(QStorage *s,const char *name,const char *pin);
int qprofiles_has_pin(QStorage *s,const char *name);
#endif
