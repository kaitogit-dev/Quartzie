#include "network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL networking line %d\n",__LINE__);exit(1);}}while(0)
static QNetMode mode;
static QNetProtocol protocol;
static EFI_HANDLE handles[2];
static int freed;
static EFI_STATUS EFIAPI locate(uint32_t type,EFI_GUID *guid,void *search,uint64_t *count,EFI_HANDLE **output){
    (void)search;
    CHECK(type==2&&guid->a==0xa19832b9);
    *count=2;*output=handles;
    return 0;
}
static EFI_STATUS EFIAPI handle(EFI_HANDLE h,EFI_GUID *guid,void **out){
    CHECK(guid->a==0xa19832b9);
    if(h==handles[1])return 1;
    *out=&protocol;return 0;
}
static EFI_STATUS EFIAPI free_pool(void *p){CHECK(p==handles);freed++;return 0;}
int main(void){
    static EFI_BOOT_SERVICES boot;
    QNetwork network;
    qnet_refresh(0,&network);
    CHECK(network.count==0);
    handles[0]=(EFI_HANDLE)(uintptr_t)1;
    handles[1]=(EFI_HANDLE)(uintptr_t)2;
    mode.HwAddressSize=6;
    mode.CurrentAddress.bytes[0]=0x52;
    mode.CurrentAddress.bytes[1]=0x54;
    mode.CurrentAddress.bytes[2]=0x00;
    mode.CurrentAddress.bytes[3]=0x12;
    mode.CurrentAddress.bytes[4]=0xAB;
    mode.CurrentAddress.bytes[5]=0xFE;
    mode.MediaPresentSupported=1;
    mode.MediaPresent=1;
    mode.MaxPacketSize=1500;
    mode.State=2;
    protocol.Mode=&mode;
    boot.LocateHandleBuffer=locate;boot.HandleProtocol=handle;boot.FreePool=free_pool;
    qnet_refresh(&boot,&network);
    CHECK(network.count==1&&freed==1);
    CHECK(!strcmp(network.adapters[0].mac,"52:54:00:12:AB:FE"));
    CHECK(network.adapters[0].mtu==1500&&network.adapters[0].state==2);
    CHECK(network.adapters[0].link_known&&network.adapters[0].link_present);
    mode.MediaPresent=0;mode.MediaPresentSupported=0;
    qnet_refresh(&boot,&network);
    CHECK(network.count==1&&!network.adapters[0].link_known&&!network.adapters[0].link_present);
    puts("PASS: real SNP adapter discovery, MAC, link and MTU");
    return 0;
}
