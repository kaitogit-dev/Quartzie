#ifndef QUARTZIE_NETWORK_H
#define QUARTZIE_NETWORK_H
#include "efi.h"
#define QNET_ADAPTERS 4
typedef struct {
    uint32_t state,mtu;
    int link_known,link_present;
    char mac[18];
} QNetAdapter;
typedef struct {
    int count;
    QNetAdapter adapters[QNET_ADAPTERS];
} QNetwork;
typedef struct {
    uint8_t bytes[32];
} QNetMac;
typedef struct {
    uint32_t State,HwAddressSize,MediaHeaderSize,MaxPacketSize,NvRamSize,NvRamAccessSize;
    uint32_t ReceiveFilterMask,ReceiveFilterSetting,MaxMCastFilterCount,MCastFilterCount;
    QNetMac MCastFilter[16];
    QNetMac CurrentAddress,BroadcastAddress,PermanentAddress;
    uint8_t IfType,MacAddressChangeable,MultipleTxSupported,MediaPresentSupported,MediaPresent;
} QNetMode;
typedef struct QNetProtocol QNetProtocol;
struct QNetProtocol {
    uint64_t Revision;
    void *Start,*Stop,*Initialize,*Reset,*Shutdown,*ReceiveFilters,*StationAddress;
    void *Statistics,*MCastIpToMac,*NvData,*GetStatus,*Transmit,*Receive,*WaitForPacket;
    QNetMode *Mode;
};
void qnet_refresh(EFI_BOOT_SERVICES *boot,QNetwork *network);
#endif
