#include "update.h"
#include "package.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do{if(!(v)){fprintf(stderr,"FAIL update line %d: %s\n",__LINE__,#v);exit(1);}}while(0)
static QStorage fs;
static uint8_t old[320],next[320],archive[384],buffer[4096],touched[384];
static void make_efi(uint8_t *data,int which){
    memset(data,0,320);
    data[0]='M';data[1]='Z';
    data[0x3c]=0x80;
    data[0x80]='P';data[0x81]='E';
    data[0x84]=0x64;data[0x85]=0x86;
    data[0x98]=0x0b;data[0x99]=0x02;
    data[0xdc]=10;
    data[256]=(uint8_t)which;
}
int main(void){
    qfs_init(&fs,0,0);
    CHECK(qfs_mkdir(&fs,0,"\\EFI")==0);
    CHECK(qfs_mkdir(&fs,0,"\\EFI\\BOOT")==0);
    make_efi(old,1);make_efi(next,2);
    CHECK(qupd_firmware_valid(old,sizeof(old)));
    CHECK(qfs_write(&fs,0,"\\EFI\\BOOT\\BOOTX64.EFI",old,sizeof(old))==0);
    memset(archive,0,sizeof(archive));
    memcpy(archive,"QUPD",4);archive[4]=1;
    memcpy(archive+5,"0.45.1",6);archive[21]=sizeof(next)&255;archive[22]=sizeof(next)>>8;
    qpkg_sha256(next,sizeof(next),archive+25);memcpy(archive+64,next,sizeof(next));
    QUpdate info={0};
    CHECK(qupd_verify(archive,sizeof(archive),&info)==0);
    memcpy(touched,archive,sizeof(touched));touched[255]^=1;
    CHECK(qupd_verify(touched,sizeof(touched),&info)!=0);
    CHECK(qfs_write(&fs,0,"\\QUARTZIE.UPD",archive,sizeof(archive))==0);
    int volume=-1;char path[QFS_PATH];
    CHECK(qupd_find(&fs,&volume,path,sizeof(path))==0&&volume==0);
    CHECK(qupd_apply(&fs,0,0,path,buffer,sizeof(buffer))==0);
    CHECK(qupd_health(&fs,0,buffer,sizeof(buffer))==0);
    CHECK(buffer[256]==2);
    CHECK(qupd_apply(&fs,0,0,path,buffer,sizeof(buffer))!=0);
    CHECK(qupd_rollback(&fs,0,buffer,sizeof(buffer))==0);
    CHECK(qupd_health(&fs,0,buffer,sizeof(buffer))==0&&buffer[256]==1);
    CHECK(qupd_apply(&fs,0,0,path,buffer,sizeof(buffer))==0);
    CHECK(qupd_clear_backup(&fs,0)==0);
    CHECK(qupd_clear_backup(&fs,0)!=0);
    CHECK(qupd_apply(&fs,0,0,path,buffer,sizeof(buffer))==0);
    CHECK(qfs_delete(&fs,0,"\\EFI\\BOOT\\BOOTX64.EFI")==0);
    CHECK(qupd_rollback(&fs,0,buffer,sizeof(buffer))==0);
    CHECK(qupd_health(&fs,0,buffer,sizeof(buffer))==0);
    puts("PASS: firmware update SHA256, staging, integrity, rollback and backup");
    return 0;
}
