# Quartzie 0.37.1

## Build

Requires Clang, LLD, xorriso, Python 3 with Pillow, and Inter fonts. On Ubuntu:

```sh
sudo apt-get install clang lld xorriso python3-pil fonts-inter make gcc
make verify
```

## Run

Boot `Quartzie-0.37.1-x64.iso` with x86-64 QEMU and OVMF. Example Linux command:

```sh
qemu-system-x86_64 -machine q35 -m 512 -vga std \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \
  -cdrom Quartzie-0.37.1-x64.iso -usb -device usb-tablet
```

For Windows QEMU, use `tools/run-windows.ps1`. It enables display scaling with GTK, if available. To fit 1920×1080 into a smaller host window, use **View → Zoom to Fit** in GTK-based QEMU. SDL display builds may require a larger host window or another display backend. The launcher tries hardware acceleration first and falls back to software emulation.

## Controls

- F1–F6: Applications
- F7: App launcher
- F8: Notifications
- F9: Control center
- Escape: Dismiss overlays
- Drag title bars to move windows

## Animation

Windows opening and maximizing use opaque geometric scale transitions. Minimize and close retain faded exits. The app launcher, notifications and control center keep their cached-frame transitions. Window geometry is interpolated over a cached backdrop without recalculating the glass blur every frame. Dock hover stays responsive through partial repainting. Dragging always takes priority.

## Rendering

Translucent surfaces use a downsampled, separable two-pass backdrop blur. Glass surfaces have an integrated window header and a soft perimeter shadow. During dragging, only the background behind the active window is cached; the moving window, controls and dock are composited at their current positions. The top bar remains independent of window motion.

## Current limits

The EFI application retains firmware services during operation. Notes are volatile, Files has no mounted filesystem, and process isolation, sound and networking are not implemented. Native QEMU with OVMF is the supported testing environment. The unsigned EFI image requires Secure Boot to be disabled.

## Fonts

The UI font is rendered at build time from Inter. License and attribution are in `FONT-LICENSE.txt`.
