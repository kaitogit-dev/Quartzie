# Quartzie 0.24 — First rebranded release

Quartzie is the new name of the freestanding x86 OS previously called Cinmix. It boots by legacy BIOS El Torito CD, defaults to 1920×1080 where supported, and provides an Aero / Dark Aero desktop, persistent IDE disk, and a small PE32 console compatibility layer.

## Desktop interaction and taskbar motion

- **Single click selects; double click opens** desktop shortcuts and Explorer files, with a ~300 ms window and a 6-pixel location tolerance. Enter still opens keyboard-selected files, like Windows.
- **Ctrl+E** opens Explorer; **Windows/Super key** (PS/2 extended scancodes 0x5B/0x5C) opens and closes Start; **Alt+F4** closes the active window.
- **Ctrl+C**, **Ctrl+X**, and **Ctrl+V** operate on supported disk-backed files in Explorer. Copy creates a `Copy_` filename on D: and writes the contents to the signed CinmixFS volume. Cut then Paste in the same D: folder is a no-op, because Quartzie currently has only one writable root directory. Without a mounted disk, the UI explains why disk copy is unavailable.
- In Notes, **Ctrl+A** selects the whole document, Ctrl+C/X copy or cut text, and Ctrl+V pastes into the note. The clipboard is shared through OS state but Notes has no rich partial selection yet.
- The taskbar icon has a fast **shrink-and-overshoot** animation on first launch, **downward bounce** on minimize, and **upward bounce** on activation or restore. All motion is bounded and tied to the existing 120 Hz animation clock.
- Every existing feature from Cinmix 0.23 is preserved, including protected ring-3 demo tasks, Pinta-style native painter, Notes, Settings, File Explorer, Dark Aero, and PE32 console loader.

## Disk compatibility and deliberate exceptions

Quartzie retains the on-disk `CINMIXFS` signature, the existing `\Device\CinmixDisk` device path, and `\Registry\Machine\Software\Cinmix` registry compatibility. **Do not reformat old volumes.** Your Cinmix 0.18+ IDE disk can be reused as-is. The included demo disk image contains `hello.exe`, a small PE32 console app.

The IDE driver requires a writable signed disk at the primary IDE master; without one, files are temporary. The filesystem supports 16 slots and up to 4096 bytes per file. Existing limitations still apply: no GUI Win32 app loader, no .NET 10, GTK4/libadwaita, or x64 Windows binaries; Pinta is a native painter port, not upstream Pinta.

## Build and test

```sh
make verify
```

This outputs `Quartzie-0.24.iso`. Use `make Quartzie-0.24-disk.img` for a new blank/demo disk; **back up your existing image** before testing.

Example in QEMU:

```sh
qemu-system-i386 -m 256 -vga std \
 -cdrom Quartzie-0.24.iso -boot d \
 -drive file=Quartzie-0.24-disk.img,format=raw,if=ide,index=0
```

Native UI tests and ISO9660/El Torito structure tests run in Linux; this does not constitute an actual BIOS emulator boot verification. Pinta attribution remains in `third_party/`.
