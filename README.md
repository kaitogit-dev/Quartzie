# Quartzie 0.25 — Responsiveness & Stability

Quartzie is the new name of the freestanding x86 OS previously called Cinmix. It boots by legacy BIOS El Torito CD, defaults to 1920×1080 where supported, and provides an Aero / Dark Aero desktop, persistent IDE disk, and a small PE32 console compatibility layer.

## Stability and performance fixes in 0.25

- **Idle ring-3 work no longer repeatedly interrupts the desktop.** Protected worker slices now run only when there is pending paint work or a running supported PE process; PE slices are rate-limited in the main loop. This avoids repeatedly resetting the PIT and PIC simply to run demonstration spin loops.
- **Unexpected PIC IRQs are handled instead of entering a permanent halt loop** during a protected worker slice. Fault isolation and PE32 support are preserved. The default desktop remains the priority over background worker jobs.
- **Faster 32-bit framebuffer copies** use x86 `rep movsl`; large solid UI fills use `rep stosl`, and translucent color mixing uses packed-channel arithmetic. Repeated shadow blending was reduced. Cursor movement continues using small damage rectangles, not whole-screen repaints.
- **Maximize and Explorer remain interactive.** The existing damage compositor is preserved and window animation geometry is clipped before expensive drawing. Missed PIT frame pulses can advance several tween steps to avoid extending an animation by a full frame per missed tick.
- **Notifications and glass panels now have a more visible tween** (18 nominal 120 Hz steps, about 150 ms under ideal timing), rather than the previous six-step / 50 ms flicker. Window resize retains its four-step target to avoid extra full-HD redraws.
- **Reduced IDE polling timeout** helps avoid extended stalls when a controller does not respond. The existing signed CinmixFS disk format and saved files are unchanged.
- **Console `renderstat` command** reports full redraws, partial redraws, cursor-only frames, and pixels copied to the display for diagnosing expensive UI operations.

**Native benchmark:** On this Linux host, median total measured render time across nine runs decreased by approximately 26% for Explorer opening and 27% for maximizing at 1920×1080, compared with 0.24. These numbers exclude actual virtualization overhead and do **not** predict the frame rate inside QEMU. Notification drawing performs more frames by design to produce a slower transition.

**Important:** Native rendering, ISO structure and simulated interrupt/worker behavior have been tested, but this environment cannot run QEMU/Bochs. These changes reduce known causes of freezing; they cannot yet establish that a specific emulator's freeze is resolved. Report the VM's graphics adapter, RAM, CPU settings and whether a disk image is attached if the problem continues.

## Desktop interaction and taskbar motion

- **Single click selects; double click opens** desktop shortcuts and Explorer files, with a ~300 ms window and a 6-pixel location tolerance. Enter still opens keyboard-selected files, like Windows.
- **Ctrl+E** opens Explorer; **Windows/Super key** (PS/2 extended scancodes 0x5B/0x5C) opens and closes Start; **Alt+F4** closes the active window.
- **Ctrl+C**, **Ctrl+X**, and **Ctrl+V** operate on supported disk-backed files in Explorer. Copy creates a `Copy_` filename on D: and writes the contents to the signed CinmixFS volume. Cut then Paste in the same D: folder is a no-op, because Quartzie currently has only one writable root directory. Without a mounted disk, the UI explains why disk copy is unavailable.
- In Notes, **Ctrl+A** selects the whole document, Ctrl+C/X copy or cut text, and Ctrl+V pastes into the note. The clipboard is shared through OS state but Notes has no rich partial selection yet.
- The taskbar icon has a fast **shrink-and-overshoot** animation on first launch, **downward bounce** on minimize, and **upward bounce** on activation or restore. All motion is bounded and tied to the existing 120 Hz animation clock.
- Every existing feature from Quartzie 0.24 is preserved, including protected ring-3 demo tasks, Pinta-style native painter, Notes, Settings, File Explorer, Dark Aero, and PE32 console loader.

## Disk compatibility and deliberate exceptions

Quartzie retains the on-disk `CINMIXFS` signature, the existing `\Device\CinmixDisk` device path, and `\Registry\Machine\Software\Cinmix` registry compatibility. **Do not reformat old volumes.** Your Cinmix 0.18+ IDE disk can be reused as-is. The included demo disk image contains `hello.exe`, a small PE32 console app.

The IDE driver requires a writable signed disk at the primary IDE master; without one, files are temporary. The filesystem supports 16 slots and up to 4096 bytes per file. Existing limitations still apply: no GUI Win32 app loader, no .NET 10, GTK4/libadwaita, or x64 Windows binaries; Pinta is a native painter port, not upstream Pinta.

## Build and test

```sh
make verify
```

This outputs `Quartzie-0.25.iso`. Use `make Quartzie-0.25-disk.img` for a new blank/demo disk; **back up your existing image** before testing.

Example in QEMU:

```sh
qemu-system-i386 -m 256 -vga std \
 -cdrom Quartzie-0.25.iso -boot d \
 -drive file=Quartzie-0.25-disk.img,format=raw,if=ide,index=0
```

Native UI tests and ISO9660/El Torito structure tests run in Linux; this does not constitute an actual BIOS emulator boot verification. Pinta attribution remains in `third_party/`.
