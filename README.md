# Quartzie

An x86-64 UEFI desktop with glass windows, native applications, persistent profiles, personal wallpapers, FAT storage, a native services core, and PE32+ executable support.

## Build

On Ubuntu, install clang, lld, xorriso, ovmf, qemu-system-x86, python3-pil, fonts-inter, make, and gcc.

```sh
make verify
make release
```

GitHub Actions builds Quartzie-0.44.0-x64.iso, Quartzie-0.44.0-x64.img, and Quartzie-0.44.0-userdata.img and runs firmware boot and application tests.

## Boot

Run `tools/run-windows.ps1` from PowerShell with the ISO and persistent data disk alongside it or in Downloads. It uses silent QEMU audio by default to avoid DirectSoundCapture failures. On a working audio device, use `-AudioBackend dsound` to enable PC speaker playback. Use `-BootMode disk` to start from the writable UEFI disk image when testing firmware updates and recovery instead of the read-only ISO.

For QEMU/OVMF on Linux, boot the ISO and attach Quartzie-userdata.img as a writable FAT disk. Keep the same user disk across OS updates.

## Desktop and applications

Files supports multiple views, search, sorting, clipboard operations and a Recycle Bin for Home deletions. Restore, Delete Forever and Empty Bin are available from Files. Task Manager lists native processes and desktop applications.

Window snapping supports sides, corners and maximize-hover layouts. Three virtual desktops are accessible from the dock; Tab cycles through open windows.

Notes edits and saves text files. Photos supports BMP, PNG, JPEG, GIF first-frame and TGA, rotates and mirrors images, exports BMP, and sets the current photo as a wallpaper. Media Player offers WAV, MP3 and uncompressed AVI playback with track navigation, repeat and volume controls.

Settings includes light and dark modes, eight accent colors, wallpaper presets, account profiles and screen locking with an optional account PIN. User folders and appearance preferences persist on the writable Home disk. The firmware image does not provide encrypted isolation of profile files.

## Quartzie Update and Recovery

Settings > Update & Recovery can locate a local `QUARTZIE.UPD` package in a FAT volume root or in the active user's Downloads folder. Generate an update from an x86-64 UEFI application using:

```sh
python3 tools/update.py pack build/BOOTX64.EFI -o QUARTZIE.UPD --version 0.44.0
```

Installation validates the UEFI payload and its SHA-256 digest, stages the new EFI binary, backs up the existing firmware, and verifies the result. The target system partition must be writable; ISO media is not. Keep the previous `QUARTZIE.BAK` binary until you're satisfied the update boots. Restore the backup by booting a working Quartzie recovery ISO with the affected disk attached and selecting Roll Back. A backup must be rolled back or explicitly cleared before installing a further firmware update.

The Update & Recovery page runs boot diagnostics covering the EFI image, saved backup, Home storage, profile folder and network hardware. The same report is available using `diagnose` in Terminal. It also restores the preceding saved appearance configuration. These operations use local media; the networking stack currently provides adapter diagnostics rather than Internet update downloads.

Quartzie Store converts EXE/APK packages to `.qapp` for distribution and installs verified archives, while execution still depends on the corresponding runtime.
