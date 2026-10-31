#include "update.h"
#include "package.h"

static const char *const firmware="\\EFI\\BOOT\\BOOTX64.EFI";
static const char *const backup="\\EFI\\BOOT\\QUARTZIE.BAK";
static const char *const staged="\\EFI\\BOOT\\QUARTZIE.NEW";
static const char *const stamp="\\EFI\\BOOT\\QUARTZIE.HSH";
static const char *const probe="\\EFI\\BOOT\\QUARTZIE.CHK";
static uint16_t le16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t *p){return (uint32_t)le16(p)|((uint32_t)le16(p+2)<<16);}
static int match(const char *a,const char *b){
    for(;*a&&*b;a++,b++)if(*a!=*b)return 0;
    return *a==*b;
}
static int has_file(QStorage *s,int volume,const char *path){
    int cut=0;
    for(int i=0;path[i];i++)if(path[i]=='\\')cut=i;
    char parent[QFS_PATH],name[72];int i=0;
    if(cut==0){parent[0]='\\';parent[1]=0;}
    else{for(;i<cut;i++)parent[i]=path[i];parent[i]=0;}
    int at=0;
    for(i=cut+1;path[i]&&at<71;i++)name[at++]=path[i];
    name[at]=0;
    static QFsEntry files[QFS_ENTRIES];
    int n=qfs_list(s,volume,parent,files,QFS_ENTRIES);
    for(i=0;i<n;i++)if(match(files[i].name,name)&&!files[i].directory)return 1;
    return 0;
}
int qupd_firmware_valid(const uint8_t *p,size_t size){
    if(!p||size<256||p[0]!='M'||p[1]!='Z')return 0;
    uint32_t pe=le32(p+0x3c);
    if(pe<0x40||pe>size||size-pe<96||le32(p+pe)!=0x00004550)return 0;
    return le16(p+pe+4)==0x8664&&le16(p+pe+24)==0x20b&&le16(p+pe+24+68)==10;
}
int qupd_verify(const uint8_t *archive,size_t length,QUpdate *out){
    if(!archive||!out||length<320||length>QUPD_LIMIT||
       archive[0]!='Q'||archive[1]!='U'||archive[2]!='P'||archive[3]!='D'||archive[4]!=1)return -1;
    int n=0;
    for(;n<16&&archive[5+n];n++){
        char c=(char)archive[5+n];
        if(!((c>='0'&&c<='9')||c=='.'||c=='-'))return -1;
        out->version[n]=c;
    }
    if(!n||n==16)return -1;
    out->version[n]=0;
    for(int i=5+n;i<21;i++)if(archive[i])return -1;
    size_t size=le32(archive+21);
    if(size!=length-64||!qupd_firmware_valid(archive+64,size))return -1;
    uint8_t hash[32];
    qpkg_sha256(archive+64,size,hash);
    for(int i=0;i<32;i++)if(hash[i]!=archive[25+i])return -1;
    for(int i=57;i<64;i++)if(archive[i])return -1;
    for(int i=0;i<32;i++)out->hash[i]=hash[i];
    out->size=(uint32_t)size;
    return 0;
}
int qupd_find(QStorage *s,int *volume,char *path,size_t capacity){
    if(!s||!volume||!path)return -1;
    char downloads[QFS_PATH];
    if(qfs_profile_path(s,"Downloads\\QUARTZIE.UPD",downloads,sizeof(downloads))!=0)return -1;
    const char *paths[]={"\\QUARTZIE.UPD",downloads};
    for(int v=0;v<s->count;v++)for(int k=0;k<2;k++){
        if(k&&v!=s->home_index)continue;
        if(!has_file(s,v,paths[k]))continue;
        size_t i=0;
        for(;paths[k][i];i++){if(i+1>=capacity)return -1;path[i]=paths[k][i];}
        path[i]=0;*volume=v;return 0;
    }
    return -1;
}
int qupd_target(QStorage *s,int require_backup){
    if(!s)return -1;
    for(int v=0;v<s->ram_index;v++){
        if(require_backup?!has_file(s,v,backup):!has_file(s,v,firmware))continue;
        if(qfs_write(s,v,probe,"Q",1)!=0)continue;
        qfs_delete(s,v,probe);
        return v;
    }
    return -1;
}
int qupd_health(QStorage *s,int target,uint8_t *buffer,size_t capacity){
    if(!s||!buffer||capacity<256||target<0||target>=s->count)return -1;
    size_t size=0;
    if(qfs_read(s,target,firmware,buffer,capacity,&size)!=0||
       !qupd_firmware_valid(buffer,size))return -1;
    uint8_t recorded[36];size_t count=0;
    if(qfs_read(s,target,stamp,recorded,sizeof(recorded),&count)==0&&count==36&&
       recorded[0]=='Q'&&recorded[1]=='H'&&recorded[2]=='S'&&recorded[3]=='1'){
        uint8_t hash[32];
        qpkg_sha256(buffer,size,hash);
        for(int i=0;i<32;i++)if(hash[i]!=recorded[4+i])return -1;
    }
    return 0;
}
int qupd_apply(QStorage *s,int target,int source,const char *path,uint8_t *buffer,size_t capacity){
    if(!s||!path||!buffer||target<0||target>=s->count||
       source<0||source>=s->count||capacity<320||capacity>QUPD_LIMIT||
       has_file(s,target,backup))return -1;
    size_t length=0;QUpdate update={0};
    if(qfs_read(s,source,path,buffer,capacity,&length)!=0||
       qupd_verify(buffer,length,&update)!=0||!has_file(s,target,firmware))return -1;
    if(has_file(s,target,staged)&&qfs_delete(s,target,staged)!=0)return -1;
    if(qfs_write(s,target,staged,buffer+64,update.size)!=0){qfs_delete(s,target,staged);return -1;}
    size_t checked=0;
    if(qfs_read(s,target,staged,buffer,capacity,&checked)!=0||checked!=update.size){
        qfs_delete(s,target,staged);return -1;
    }
    uint8_t hash[32];qpkg_sha256(buffer,checked,hash);
    for(int i=0;i<32;i++)if(hash[i]!=update.hash[i]){
        qfs_delete(s,target,staged);return -1;
    }
    if(qfs_copy(s,target,firmware,target,backup)!=0){qfs_delete(s,target,staged);return -1;}
    if(qfs_delete(s,target,firmware)!=0){
        qfs_delete(s,target,staged);qfs_delete(s,target,backup);return -1;
    }
    if(qfs_rename(s,target,staged,firmware)!=0){
        qfs_copy(s,target,backup,target,firmware);return -1;
    }
    uint8_t record[36]={'Q','H','S','1'};
    for(int i=0;i<32;i++)record[i+4]=update.hash[i];
    if(qfs_write(s,target,stamp,record,sizeof(record))!=0||qupd_health(s,target,buffer,capacity)!=0){
        qupd_rollback(s,target,buffer,capacity);
        return -1;
    }
    return 0;
}
int qupd_rollback(QStorage *s,int target,uint8_t *buffer,size_t capacity){
    if(!s||!buffer||target<0||target>=s->count||capacity<256)return -1;
    size_t length=0;
    if(qfs_read(s,target,backup,buffer,capacity,&length)!=0||
       !qupd_firmware_valid(buffer,length))return -1;
    if(qfs_write(s,target,firmware,buffer,length)!=0)return -1;
    if(qupd_health(s,target,buffer,capacity)!=0){
        qfs_delete(s,target,stamp);
        if(qupd_health(s,target,buffer,capacity)!=0)return -1;
    }
    if(has_file(s,target,stamp))qfs_delete(s,target,stamp);
    return qfs_delete(s,target,backup);
}
int qupd_clear_backup(QStorage *s,int target){
    if(!s||target<0||target>=s->count||!has_file(s,target,backup))return -1;
    return qfs_delete(s,target,backup);
}
