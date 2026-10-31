# Quartzie

An x86-64 UEFI desktop with a glass compositor, native applications, FAT storage, virtual desktops, a native services core, and an experimental PE32+ compatibility layer.

## Build

Ubuntu dependencies: `clang`, `lld`, `xorriso`, `ovmf`, `qemu-system-x86`, `python3-pil`, `fonts-inter`, `make`, and `gcc`.

```sh
make verify
make release
```

GitHub Actions verifies the firmware and application tests, boots both ISO and GPT images under OVMF, and publishes the ISO, UEFI disk, and writable Home disk.

## Boot and storage

The boot ISO is `Quartzie-0.43.1-x64.iso`. Attach `Quartzie-0.43.1-userdata.img` as a writable USB mass-storage disk to retain user files and preferences between sessions. The Windows QEMU launcher `tools/run-windows.ps1` attaches the disk automatically. Keep the same data image when upgrading.

Quartzie uses `Users\\Default` with Desktop, Documents, Downloads, Pictures, Music, Videos, and Applications. If the user disk is missing, Home falls back to temporary RAM storage.

Files offers folder browsing, multiple views, searching, sorting, multi-selection, copying, moving, and renaming. Home deletions move to **Recycle Bin**, with restore, permanent removal, and empty-bin operations. Deletions from other volumes require explicit confirmation and are permanent. Settings > Storage shows capacity and free space for volumes that report it.

## Desktop and applications

Drag window title bars to screen edges or corners for snapping; hover over Maximize for snap layouts. The dock switcher offers three desktops and lets you move the active window between them. Tab cycles through windows on the active desktop. Task Manager lists running apps and native processes, and can terminate selected tasks.

Notes supports editable text and saving to Documents. Calculator supports arithmetic precedence and keeps recent results. Photos opens BMP, PNG, JPG, GIF (first frame), and TGA; it can rotate, mirror, browse pictures, and export BMP. Media Player supports MP3, PCM WAV, and uncompressed AVI, with track navigation, repeat and volume controls. Audio uses the PC speaker backend.

Quartzie Store installs verified `.qapp` archives from Files to the user disk and supports removal. Convert EXE/APK payloads using `tools/qapp.py`; installing a package does not imply runtime support. The PE32+ loader can run supported test executables through a limited Win32 bridge; Android application execution remains under development.

UEFI Simple Network Protocol is used for real adapter, MAC, link, and MTU diagnostics in Settings > Network and Terminal `netinfo`. TCP/IP, DNS and web browsing are not yet integrated.

## Test executable

The boot volume includes `QSTART.EXE`. To substitute a supported AMD64 PE32+ program:

```sh
python3 tools/replace_app.py Quartzie-0.43.1-x64.iso program.exe Quartzie-custom.iso
```

Font and codec notices are retained in `FONT-LICENSE.txt` and `LICENSES/media-codecs.txt`.
