# Changelog

Release-specific changes belong here. See [README.md](README.md) for the project overview, getting started, usage, and build instructions.

## Quartzie 0.25 — Responsiveness and stability

### Rendering and scheduling

- Avoid repeatedly running idle protected ring-3 worker slices; invoke worker activity only when painting work or a supported PE process needs execution. Rate-limit PE slices in the main loop to reduce desktop timer interference.
- Handle unexpected PIC interrupts during protected-worker slices instead of entering the previous permanent-halt handler.
- Replace large pixel-by-pixel framebuffer transfers with x86 `rep movsl` and solid fills with `rep stosl`; use packed-channel blending and reduce redundant shadow work.
- Preserve the damage-based compositor and clip animated window geometry before expensive drawing. Permit animation-step catch-up after missed timer pulses.
- Keep window maximize at four nominal animation steps; use 18 nominal 120 Hz steps (approximately 150 ms under ideal timing) for notification and glass-panel transitions.
- Reduce IDE timeout polling to limit stalls when a controller does not respond.
- Add `renderstat` to Console to report full, partial, and pointer-only redraw activity and transferred pixel counts.

### Testing

- Add native 1920×1080 Explorer-open/maximize/minimize/restore and notification stress coverage.
- In nine repeated native benchmark runs on the development host, median total rendering time decreased approximately **26% for Explorer opening** and **27% for maximizing** versus 0.24. These are native measurements, **not** QEMU or BIOS-emulator performance measurements.
- Existing desktop, storage, PE32, NT API, and painter regression suites remained passing. Hardware-level boot and freeze resolution have **not** been verified in an emulator.

## Quartzie 0.24 — Rebrand and desktop interaction

- Rename the OS from Cinmix to Quartzie, update the visible identity, and introduce the Q-mark Start icon.
- Make single-click select and double-click open desktop shortcuts and Explorer entries (approximately 300 ms and 6-pixel tolerance). Keyboard Enter still opens selected items.
- Implement `Ctrl+E` for Explorer, Windows/Super key for Start, and `Alt+F4` for closing the active window.
- Add clipboard operations for supported Notes text and disk-backed Explorer files, with current single-root-directory restrictions.
- Add taskbar icon shrinking/overshoot when launching, downward bounce when minimizing, and upward bounce when restoring or activating.
- Retain the existing `CINMIXFS` disk signature and NT device/registry aliases for compatibility with earlier data.

## Earlier releases

Versions before Quartzie 0.24 were released under the Cinmix name. See the [repository commit history](https://github.com/kaitogit-dev/quartzie/commits/main/) for their development history.
