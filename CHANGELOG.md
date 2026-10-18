# Changelog

Release-specific changes belong here. See [README.md](README.md) for the project overview, getting started, usage, and build instructions.

## Quartzie 0.30 — v86 graphics stability

- Render newly opened application windows at their final dimensions immediately, rather than showing intermediate window shells without usable content.
- Reduce glass blur from 8×8 per-tile sampling to scanline color sampling and replace expensive full-area shadow distance calculations with bounded perimeter layers.
- Repaint and present the full framebuffer on structural window/stacking changes to avoid fragmented window-open frames.
- Replace the embedded 256×144 upscale wallpaper with a native-resolution procedural Fluid Layers wallpaper, with crisp geometry and smooth color gradients.
- Keep the existing disk signature and native API compatibility; Windows GUI support remains experimental.

## Quartzie 0.29 — Boot/input stabilization and PNG wallpaper

- **Fix a boot-time kernel stack overlap.** The old protected-mode stack at `0x90000` was inside kernel BSS (`0x32030`–`0xB2000` in 0.28). Move the stack to `0x200000`, separate from kernel state, graphics buffers, and the fixed framebuffer. Verify the non-overlap and stack instruction during every ISO build. This explains a plausible cause of desktop corruption and the static cursor, but does not constitute hardware-level confirmation.
- Disable experimental paging/ring-3 initialization during ordinary desktop startup. It previously ran immediately after drawing the first frame. Retain the functionality as an explicit Console experiment (`useron`, `useroff`, `userstat`). Native desktop apps do not depend on it.
- Vertically center taskbar icons, their activation highlights and running indicators in the middle glass island; fix taskbar hit-test bounds accordingly.
- Replace the CPU-generated wallpaper with one editable `assets/wallpaper.png`. Convert the PNG at build time (Python standard library, no runtime PNG decoder) to RGB565; draw an interpolated copy once and cache it for damage-only redraws. Settings color choices tint the same image.
- Expand the ISO kernel load region from 80 to 160 sectors safely below the relocated stack, and make the ISO validator check the correct region and kernel BSS/stack separation.
- Add a 1920×1080 regression for wallpaper source pixels, top-edge integrity, centered taskbar targets, and moving PS/2 cursor before and after Explorer opens. Preserve existing CinmixFS disk compatibility.
- Native regressions, boot-image structure and code assertions pass. **A real BIOS-emulator boot and input test has not been performed in this environment.** USB HID is not supported.

## Quartzie 0.28 — Stability and Fluid Layers rebuild

- **Fix a confirmed global-buffer overflow:** the old rounded-corner scratch array stored only 18-pixel radii, but the Start menu requested 20 pixels. An AddressSanitizer reproduction on 0.27 reported a global-buffer-overflow in `round_capture`. The replacement uses bounded, matched, nested corner layers.
- Fix further unbalanced corner captures in window buttons, Start, Quick Panel, notifications, and tooltips. Retain the outer window mask until *after* its child content is painted; avoid the old stray second mask operation that used an unrelated buffer.
- Replace striped rectangular shadow outlines with continuous feathered distance-based shadow falloff and subtle refraction borders.
- Replace expensive per-pixel glass saturation with clipped 8-pixel backdrop sampling and stable wallpaper-sourced 3-by-3 averaging; this is a CPU-oriented approximation, **not a hardware-accelerated CSS backdrop-filter**.
- Replace the old blue Aero wallpaper with a violet/cyan ambient gradient, separate the taskbar into floating glass islands, center the new Start menu, and present pinned apps in a modern grid with search and recommendations.
- Preserve the disk format, installed tools, focus behaviors, and native PE32/ring-3 experiments.
- Add `corners28`, `appopen28`, `screens28` and `renderbench28` native tests. `appopen28` exercises 30 cross-application open/maximize/minimize/restore/close cycles and redraw equality. Rounded layer errors are now visible through `renderstat`.
- Native 1920-by-1080 benchmark: across repeated cycles, opening Explorer is roughly four times faster and maximizing about four to five times faster than the previous 0.27 renderer on the development host. **Emulator performance and a real BIOS boot remain unverified**.

## Quartzie 0.27 — Fluid Layers rebuild

### Stability and performance

- Shorten window open and resize animations so app launches feel snappier and repeated activation is less likely to look like a stall.
- Coalesce many small compositor damage rectangles into one bounded redraw region when the desktop is under heavy churn, reducing repeated scene re-rendering during rapid window state changes.
- Replace the previous many-pass perimeter shadow routine with a lighter layered shadow model that keeps depth cues while cutting redundant pixel work.
- Keep the bounded stacking-order and rapid-click safeguards from 0.26.

### Design language

- Replace the Windows 7–style Aero treatment with a Windows 11–inspired **Modular Glassmorphism (Fluid Layers)** look.
- Restyle windows, Start, Quick Panel, notifications, Run, tooltips, Alt+Tab, desktop selection states, and the taskbar as floating polished-glass surfaces.
- Add deeper frosted backdrop treatment, stronger background color sampling, high-saturation glass tinting, finer refractive borders, and richer layered shadows.
- Make focused windows lift more clearly off the canvas while inactive surfaces recede.

## Quartzie 0.26 — Taskbar click freeze fix

- Replace a window hit-test loop that counted down from an ever-increasing stacking-order counter with a bounded scan of at most ten actual open windows.
- Compact window stacking orders before they grow indefinitely, with a bounded maximum of 48. Avoid repeatedly raising the already-active window.
- Make redundant activations idempotent and avoid restarting an already-running taskbar icon bounce of the same kind.
- Add a full-HD stress test that injects 24,000 real PS/2 taskbar click sequences and 15,000 window clicks. It checks that application restoration, event queues, framebuffer presentation and stacking orders stay valid, including near integer overflow.
- Preserve all existing applications, disk format, panel timing, and prior optimizations. Native regression tests do not replace a real BIOS-emulator interaction test.

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
