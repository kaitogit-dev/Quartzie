#include "profiles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do{if(!(v)){fprintf(stderr,"FAIL profiles line %d: %s\n",__LINE__,#v);exit(1);}}while(0)
static QStorage s;
static QProfile users[QPROFILE_LIMIT];
int main(void){
    qfs_init(&s,0,0);
    CHECK(qprofiles_load(&s)==0&&!strcmp(s.profile,"Default"));
    CHECK(qprofiles_create(&s,"Alice")==0);
    CHECK(qprofiles_create(&s,"Bob_2")==0);
    CHECK(qprofiles_create(&s,"Alice")!=0);
    CHECK(qprofiles_create(&s,"..")!=0);
    CHECK(qprofiles_create(&s,"a\\b")!=0);
    CHECK(qprofiles_list(&s,users,QPROFILE_LIMIT)==2);
    CHECK(qprofiles_switch(&s,"Alice","")==0);
    CHECK(!strcmp(s.profile,"Alice"));
    CHECK(qprofiles_set_pin(&s,"123456")==0);
    CHECK(qprofiles_has_pin(&s,"Alice"));
    CHECK(qprofiles_check_pin(&s,"Alice","123456")==0);
    CHECK(qprofiles_check_pin(&s,"Alice","123455")!=0);
    CHECK(qprofiles_switch(&s,"Bob_2","")==0);
    CHECK(qprofiles_switch(&s,"Alice","")!=0);
    CHECK(qprofiles_switch(&s,"Alice","123456")==0);
    char home[QFS_PATH];
    CHECK(qfs_profile_path(&s,"Documents",home,sizeof(home))==0);
    CHECK(!strcmp(home,"\\Users\\Alice\\Documents"));
    CHECK(qprofiles_set_pin(&s,"")==0);
    CHECK(!qprofiles_has_pin(&s,"Alice"));
    puts("PASS: profile creation, paths, switching and PIN validation");
    return 0;
}
