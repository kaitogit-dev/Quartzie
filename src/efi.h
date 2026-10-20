#ifndef QUARTZIE_EFI_H
#define QUARTZIE_EFI_H
#include <stdint.h>
#include <stddef.h>
#define EFIAPI __attribute__((ms_abi))
typedef uint64_t EFI_STATUS;
typedef void *EFI_HANDLE;
typedef struct { uint32_t a; uint16_t b,c; uint8_t d[8]; } EFI_GUID;
typedef struct { uint64_t signature; uint32_t revision,header_size,crc32,reserved; } EFI_TABLE_HEADER;
typedef struct { uint16_t ScanCode, UnicodeChar; } EFI_INPUT_KEY;
typedef struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *Reset)(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *,uint8_t);
    EFI_STATUS (EFIAPI *ReadKeyStroke)(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *, EFI_INPUT_KEY *);
    void *WaitForKey;
};
typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Header;
    void *RaiseTPL;
    void *RestoreTPL;
    EFI_STATUS (EFIAPI *AllocatePages)(uint32_t,uint32_t,uint64_t,uint64_t *);
    void *reserved_before_free_pool[3];
    EFI_STATUS (EFIAPI *FreePool)(void *);
    void *reserved_before_handle[9];
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE,EFI_GUID *,void **);
    void *reserved_before_stall[11];
    EFI_STATUS (EFIAPI *Stall)(uint64_t);
    void *reserved_before_locate[7];
    EFI_STATUS (EFIAPI *LocateHandleBuffer)(uint32_t,EFI_GUID *,void *,uint64_t *,EFI_HANDLE **);
    EFI_STATUS (EFIAPI *LocateProtocol)(EFI_GUID *, void *, void **);
};
typedef struct {
    EFI_TABLE_HEADER Header;
    uint16_t *FirmwareVendor;
    uint32_t FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    void *ConOut;
    EFI_HANDLE StandardErrorHandle;
    void *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    uint64_t NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;
typedef struct {
    uint32_t Version,HorizontalResolution,VerticalResolution,PixelFormat;
    struct { uint32_t RedMask,GreenMask,BlueMask,ReservedMask; } PixelInformation;
    uint32_t PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;
typedef struct {
    uint32_t MaxMode,Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    uint64_t SizeOfInfo,FrameBufferBase,FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;
typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;
struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *QueryMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL *,uint32_t,uint64_t *,EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **);
    EFI_STATUS (EFIAPI *SetMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL *,uint32_t);
    void *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};
typedef struct {
    uint64_t ResolutionX,ResolutionY,ResolutionZ;
    uint8_t LeftButton,RightButton;
} EFI_SIMPLE_POINTER_MODE;
typedef struct { int32_t RelativeMovementX,RelativeMovementY,RelativeMovementZ; uint8_t LeftButton,RightButton; } EFI_SIMPLE_POINTER_STATE;
typedef struct EFI_SIMPLE_POINTER_PROTOCOL EFI_SIMPLE_POINTER_PROTOCOL;
struct EFI_SIMPLE_POINTER_PROTOCOL {
    EFI_STATUS (EFIAPI *Reset)(EFI_SIMPLE_POINTER_PROTOCOL *,uint8_t);
    EFI_STATUS (EFIAPI *GetState)(EFI_SIMPLE_POINTER_PROTOCOL *,EFI_SIMPLE_POINTER_STATE *);
    void *WaitForInput;
    EFI_SIMPLE_POINTER_MODE *Mode;
};
typedef struct { uint64_t CurrentX,CurrentY,CurrentZ; uint32_t ActiveButtons; } EFI_ABSOLUTE_POINTER_STATE;
typedef struct {
    uint64_t AbsoluteMinX,AbsoluteMinY,AbsoluteMinZ,AbsoluteMaxX,AbsoluteMaxY,AbsoluteMaxZ;
    uint32_t Attributes;
} EFI_ABSOLUTE_POINTER_MODE;
typedef struct EFI_ABSOLUTE_POINTER_PROTOCOL EFI_ABSOLUTE_POINTER_PROTOCOL;
struct EFI_ABSOLUTE_POINTER_PROTOCOL {
    EFI_STATUS (EFIAPI *Reset)(EFI_ABSOLUTE_POINTER_PROTOCOL *,uint8_t);
    EFI_STATUS (EFIAPI *GetState)(EFI_ABSOLUTE_POINTER_PROTOCOL *,EFI_ABSOLUTE_POINTER_STATE *);
    void *WaitForInput;
    EFI_ABSOLUTE_POINTER_MODE *Mode;
};
static const EFI_GUID GOP_GUID = {0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};
static const EFI_GUID MOUSE_GUID = {0x31878c87,0x0b75,0x11d5,{0x9a,0x4f,0x00,0x90,0x27,0x3f,0xc1,0x4d}};
static const EFI_GUID ABS_MOUSE_GUID = {0x8d59d32b,0xc655,0x4ae9,{0x9b,0x15,0xf2,0x59,0x04,0x99,0x2a,0x43}};
#endif
