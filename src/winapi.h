#ifndef QUARTZIE_WINAPI_H
#define QUARTZIE_WINAPI_H
#include <stdint.h>
#include <stddef.h>
#include "efi.h"
#include "ntcore.h"
typedef void (*QWinSerial)(uint8_t byte);
int qwin_execute(QNtKernel *kernel,EFI_BOOT_SERVICES *services,const uint8_t *exe,size_t bytes,QWinSerial writer);
#endif
