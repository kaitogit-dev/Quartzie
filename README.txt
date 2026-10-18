Quartzie 0.30 - experimental 32-bit x86 graphical operating system

Boot this El Torito CD using a legacy BIOS emulator. A compatible VBE
adapter is recommended. Quartzie prefers 1920x1080x32 when available.

To save documents, attach a writable CinmixFS/Quartzie persistent disk as
the primary IDE master. Old signed CINMIXFS volumes are supported; keep
backups and do not replace your existing disk with a blank one.

Quartzie includes File Explorer, Settings, Notes, Console, and a native
painting application. Windows PE32 console support is experimental.
It does not run ordinary Windows GUI applications, .NET 10, or GTK4 apps.

For build instructions, current limitations, and project information see
README.md in the source distribution. For release history see CHANGELOG.md.

For release-specific fixes and changes, see CHANGELOG.md in the source archive.

The safe desktop boots without ring-3 workers. Console command useron
enables experimental protected execution; useroff pauses it.
The bundled wallpaper comes from assets/wallpaper.png in the source.
