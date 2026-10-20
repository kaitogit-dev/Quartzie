# Quartzie 0.38.5

## Build

Requires Clang, LLD, xorriso, Python 3 with Pillow, and Inter fonts. On Ubuntu:

```sh
sudo apt-get install clang lld xorriso python3-pil fonts-inter make gcc
make verify
```

## Run

Boot `Quartzie-0.38.5-x64.iso` with x86-64 QEMU and OVMF. Example Linux command:

```sh
qemu-system-x86_64 -machine q35 -m 512 -vga std \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \
  -cdrom Quartzie-0.38.5-x64.iso -usb -device usb-tablet
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

## Responsiveness

The Windows QEMU launcher enables a native host cursor and disables Quartzie's emulated framebuffer cursor through QEMU firmware configuration. Pointer positioning and clicks still reach Quartzie. Other boot methods retain Quartzie's custom pointer. The PS/2 fallback now consumes streaming movement packets rather than requesting individual packets every 12 milliseconds, with a 200 Hz sample rate. The launcher keeps the USB tablet and display scaling enabled.

Opening and maximizing use elapsed-time easing with a 16 ms target frame interval. Frosted-glass rendering does not call firmware mouse services inside drawing loops.

## Rendering

Translucent surfaces use a downsampled, separable two-pass backdrop blur. Glass surfaces have an integrated window header and a soft perimeter shadow. During dragging, only the background behind the active window is cached; the moving window, controls and dock are composited at their current positions. The top bar remains independent of window motion.

## Run a Windows executable

The boot volume contains `QSTART.EXE`. To replace it with a PE32+ AMD64 executable, run:

```sh
python3 tools/replace_app.py Quartzie-0.38.5-x64.img program.exe Quartzie-custom.img
```

Use the ISO as input and output to create a custom ISO instead. Boot the resulting image in QEMU. The executable loader supports relocation, imports by name or ordinal through the resolver, and PE exports. The current `KERNEL32.dll` bridge implements `WriteFile`, `GetStdHandle`, `GetTickCount`, `GetLastError`, `GetCurrentProcessId`, `GetCurrentThreadId`, `GetCommandLineA`, and `GetModuleHandleA`.

## Native services

The NT-style core provides process and thread records, quantum-based scheduling, typed and generation-checked handles, memory-region permissions, in-memory files, syscall dispatch, and driver registration. Tests run a compiled Windows console executable, relocate it, resolve imports, call its entry point, and validate the services.

## Icons

The icon assets are stored in `assets/icons.zip`. Building generates `build/icons.c` from the 20 supplied multi-resolution ICO files. The desktop, application launcher, dock, window headers, Files, and Control Center use the matching icons. The atlas keeps transparency and uses the system compositor for blending.

## Fonts

The UI font is rendered at build time from Inter. License and attribution are in `FONT-LICENSE.txt`.
