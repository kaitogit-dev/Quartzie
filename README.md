# Quartzie

Quartzie is an experimental desktop operating system for 32-bit x86 PCs and virtual machines. It boots from a **legacy BIOS / El Torito ISO** and provides a **Modular Glassmorphism (Fluid Layers)** desktop with Windows 11–inspired floating panels, software-sampled glass, and window management, File Explorer, Settings, Notes, Console, Calendar, Calculator, and a native painting app.

The project was previously called **Cinmix**. Its Modular Glassmorphism (Fluid Layers) interface and limited PE32 console support are works in progress; Quartzie is **not Windows NT**, and it does not run arbitrary Windows programs.

## Getting started

Download the ISO and the **separate writable disk image** from the current release, then boot them together in a legacy-BIOS emulator. For example, with QEMU (256 MiB RAM and PS/2 input):

```sh
qemu-system-i386 -m 256 -vga std \
  -cdrom Quartzie-0.30.iso -boot d \
  -drive file=Quartzie-0.30-disk.img,format=raw,if=ide,index=0
```

A PS/2 keyboard and mouse are required (there is no USB HID driver yet). The preferred desktop resolution is **1920×1080 at 32-bit color** if the virtual display supports it; otherwise the bootloader chooses a supported fallback. The IDE hard disk must be attached as the **primary master** to enable persistence. Quartzie does **not** automatically format unknown disks.

**Keep your existing disk image** when updating Quartzie. The on-disk `CINMIXFS` signature remains compatible with older Cinmix/Quartzie disks. Make a backup before testing a new build; downloading a blank disk will not carry over your files.

## Desktop and applications

- **File Explorer:** Browse the CinmixFS volume, create and select documents, search, change view/sort settings, and open supported files. A double-click opens items; a single click selects them.
- **Settings:** Switch Light Glass / Dark Glass themes, wallpaper, accent colors and glass strength; manage notification and software-volume settings.
- **Notes:** Edit text and save it to the attached disk using `Ctrl+S`.
- **Painter:** A native Pinta-style painting application with pencil, brush, eraser, shapes, fill, color selection, undo/redo and persistent saving. It is **not** the upstream Pinta GTK/.NET executable.
- **Other desktop tools:** Console, System information, Calculator, Clock, Calendar, and a floating glass taskbar with window animations.

### Keyboard and mouse shortcuts

| Input | Action |
| --- | --- |
| Single click / double-click | Select / open an item |
| Windows/Super key | Toggle Start |
| `Ctrl+E` | Open File Explorer |
| `Alt+F4` | Close the active window |
| `Alt+Tab` | Switch windows |
| `Ctrl+C`, `Ctrl+X`, `Ctrl+V` | Copy, cut, paste in supported apps |
| `Ctrl+S` | Save in Notes or Painter |
| `Ctrl+R` | Open Run |

Shortcuts depend on the focused application. File clipboard operations currently work within the single writable volume and are more limited than on Windows.

## Building from source

On Linux, install GCC with 32-bit compilation support, GNU binutils, Python 3, and `make`. The wallpaper PNG conversion uses only the Python standard library.

```sh
make                 # ISO and demo disk image
make verify          # ISO/boot-image validation plus native regression tests
make clean
```

The build creates `Quartzie-0.30.iso` and `Quartzie-0.30-disk.img`. The disk target creates a **new** image—do not overwrite one containing data. You can run the ISO in a compatible legacy-BIOS emulator using the command above.

### Code layout

| Path | Purpose |
| --- | --- |
| `src/kernel.c` | Desktop, compositor, input, and built-in applications |
| `src/proc.c`, `src/proc.S` | Paging, ring-3 workers, interrupts, and scheduler |
| `src/ntos.c`, `src/objects.c` | Experimental NT-style native I/O and object handles |
| `src/storage.c` | ATA PIO disk driver and persistent CinmixFS storage |
| `src/pe.c` | Experimental 32-bit PE console executable loader |
| `src/pinta.c` | Native painting functionality |
| `src/cdboot.S`, `src/entry.S` | BIOS CD boot, protected-mode entry, and disjoint 2 MiB kernel stack |
| `assets/wallpaper.png` | Single replaceable image used for the cached desktop wallpaper |
| `tools/wallpaper.py` | Converts the PNG at build time into a small RGB565 resource |
| `tools/` | ISO, disk-image, and test utilities |
| `tests/` | Native regression, storage, UI, and rendering tests |

## Capabilities and limitations

Quartzie is a **hobby OS**, not a Windows replacement. Current limitations include:

- The signed CinmixFS volume supports **16 file slots** with a maximum of **4 KiB per file**; it is not FAT, NTFS, or a general-purpose filesystem.
- The PE32 compatibility layer handles a narrow set of **32-bit console** executables and `KERNEL32` imports. It does not support general Windows GUI applications, x64 PE files, .NET 10, GTK4/libadwaita, or Windows DLL loading in general.
- Experimental protected ring-3 workers are **off by default** for a safer desktop boot. In Console, `useron` enables the experimental subsystem, `useroff` pauses its scheduling, and `userstat` reports its state. Windows PE32 demos require `useron`; the desktop, Explorer, Notes, and Painter work without it. Most desktop applications still execute in the kernel's address space.
- Glass translucency and blur are CPU-rendered approximations sampled from the cached PNG-derived wallpaper; they are not CSS or GPU backdrop filtering. Full cross-window backdrop sampling is not implemented.
- Audio volume is a software UI setting; there is no working audio device driver.
- `make verify` exercises native tests and verifies the ISO structure. It does **not** prove successful boot or smooth performance inside QEMU/Bochs. A real emulator run is important before treating any hardware-level change as confirmed.

## Documentation and history

See [CHANGELOG.md](CHANGELOG.md) for changes between releases. See [the source repository](https://github.com/kaitogit-dev/quartzie) for the issue tracker, history, and development source.

The bundled font license is in [FONT-LICENSE.txt](FONT-LICENSE.txt). Pinta-related attribution and licensing information are in [`third_party/`](third_party/).
