#include "storage.h"
typedef struct {
    uint32_t revision,reserved;
    EFI_HANDLE parent;
    EFI_SYSTEM_TABLE *system;
    EFI_HANDLE device;
} QLoadedImage;
typedef struct QFile QFile;
struct QFile {
    uint64_t Revision;
    EFI_STATUS (EFIAPI *Open)(QFile *,QFile **,uint16_t *,uint64_t,uint64_t);
    EFI_STATUS (EFIAPI *Close)(QFile *);
    EFI_STATUS (EFIAPI *Delete)(QFile *);
    EFI_STATUS (EFIAPI *Read)(QFile *,uint64_t *,void *);
    EFI_STATUS (EFIAPI *Write)(QFile *,uint64_t *,void *);
    EFI_STATUS (EFIAPI *GetPosition)(QFile *,uint64_t *);
    EFI_STATUS (EFIAPI *SetPosition)(QFile *,uint64_t);
    EFI_STATUS (EFIAPI *GetInfo)(QFile *,EFI_GUID *,uint64_t *,void *);
    EFI_STATUS (EFIAPI *SetInfo)(QFile *,EFI_GUID *,uint64_t,void *);
    EFI_STATUS (EFIAPI *Flush)(QFile *);
};
typedef struct QSimpleFS QSimpleFS;
struct QSimpleFS {
    uint64_t Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(QSimpleFS *,QFile **);
};
static EFI_GUID fs_guid={0x964e5b22,0x6459,0x11d2,{0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
static EFI_GUID image_guid={0x5b1b31a1,0x9562,0x11d2,{0x8e,0x3f,0,0xa0,0xc9,0x69,0x72,0x3b}};
static EFI_GUID info_guid={0x09576e92,0x6d3f,0x11d2,{0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
static void zero(void *p,size_t n){uint8_t *b=p;for(size_t i=0;i<n;i++)b[i]=0;}
static int equal(const char *a,const char *b){
    for(;*a&&*b;a++,b++)if(*a!=*b)return 0;
    return *a==*b;
}
static int wide(uint16_t *dst,size_t capacity,const char *source){
    size_t n=0;
    if(!source||!capacity)return -1;
    if(*source!='\\'){dst[n++]='\\';}
    for(;*source;source++){
        if(n+1>=capacity)return -1;
        if((unsigned char)*source<32||*source=='/')return -1;
        dst[n++]=(uint8_t)*source;
    }
    dst[n]=0;
    return 0;
}
int qfs_join(char *dst,size_t capacity,const char *dir,const char *name){
    if(!dst||!capacity||!dir||!name||!*name)return -1;
    size_t n=0;
    while(dir[n]){if(n+1>=capacity)return -1;dst[n]=dir[n];n++;}
    if(n==0){dst[n++]='\\';}
    if(dst[n-1]!='\\'){if(n+1>=capacity)return -1;dst[n++]='\\';}
    for(size_t k=0;name[k];k++){
        if(name[k]=='/'||name[k]=='\\'||(unsigned char)name[k]<32||name[k]==':')return -1;
        if(n+1>=capacity)return -1;
        dst[n++]=name[k];
    }
    dst[n]=0;return 0;
}
static int open_path(QStorage *s,int volume,const char *path,uint64_t mode,uint64_t attributes,QFile **result){
    if(!s||volume<0||volume>=s->count||!s->boot||!s->boot->HandleProtocol||!result)return -1;
    *result=0;
    QSimpleFS *fs=0;
    if(s->boot->HandleProtocol(s->volumes[volume].handle,&fs_guid,(void **)&fs)!=0||!fs||!fs->OpenVolume)return -1;
    QFile *root=0;
    if(fs->OpenVolume(fs,&root)!=0||!root||!root->Open||!root->Close)return -1;
    uint16_t file[QFS_PATH+2];
    if(wide(file,QFS_PATH+2,path)!=0){root->Close(root);return -1;}
    if(equal(path,"\\")||equal(path,"")){
        *result=root;
        return 0;
    }
    int ok=root->Open(root,result,file,mode,attributes)==0&&*result!=0;
    root->Close(root);
    return ok?0:-1;
}
static int compare(const QFsEntry *a,const QFsEntry *b){
    if(a->directory!=b->directory)return a->directory?-1:1;
    for(int i=0;i<72;i++){
        unsigned char ca=(unsigned char)a->name[i],cb=(unsigned char)b->name[i];
        if(ca>='A'&&ca<='Z')ca+=32;
        if(cb>='A'&&cb<='Z')cb+=32;
        if(ca!=cb)return ca<cb?-1:1;
        if(!ca)return 0;
    }
    return 0;
}
static int ram_find(QStorage *s,const char *path){
    for(int i=0;i<QFS_MEMORY_FILES;i++)if(s->memory[i].active&&equal(s->memory[i].path,path))return i;
    return -1;
}
static int ram_folder(QStorage *s,const char *path){
    if(equal(path,"\\")||equal(path,""))return 1;
    int i=ram_find(s,path);
    return i>=0&&s->memory[i].directory;
}
static void ram_parent(const char *path,char *parent){
    int n=0,cut=0;
    for(;path[n]&&n<QFS_PATH-1;n++)if(path[n]=='\\')cut=n;
    if(cut==0){parent[0]='\\';parent[1]=0;return;}
    for(int i=0;i<cut;i++)parent[i]=path[i];
    parent[cut]=0;
}
static int ram_add(QStorage *s,const char *path,int directory,const void *buffer,size_t length){
    if(!s||!path||!*path||length>QFS_MEMORY_BYTES||ram_find(s,path)>=0)return -1;
    char parent[QFS_PATH];ram_parent(path,parent);
    if(!ram_folder(s,parent))return -1;
    for(int i=0;i<QFS_MEMORY_FILES;i++)if(!s->memory[i].active){
        QMemoryFile *f=&s->memory[i];
        zero(f,sizeof(*f));f->active=1;f->directory=directory;
        int j=0;for(;path[j]&&j<QFS_PATH-1;j++)f->path[j]=path[j];
        if(path[j]){f->active=0;return -1;}
        f->length=length;
        for(size_t k=0;k<length;k++)f->data[k]=((const uint8_t *)buffer)[k];
        return 0;
    }
    return -1;
}
static int ram_list(QStorage *s,const char *path,QFsEntry *entries,int limit){
    if(!ram_folder(s,path))return -1;
    int count=0;
    for(int i=0;i<QFS_MEMORY_FILES;i++)if(s->memory[i].active){
        char parent[QFS_PATH];ram_parent(s->memory[i].path,parent);
        if(!equal(parent,path)&&!(equal(path,"")&&equal(parent,"\\")))continue;
        if(count==limit)break;
        QFsEntry *e=&entries[count];zero(e,sizeof(*e));
        const char *basename=s->memory[i].path;
        for(const char *p=basename;*p;p++)if(*p=='\\')basename=p+1;
        int j=0;for(;basename[j]&&j<71;j++)e->name[j]=basename[j];
        e->size=s->memory[i].length;e->directory=s->memory[i].directory;
        count++;
    }
    for(int i=1;i<count;i++){
        QFsEntry value=entries[i];int j=i;
        while(j>0&&compare(&value,&entries[j-1])<0){entries[j]=entries[j-1];j--;}
        entries[j]=value;
    }
    return count;
}
void qfs_init(QStorage *s,EFI_HANDLE image,EFI_BOOT_SERVICES *boot){
    if(!s)return;
    zero(s,sizeof(*s));s->boot=boot;
    if(boot&&boot->HandleProtocol&&boot->LocateHandleBuffer){
        EFI_HANDLE preferred=0;
        QLoadedImage *loaded=0;
        if(image&&boot->HandleProtocol(image,&image_guid,(void **)&loaded)==0&&loaded)preferred=loaded->device;
        uint64_t count=0;EFI_HANDLE *handles=0;
        if(boot->LocateHandleBuffer(2,&fs_guid,0,&count,&handles)==0&&handles){
            for(uint64_t pass=0;pass<2;pass++)for(uint64_t i=0;i<count;i++){
                if((handles[i]==preferred)!=(pass==0))continue;
                QSimpleFS *fs=0;QFile *root=0;
                if(boot->HandleProtocol(handles[i],&fs_guid,(void **)&fs)!=0||!fs||!fs->OpenVolume)continue;
                if(fs->OpenVolume(fs,&root)!=0||!root)continue;
                if(root->Close)root->Close(root);
                if(s->count==QFS_VOLUMES-1)break;
                int n=s->count++;
                s->volumes[n].handle=handles[i];
                const char *label=(handles[i]==preferred)?"Boot":0;
                if(label){for(int k=0;label[k];k++)s->volumes[n].label[k]=label[k];}
                else{
                    s->volumes[n].label[0]='D';s->volumes[n].label[1]='i';s->volumes[n].label[2]='s';s->volumes[n].label[3]='k';s->volumes[n].label[4]=' ';
                    s->volumes[n].label[5]=(char)('1'+(n>0?n-1:0));
                }
            }
            if(boot->FreePool)boot->FreePool(handles);
        }
    }
    s->ram_index=s->count;
    QFsVolume *home=&s->volumes[s->count++];
    const char *label="Home RAM";
    for(int i=0;label[i];i++)home->label[i]=label[i];
}
int qfs_list(QStorage *s,int volume,const char *path,QFsEntry *entries,int limit){
    if(!entries||limit<1)return -1;
    if(s&&volume==s->ram_index)return ram_list(s,path,entries,limit);
    QFile *directory=0;
    if(open_path(s,volume,path,1,0,&directory)!=0||!directory||!directory->Read)return -1;
    int count=0;
    for(int tries=0;tries<1024;tries++){
        uint64_t size=1024;
        union {uint64_t align;uint8_t bytes[1024];} data;
        if(directory->Read(directory,&size,data.bytes)!=0||size==0)break;
        if(size<82)continue;
        uint16_t *name=(uint16_t *)(void *)(data.bytes+80);
        if(name[0]=='.'&&(name[1]==0||(name[1]=='.'&&name[2]==0)))continue;
        if(count>=limit)continue;
        QFsEntry *entry=&entries[count];
        zero(entry,sizeof(*entry));
        for(int i=0;i<71&&80+(size_t)(i+1)*2<=size&&name[i];i++)
            entry->name[i]=name[i]<128?(char)name[i]:'?';
        if(!entry->name[0])continue;
        entry->size=*(uint64_t *)(void *)(data.bytes+8);
        entry->directory=(*(uint64_t *)(void *)(data.bytes+72)&0x10u)!=0;
        count++;
    }
    if(directory->Close)directory->Close(directory);
    for(int i=1;i<count;i++){
        QFsEntry v=entries[i];
        int k=i;
        while(k>0&&compare(&v,&entries[k-1])<0){entries[k]=entries[k-1];k--;}
        entries[k]=v;
    }
    return count;
}
int qfs_read(QStorage *s,int volume,const char *path,void *data,size_t capacity,size_t *length){
    if(!data||!length||capacity<1)return -1;
    *length=0;
    if(s&&volume==s->ram_index){
        int i=ram_find(s,path);
        if(i<0||s->memory[i].directory||s->memory[i].length>capacity)return -1;
        *length=s->memory[i].length;
        for(size_t k=0;k<*length;k++)((uint8_t *)data)[k]=s->memory[i].data[k];
        return 0;
    }
    QFile *file=0;
    if(open_path(s,volume,path,1,0,&file)!=0||!file||!file->Read)return -1;
    uint64_t size=capacity;
    int ok=file->Read(file,&size,data)==0&&size<=capacity;
    if(file->Close)file->Close(file);
    if(!ok)return -1;
    *length=(size_t)size;return 0;
}
int qfs_write(QStorage *s,int volume,const char *path,const void *data,size_t length){
    if(!data||!s||volume<0||volume>=s->count)return -1;
    if(volume==s->ram_index){
        int i=ram_find(s,path);
        if(i<0)return ram_add(s,path,0,data,length);
        if(s->memory[i].directory||length>QFS_MEMORY_BYTES)return -1;
        QMemoryFile *f=&s->memory[i];f->length=length;
        for(size_t j=0;j<length;j++)f->data[j]=((const uint8_t *)data)[j];
        return 0;
    }
    QFile *file=0;
    if(open_path(s,volume,path,0x8000000000000003ull,0,&file)!=0||!file||!file->Write)return -1;
    if(file->SetPosition&&file->SetPosition(file,0)!=0){file->Close(file);return -1;}
    uint64_t count=length;
    int ok=file->Write(file,&count,(void *)data)==0&&count==length;
    if(ok&&file->GetInfo&&file->SetInfo){
        union {uint64_t align;uint8_t bytes[512];} info;
        uint64_t len=sizeof(info.bytes);
        if(file->GetInfo(file,&info_guid,&len,info.bytes)==0&&len>=82&&*(uint64_t *)(void *)(info.bytes+8)>count){
            *(uint64_t *)(void *)(info.bytes+8)=count;
            if(file->SetInfo(file,&info_guid,len,info.bytes)!=0)ok=0;
        }
    }
    if(file->Flush&&file->Flush(file)!=0)ok=0;
    file->Close(file);
    return ok?0:-1;
}
int qfs_mkdir(QStorage *s,int volume,const char *path){
    if(s&&volume==s->ram_index)return ram_add(s,path,1,"",0);
    QFile *file=0;
    if(open_path(s,volume,path,0x8000000000000003ull,0x10,&file)!=0)return -1;
    if(file&&file->Close)file->Close(file);
    return 0;
}
int qfs_delete(QStorage *s,int volume,const char *path){
    if(s&&volume==s->ram_index){
        int i=ram_find(s,path);
        if(i<0)return -1;
        if(s->memory[i].directory){
            char parent[QFS_PATH];
            for(int j=0;j<QFS_MEMORY_FILES;j++)if(s->memory[j].active&&j!=i){
                ram_parent(s->memory[j].path,parent);
                if(equal(parent,path))return -1;
            }
        }
        zero(&s->memory[i],sizeof(s->memory[i]));
        return 0;
    }
    QFile *file=0;
    if(open_path(s,volume,path,3,0,&file)!=0||!file)return -1;
    if(!file->Delete){if(file->Close)file->Close(file);return -1;}
    return file->Delete(file)==0?0:-1;
}
