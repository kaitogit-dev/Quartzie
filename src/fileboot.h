#ifndef QUARTZIE_FILEBOOT_H
#define QUARTZIE_FILEBOOT_H
#include "efi.h"
#include <stddef.h>
#include <stdint.h>
int qefi_read_app(EFI_HANDLE image,EFI_BOOT_SERVICES *boot,uint8_t *buffer,size_t capacity,size_t *length);
#endif
