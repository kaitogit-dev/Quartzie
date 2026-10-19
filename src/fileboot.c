#include "fileboot.h"
typedef struct {
    uint32_t Revision;
    uint32_t Reserved;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE DeviceHandle;
} QLoadedImage;
typedef struct QFile QFile;
struct QFile {
    uint64_t Revision;
    EFI_STATUS (EFIAPI *Open)(QFile *,QFile **,uint16_t *,uint64_t,uint64_t);
    EFI_STATUS (EFIAPI *Close)(QFile *);
    void *Delete;
    EFI_STATUS (EFIAPI *Read)(QFile *,uint64_t *,void *);
};
typedef struct QSimpleFS QSimpleFS;
struct QSimpleFS {
    uint64_t Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(QSimpleFS *,QFile **);
};
static const EFI_GUID loaded_guid={0x5b1b31a1,0x9562,0x11d2,{0x8e,0x3f,0,0xa0,0xc9,0x69,0x72,0x3b}};
static const EFI_GUID filesystem_guid={0x964e5b22,0x6459,0x11d2,{0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
int qefi_read_app(EFI_HANDLE image,EFI_BOOT_SERVICES *boot,uint8_t *buffer,size_t capacity,size_t *length){
    if(!image||!boot||!boot->HandleProtocol||!buffer||!length||capacity<4096)return -1;
    *length=0;
    QLoadedImage *loaded=0;
    if(boot->HandleProtocol(image,(EFI_GUID *)&loaded_guid,(void **)&loaded)!=0||!loaded)return -1;
    QSimpleFS *fs=0;
    if(boot->HandleProtocol(loaded->DeviceHandle,(EFI_GUID *)&filesystem_guid,(void **)&fs)!=0||!fs||!fs->OpenVolume)return -1;
    QFile *root=0;
    if(fs->OpenVolume(fs,&root)!=0||!root||!root->Open||!root->Close)return -1;
    uint16_t file_name[]={ '\\','Q','S','T','A','R','T','.','E','X','E',0 };
    QFile *file=0;
    int result=-1;
    if(root->Open(root,&file,file_name,1,0)==0&&file&&file->Read&&file->Close){
        size_t count=0;
        for(;;){
            if(count>=capacity)break;
            uint64_t available=(uint64_t)(capacity-count);
            if(available>4096)available=4096;
            if(file->Read(file,&available,buffer+count)!=0)break;
            if(available==0){
                *length=count;
                result=count>0?0:-1;
                break;
            }
            if(available>capacity-count)break;
            count+=(size_t)available;
        }
        file->Close(file);
    }
    root->Close(root);
    return result;
}
