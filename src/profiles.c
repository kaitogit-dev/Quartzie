#include "profiles.h"
#include "package.h"
static int equal(const char *a,const char *b){
    for(;*a&&*b;a++,b++)if(*a!=*b)return 0;
    return !*a&&!*b;
}
static int valid(const char *name){
    if(!name||!name[0])return 0;
    int i=0;
    for(;name[i]&&i<24;i++){
        char c=name[i];
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
             (c>='0'&&c<='9')||c=='-'||c=='_'))return 0;
    }
    return !name[i];
}
static void assign(char *out,const char *name){
    int i=0;for(;name[i]&&i<24;i++)out[i]=name[i];
    out[i]=0;
}
static int dir(QStorage *s,const char *name,char *out){
    (void)s;
    size_t at=0;
    const char *prefix="\\Users\\";
    for(int i=0;prefix[i];i++)out[at++]=prefix[i];
    if(!valid(name))return -1;
    for(int i=0;name[i];i++)out[at++]=name[i];
    out[at]=0;
    return at<QFS_PATH?0:-1;
}
static int has_user(QStorage *s,const char *name){
    static QFsEntry list[QFS_ENTRIES];int n=qfs_list(s,s->home_index,"\\Users",list,QFS_ENTRIES);
    for(int i=0;i<n;i++)if(list[i].directory&&equal(list[i].name,name))return 1;
    return 0;
}
static int path(QStorage *s,const char *name,const char *leaf,char *out){
    if(dir(s,name,out)!=0)return -1;
    return qfs_join(out,QFS_PATH,out,leaf);
}
static void hash_pin(const char *name,const char *pin,uint8_t result[32]){
    uint8_t data[80]={0};size_t n=0;
    const char *prefix="Quartzie account v1";
    for(int i=0;prefix[i];i++)data[n++]=(uint8_t)prefix[i];
    for(int i=0;name[i];i++)data[n++]=(uint8_t)name[i];
    data[n++]=0;
    for(int i=0;pin[i];i++)data[n++]=(uint8_t)pin[i];
    qpkg_sha256(data,n,result);
    for(int k=0;k<4096;k++)qpkg_sha256(result,32,result);
}
int qprofiles_has_pin(QStorage *s,const char *name){
    if(!s||!valid(name)||!has_user(s,name))return 0;
    char location[QFS_PATH];uint8_t record[36];size_t length=0;
    if(path(s,name,"PASS.DAT",location)!=0)return 0;
    return qfs_read(s,s->home_index,location,record,sizeof(record),&length)==0&&
       length==36&&record[0]=='Q'&&record[1]=='P'&&record[2]=='I'&&record[3]=='N';
}
int qprofiles_check_pin(QStorage *s,const char *name,const char *pin){
    if(!s||!valid(name)||!pin)return -1;
    if(s->home_index==s->ram_index&&equal(name,"Default")&&!pin[0])return 0;
    if(!has_user(s,name))return -1;
    if(!qprofiles_has_pin(s,name))return pin[0]?-1:0;
    char location[QFS_PATH];uint8_t record[36];size_t length=0;
    if(path(s,name,"PASS.DAT",location)!=0||
       qfs_read(s,s->home_index,location,record,sizeof(record),&length)!=0||length!=36)return -1;
    uint8_t result[32];hash_pin(name,pin,result);
    uint8_t diff=0;
    for(int i=0;i<32;i++)diff|=result[i]^record[i+4];
    return diff?-1:0;
}
int qprofiles_load(QStorage *s){
    if(!s)return -1;
    assign(s->profile,"Default");
    if(s->home_index==s->ram_index)return 0;
    uint8_t name[25]={0};size_t length=0;
    if(qfs_read(s,s->home_index,"\\Users\\ACTIVE.CFG",name,sizeof(name),&length)!=0||
       length<1||length>=sizeof(name))return 0;
    name[length]=0;
    if(!valid((const char *)name)||!has_user(s,(const char *)name))return 0;
    assign(s->profile,(const char *)name);
    return 0;
}
int qprofiles_list(QStorage *s,QProfile *out,int capacity){
    if(!s||!out||capacity<1)return -1;
    static QFsEntry all[QFS_ENTRIES];int n=qfs_list(s,s->home_index,"\\Users",all,QFS_ENTRIES);
    if(n<0)return -1;
    int count=0;
    for(int i=0;i<n&&count<capacity;i++)if(all[i].directory&&valid(all[i].name)){
        assign(out[count].name,all[i].name);
        out[count].has_pin=qprofiles_has_pin(s,out[count].name);
        count++;
    }
    return count;
}
int qprofiles_create(QStorage *s,const char *name){
    if(!s||!valid(name)||equal(name,"Default")||has_user(s,name))return -1;
    QFsEntry probe[1];
    if(qfs_list(s,s->home_index,"\\Users",probe,1)<0&&
       qfs_mkdir(s,s->home_index,"\\Users")!=0)return -1;
    char root[QFS_PATH],folder[QFS_PATH];
    if(dir(s,name,root)!=0||qfs_mkdir(s,s->home_index,root)!=0)return -1;
    const char *folders[]={"Desktop","Documents","Downloads","Pictures","Music","Videos","Applications"};
    for(size_t i=0;i<sizeof(folders)/sizeof(folders[0]);i++){
        if(qfs_join(folder,sizeof(folder),root,folders[i])!=0||
           qfs_mkdir(s,s->home_index,folder)!=0)return -1;
    }
    return 0;
}
int qprofiles_switch(QStorage *s,const char *name,const char *pin){
    if(!s||qprofiles_check_pin(s,name,pin)!=0)return -1;
    size_t size=0;while(name[size])size++;
    if(s->home_index!=s->ram_index&&
       qfs_write(s,s->home_index,"\\Users\\ACTIVE.CFG",name,size)!=0)return -1;
    assign(s->profile,name);
    return 0;
}
int qprofiles_set_pin(QStorage *s,const char *pin){
    if(!s||!pin)return -1;
    int n=0;while(pin[n]&&n<25)n++;
    if(pin[n]||(!n?0:n<4))return -1;
    char name[QFS_PATH];
    if(path(s,s->profile,"PASS.DAT",name)!=0)return -1;
    if(!n)return qfs_delete(s,s->home_index,name);
    uint8_t record[36]={'Q','P','I','N'};
    hash_pin(s->profile,pin,record+4);
    return qfs_write(s,s->home_index,name,record,sizeof(record));
}
