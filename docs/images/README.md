# README image maintenance

Updated: 2026-09-05. The screenshot gallery is pending; no showcase images were
published during this pass. Do not add empty image links to the main README.

## Required captures

| Filename | Content and method | Status |
| --- | --- | --- |
| `windows-playback.png` | Stable Windows application, Johnny clearly visible; use F12 | Blocked: native capture helper unavailable |
| `windows-settings.png` | Stable application's F1 panel, readable controls | Blocked: native capture helper unavailable |
| `esp32-qemu.png` | Actual 800x480 QEMU framebuffer showing scene playback | Blocked: current graphical profile stops at a diagnostic summary |
| `web-controls.png` | Real authenticated controller, scene navigation and sidebar settings | Blocked: Chrome reported another extension UI open |

Windows stable binary is available in ignored
`build/release/windows-2026.1.0/JohnnyCastaway-Windows-x64.exe`.
Its release source is `f6d292174206a24d53c9f03df6c61053074b5de8`.
F12 writes PNGs to the user's Pictures/Johnny Castaway folder and embeds capture
metadata. Inspect both pixels and metadata before copying images here.

## QEMU investigation

Source: `26c164a479ca70039a161db6d190b3259508ce23`, version `2026.1.0`.
The existing graphical profile rebuilt successfully with ESP-IDF v5.5.5.
Application SHA-256:
`56f452d5c88c20aa22a0b5ecc33d8b69493c2b09e6dbd4c15bccaa0c6cbbb86a`.

QEMU 9.2.2 (`esp_develop_9.2.2_20260417`) exported an actual 800x480
framebuffer using QMP `screendump`. It contains the diagnostic REVIEW COMPLETE
fixture, including synthetic 60 OK / 3 REVIEW counts. This is not gameplay or
the physical acceptance ledger, and must not be used as a showcase image.
The graphical-only branch in `esp32/johnny-esp32/main/main.c` deliberately
presents that summary and waits indefinitely before the continuous playback
loop. Resolving this requires separately scoped QEMU harness work; no source
or firmware behavior was changed for documentation.

Build logs, private merged flash/data, and diagnostic captures remain in ignored
`esp32/build-qemu-graphics-scene-runner/`. Do not commit these artifacts.

## Finish the gallery

1. Restore Windows capture access and dismiss the blocking Chrome extension UI.
   Open the real controller through its existing authenticated session. Do not
   substitute locally reconstructed HTML or invented device state.
2. Resolve the QEMU playback capture prerequisite separately. Use only QEMU for
   the ESP32 display image, not a physical-board photo or a desktop substitute.
3. Capture the four images above as static PNGs. Retain native aspect ratios and
   pixel clarity. Exclude credentials, local addresses, identifying location
   details, personal paths and screenshot confirmation overlays.
4. Record each image's capture date, exact source/version, method and dimensions
   here. Strip unnecessary PNG metadata after inspection. No generated visuals,
   GIFs, or diagnostic fixtures presented as product screenshots.
5. Add a compact gallery after the platform/download links and before the feature
   comparison. Use clickable full-size images with descriptive alt text and
   captions. Caption the ESP32 image "ESP32 firmware running in QEMU" and state
   that it does not demonstrate physical display quality or live weather.
6. Preview at desktop and narrow widths; check readability, sizing, privacy and
   relative links. Stage only the reviewed PNGs and documentation, run
   `git diff --check`, then commit and verify publication.

The README overview and navigation links were completed separately. Browser
visual preview of this pass was blocked by the same Chrome extension UI;
desktop/narrow-width visual acceptance remains open alongside the gallery.
