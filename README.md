# Quartzie 0.33.0

## Build

Requires Clang, LLD, xorriso, Python 3 with Pillow, and Inter fonts. On Ubuntu:

```sh
sudo apt-get install clang lld xorriso python3-pil fonts-inter make gcc
make verify
```

## Run

Boot `Quartzie-0.33.0-x64.iso` with x86-64 QEMU and OVMF. Example Linux command:

```sh
qemu-system-x86_64 -machine q35 -m 512 -vga std \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \
  -cdrom Quartzie-0.33.0-x64.iso -usb -device usb-tablet
```

For Windows QEMU, use `qemu-system-x86_64.exe` and the firmware files provided by the QEMU installer. Add `-usb -device usb-tablet` for UEFI tablet input; Quartzie also includes a PS/2 mouse fallback.

## Controls

- F1–F6: Applications
- F7: App launcher
- F8: Notifications
- F9: Control center
- Escape: Dismiss overlays
- Drag title bars to move windows

## Current limits

The EFI application retains firmware services during operation. Notes are volatile, Files has no mounted filesystem, and process isolation, sound and networking are not implemented. Native QEMU with OVMF is the supported testing environment. The unsigned EFI image requires Secure Boot to be disabled.

## Fonts

The UI font is rendered at build time from Inter. License and attribution are in `FONT-LICENSE.txt`.
