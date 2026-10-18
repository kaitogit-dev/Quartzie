# Quartzie x64

Quartzie x64 is a newly written experimental graphical desktop for x86-64 machines using UEFI firmware. It is not a port of the former 32-bit kernel. The old BIOS boot sector, protected-mode entry, window compositor and PS/2 packet handling have been replaced with a freestanding PE32+ UEFI application, native GOP framebuffer renderer and UEFI keyboard/pointer input.

## Important architecture note

This is a **UEFI-hosted desktop prototype**, not yet an independent kernel that calls `ExitBootServices`. It does not provide kernel isolation, a process model, persistent filesystem access, network access, native hardware drivers or general executable loading. The Files window is a placeholder and scratchpad changes remain in memory. It is not Windows and does not include Microsoft code.

UEFI is the modern PC firmware interface used by current Windows installations. Optical UEFI bootable ISO images still require an El Torito EFI boot catalog; Quartzie includes one for ISO compatibility. The GPT disk image instead boots through a FAT32 EFI System Partition and `EFI/BOOT/BOOTX64.EFI`.

## Build

On Linux, install Clang, LLD (`lld-link`), Python 3, GCC, Make and xorriso.

```sh
make verify
```

Artifacts:

- `Quartzie-x64.iso`: UEFI optical image.
- `Quartzie-x64-uefi.img`: GPT disk image containing a FAT32 EFI System Partition.
- `build/BOOTX64.EFI`: PE32+ EFI application.

## Run in QEMU

Use OVMF (UEFI firmware), a display with GOP and a supported UEFI mouse implementation.

```sh
qemu-system-x86_64 -machine q35 -m 512 -vga std -bios /usr/share/OVMF/OVMF_CODE.fd -cdrom Quartzie-x64.iso
```

OVMF paths vary between Linux distributions. Disable Secure Boot for this unsigned test image. Do not write this preview disk image to a real drive containing data. Use a fresh virtual machine.

## UI

The new interface has an independent click router for Start, six application shortcuts, notifications and quick settings. Windows can be opened, focused, dragged and closed. The desktop renders glass layers with softened cached backdrops, rounded edges and distance-falloff shadows. Pointer-only movement updates just the cursor pixels instead of repainting the whole screen.

F1 through F6 open Files, Notes, Terminal, Calculator, Settings and About. F7 toggles Start, F8 toggles notifications, F9 toggles quick settings, and Escape closes popups. These shortcuts provide navigation if the firmware does not expose a mouse protocol.

## Verification

`make verify` checks x64 PE/COFF EFI properties, FAT32 directory placement, ISO EFI boot catalog, GPT checksums, and desktop/input logic in native unit tests. Static verification is not a substitute for an OVMF boot test on the desired emulator or hardware.
