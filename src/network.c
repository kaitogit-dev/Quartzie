#include "network.h"
static const EFI_GUID NETWORK_GUID={0xa19832b9,0xac25,0x11d3,{0x9a,0x2d,0x00,0x90,0x27,0x3f,0xc1,0x4d}};
static char hex(unsigned v){return (char)(v<10?'0'+v:'A'+v-10);}
void qnet_refresh(EFI_BOOT_SERVICES *boot,QNetwork *network){
    if(!network)return;
    QNetwork result={0};
    if(boot&&boot->LocateHandleBuffer&&boot->HandleProtocol){
        EFI_HANDLE *handles=0;
        uint64_t found=0;
        if(boot->LocateHandleBuffer(2,(EFI_GUID *)(void *)&NETWORK_GUID,0,&found,&handles)==0&&handles){
            for(uint64_t i=0;i<found&&result.count<QNET_ADAPTERS;i++){
                QNetProtocol *protocol=0;
                if(boot->HandleProtocol(handles[i],(EFI_GUID *)(void *)&NETWORK_GUID,(void **)&protocol)!=0||
                   !protocol||!protocol->Mode)continue;
                QNetMode *mode=protocol->Mode;
                QNetAdapter *adapter=&result.adapters[result.count++];
                adapter->state=mode->State;
                adapter->mtu=mode->MaxPacketSize;
                adapter->link_known=mode->MediaPresentSupported!=0;
                adapter->link_present=mode->MediaPresent!=0;
                int address_length=(int)mode->HwAddressSize;
                if(address_length>6)address_length=6;
                int at=0;
                for(int j=0;j<address_length;j++){
                    uint8_t address=mode->CurrentAddress.bytes[j];
                    adapter->mac[at++]=hex(address>>4);
                    adapter->mac[at++]=hex(address&15);
                    if(j+1<address_length)adapter->mac[at++]=':';
                }
                if(!at){adapter->mac[0]='?';at=1;}
                adapter->mac[at]=0;
            }
            if(boot->FreePool)boot->FreePool(handles);
        }
    }
    *network=result;
}
