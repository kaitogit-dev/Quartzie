# Quartzie

An x86-64 UEFI desktop with a framebuffer compositor, native applications, a UEFI filesystem browser, an NT-style services core, and an experimental PE32+ executable loader.

## Build

On Ubuntu:

```sh
sudo apt-get install clang lld xorriso python3-pil fonts-inter make gcc
make verify
make release
```

The release artifacts are `Quartzie-0.43.0-x64.iso` and `Quartzie-0.43.0-x64.img`. GitHub Actions builds and tests both boot formats and publishes them as workflow artifacts.

## Boot

Boot the ISO with x86-64 QEMU and OVMF:

```sh
qemu-system-x86_64 -machine q35 -m 512 -vga std \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \
  -cdrom Quartzie-0.43.0-x64.iso -usb -device usb-tablet
```

For Windows QEMU, use `tools/run-windows.ps1`.

## Controls

| Input | Action |
| --- | --- |
| F1–F6 | Open a pinned application |
| F7 | Application launcher |
| F8 | Notifications |
| F9 | Control center |
| F10 | Minimize |
| F11 | Maximize or restore |
| F12 | Close window |
| Esc | Dismiss overlays |

Drag a window title bar to move it, or to the top edge to maximize. Drag a maximized title bar downward to restore the window. The launcher supports search, arrow-key selection, and Enter.

## Files and media

Files uses single-click selection and double-click opening, with Enter opening the selected entry. Files enumerates UEFI-readable FAT volumes, including the boot volume and attached FAT-formatted disks. A writable Home RAM volume is also available for files and folders during the current session; its contents reset after reboot. Browse folders, create text files or directories, delete entries, and open text files in Notes. Notes saves to the selected writable file.

Photos opens BMP, PNG, JPEG, GIF (first frame), and TGA. Media Player supports MP3, PCM WAV (8/16-bit mono or stereo), and uncompressed AVI video with 24/32-bit BGR frames. Audio uses the PC speaker interface, so audible output requires an emulated or physical PC speaker audio path. MP4, H.264, and AAC remain unsupported.

The boot volume includes `PHOTO.BMP`, `SOUND.WAV`, and `CLIP.AVI` as viewer test files. Photos and Media Player are available from Start and are not pinned to the dock. To keep files across ISO boots, attach a separate writable FAT-formatted disk.

## PE32+ support

The boot volume includes `QSTART.EXE`. To replace it with an AMD64 PE32+ executable:

```sh
python3 tools/replace_app.py Quartzie-0.43.0-x64.iso program.exe Quartzie-custom.iso
```

The loader implements relocation, import resolution by name and ordinal, and PE export lookup. The current `KERNEL32.dll` bridge implements `WriteFile`, `GetStdHandle`, `GetTickCount`, `GetLastError`, `GetCurrentProcessId`, `GetCurrentThreadId`, `GetCommandLineA`, and `GetModuleHandleA`. This is a targeted compatibility layer, not a complete Win32 environment.

## Source layout

- `src/` — UEFI entry point, compositor, input, native services, PE loader, and Win32 bridge
- `tools/` — image generation, assets, firmware checks, and ISO verification
- `tests/` — native behavior and rendering tests
- `assets/` — original icon and cursor artwork

The bundled Inter font is subject to the terms in `FONT-LICENSE.txt`. Media codec notices are in `LICENSES/media-codecs.txt`.

Persistent user files use `Quartzie-0.43.0-userdata.img`, a writable FAT16 volume included with each build. Attach it as a USB mass-storage disk alongside the ISO. Quartzie discovers it from its volume marker and creates `Users\\Default` with Desktop, Documents, Downloads, Pictures, Music, and Videos. Reuse the same data disk between boots to retain files. `tools/run-windows.ps1` attaches the data disk automatically. Without this disk Quartzie falls back to temporary Home RAM.

Desktop switcher: the small three-bar control at the right end of the dock opens a choice of three desktops. Click the upper half of a desktop tile to switch; click its bottom row to move the currently focused window there. Hover the maximize button to choose a snap zone. Use Tab to bring forward another open window on the current desktop.

Light/dark mode, accent and animation preferences persist in `QUARTZIE.CFG` on Home between boots with the same user-data disk.

Quartzie Store now accepts native `.qapp` packages from Files. Open a verified `.qapp` in Explorer, confirm Install in Store, and the package is copied to persistent `Users\\Default\\Applications` and independently verified after writing. The Installed packages tab lists and removes packages with a two-click confirmation. Use `tools/qapp.py pack` to convert EXE/APK uploads to signed-off `.qapp` archives with manifest and SHA-256 data. Installation does not automatically execute an application; program execution depends on its supported runtime.

Photos now supports non-destructive 90-degree rotation, horizontal mirroring, browse previous/next images and Save BMP, which writes a new 24-bit bitmap to the writable Home Pictures folder. Media Player now supports previous/next tracks in the current directory, repeat mode, volume adjustment and muting; auto-next advances when playback finishes. Audio still uses the PC speaker on the current firmware backend.

Network diagnostics now use UEFI Simple Network Protocol to enumerate actual adapters and read their MAC addresses, link state and MTUs. See Settings > Network or run `netinfo` in Terminal. This release provides hardware discovery and link diagnostics; TCP/IP, DNS, Internet downloads, and browsing require additional networking stack work.
