# Quartzie

An x86-64 UEFI desktop with a framebuffer compositor, native applications, an NT-style services core, and an experimental PE32+ executable loader.

## Build

On Ubuntu:

```sh
sudo apt-get install clang lld xorriso python3-pil fonts-inter make gcc
make verify
make release
```

The release artifacts are `Quartzie-0.38.16-x64.iso` and `Quartzie-0.38.16-x64.img`. GitHub Actions builds and tests both boot formats and publishes them as workflow artifacts.

## Boot

Boot the ISO with x86-64 QEMU and OVMF:

```sh
qemu-system-x86_64 -machine q35 -m 512 -vga std \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \
  -cdrom Quartzie-0.38.16-x64.iso -usb -device usb-tablet
```

For Windows QEMU, use `tools/run-windows.ps1`.

## Controls

| Input | Action |
| --- | --- |
| F1–F6 | Open an application |
| F7 | Application launcher |
| F8 | Notifications |
| F9 | Control center |
| F10 | Minimize |
| F11 | Maximize or restore |
| F12 | Close window |
| Esc | Dismiss overlays |

Drag a window title bar to move it, or to the top edge to maximize. Drag a maximized title bar downward to restore the window. The launcher supports search, arrow-key selection, and Enter.

## PE32+ support

The boot volume includes `QSTART.EXE`. To replace it with an AMD64 PE32+ executable:

```sh
python3 tools/replace_app.py Quartzie-0.38.16-x64.iso program.exe Quartzie-custom.iso
```

The loader implements relocation, import resolution by name and ordinal, and PE export lookup. The current `KERNEL32.dll` bridge implements `WriteFile`, `GetStdHandle`, `GetTickCount`, `GetLastError`, `GetCurrentProcessId`, `GetCurrentThreadId`, `GetCommandLineA`, and `GetModuleHandleA`. This is a targeted compatibility layer, not a complete Win32 environment.

## Source layout

- `src/` — UEFI entry point, compositor, input, native services, PE loader, and Win32 bridge
- `tools/` — image generation, assets, firmware checks, and ISO verification
- `tests/` — native behavior and rendering tests
- `assets/` — original icon and cursor artwork

The bundled Inter font is subject to the terms in `FONT-LICENSE.txt`.
