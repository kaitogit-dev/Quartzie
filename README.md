# Quartzie 0.38.15

## Memory

Framebuffer, desktop, and kernel workspaces are allocated through UEFI Boot Services. The EFI executable reserves only its own code and resources; workspace allocation failures abort startup before rendering.

## Build

Requires Clang, LLD, xorriso, Python 3 with Pillow, and Inter fonts. On Ubuntu:

```sh
sudo apt-get install clang lld xorriso python3-pil fonts-inter make gcc
make verify
```

## Run

Boot `Quartzie-0.38.15-x64.iso` with x86-64 QEMU and OVMF. Example Linux command:

```sh
qemu-system-x86_64 -machine q35 -m 512 -vga std \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \
  -cdrom Quartzie-0.38.15-x64.iso -usb -device usb-tablet
```

For Windows QEMU, use `tools/run-windows.ps1`. It enables display scaling with GTK, if available. To fit 1920×1080 into a smaller host window, use **View → Zoom to Fit** in GTK-based QEMU. SDL display builds may require a larger host window or another display backend. The launcher tries hardware acceleration first and falls back to software emulation.

## Responsiveness and icon quality

Relative mouse input uses calibrated movement at 400 units per reference pixel, with additional pointer sampling immediately after expensive redraws. Application icons are generated directly from their original 64-pixel artwork at native dock, launcher, and small UI sizes instead of repeatedly resampling 48-pixel intermediate images.

## Matched glass translucency

Light and dark glass use the same backdrop/tint opacity curve, so wallpaper colors remain equally visible through both themes. Dark mode retains its independent navy/graphite palette, subtle border highlights, and unchanged wallpaper.

## Dark glass theme

Dark mode uses its own navy/graphite glass palette with cool, subtle edge highlights rather than a light-theme tint inversion. Wallpaper colors are identical in both modes. Window chrome, dock, Start, menus, tooltips, Calculator controls, Settings and Quick Settings all share dark materials. Switching the theme invalidates compositor caches and redraws all visible surfaces without a stale-frame animation.

## Fullscreen maximize

Maximized windows fill the entire Quartzie framebuffer edge to edge. Their outer corners become square and their drop shadow is removed, while the floating dock remains accessible above the window. The maximize/restore button and F11 preserve the original window position and dimensions.

## Desktop features

The Start launcher has an app search field. Type to filter the six installed apps, use Up/Down to move selection, and Enter to launch. Its original 1-6 number shortcuts still work when search is empty.

Calculator supports keyboard input, Enter to evaluate, Backspace to edit, C to clear, chained operations, and a three-entry calculation history. Calculations reject invalid input and division by zero rather than returning misleading values.

F10 minimizes the active window, F11 maximizes or restores it, and F12 closes it.

The About OS and Calculator icons use custom supplied artwork in the dock, Start launcher, and window headers.

## Cursor

Quartzie's default pointer uses the supplied white cursor artwork with a top-left hotspot. The build generates a 32-pixel antialiased framebuffer cursor and `build/Quartzie.cur` with 24, 32, 48 and 64-pixel images. The pointer shares the same coordinates as click and hover detection. It restores only its covered pixels when it moves.

## Controls

- F1–F6: Applications
- F7: App launcher
- F8: Notifications
- F9: Control center
- Escape: Dismiss overlays
- Drag title bars to move windows

## Animation

Windows and the Start menu open with opaque geometric scale transitions. Minimize, close, and Start menu dismissal retain faded exits. The six Start menu app tiles have hover highlights and lift tweens. The Start logo sits slightly higher in the dock without an indicator dot. Dock hover uses partial repainting, and dragging takes priority.

The desktop uses the wallpaper and floating dock without a top panel or Favorites window. Maximized windows extend to the top edge with a small margin. Applications open from the dock, Start menu, or F1–F6.

## Responsiveness

The Windows QEMU launcher uses Quartzie's guest-rendered cursor, so pointer hover, click targeting, and the visible cursor share the same desktop coordinates. This avoids the coordinate drift that occurred when the host cursor was combined with relative PS/2 input under QEMU display scaling.

The PS/2 input path consumes streaming movement packets. Opening and maximizing use elapsed-time easing with a 16 ms target frame interval.

## Rendering

Translucent surfaces use a downsampled, separable two-pass backdrop blur. Glass surfaces have an integrated window header and a soft perimeter shadow. During dragging, only the background behind the active window is cached; the moving window, controls and dock are composited at their current positions. The top bar remains independent of window motion.

## Run a Windows executable

The boot volume contains `QSTART.EXE`. To replace it with a PE32+ AMD64 executable, run:

```sh
python3 tools/replace_app.py Quartzie-0.38.15-x64.img program.exe Quartzie-custom.img
```

Use the ISO as input and output to create a custom ISO instead. Boot the resulting image in QEMU. The executable loader supports relocation, imports by name or ordinal through the resolver, and PE exports. The current `KERNEL32.dll` bridge implements `WriteFile`, `GetStdHandle`, `GetTickCount`, `GetLastError`, `GetCurrentProcessId`, `GetCurrentThreadId`, `GetCommandLineA`, and `GetModuleHandleA`.

## Native services

The NT-style core provides process and thread records, quantum-based scheduling, typed and generation-checked handles, memory-region permissions, in-memory files, syscall dispatch, and driver registration. Tests run a compiled Windows console executable, relocate it, resolve imports, call its entry point, and validate the services.

## Icons

The Start button uses the supplied Quartzie logo, with its transparent artwork generated from `assets/start-icon.png.base64`. It has a hover lift, glow, tooltip, and click pulse. The Start menu itself uses a geometric opening tween and animated app-tile highlights. The logo is also available as a multi-resolution `.ico` for Windows.

The icon assets are stored in `assets/icons.zip`. Building generates `build/icons.c` from the 20 supplied multi-resolution ICO files. The desktop, application launcher, dock, window headers, Files, and Control Center use the matching icons. The atlas keeps transparency and uses the system compositor for blending.

## Fonts

The UI font is rendered at build time from Inter. License and attribution are in `FONT-LICENSE.txt`.
