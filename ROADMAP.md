# Castaway Lookout roadmap

Updated: 2026-09-05

Publication status: [release readiness tracker](docs/RELEASE_READINESS_PLAN.md).
Validation listed below is historical evidence; no tests or builds are being
rerun during the documentation/publication pass.

## Current releases

- [x] Windows 11 x64 RC3 is published with the desktop application,
  screensaver, per-user installer, shared INI, verified data setup and Castaway
  Lookout icon.
- [x] The ESP32-S3 firmware supports all 63 catalog scenes on the Waveshare
  ESP32-S3-Touch-LCD-7, with Wi-Fi setup, authenticated LAN control,
  Normal/Review playback, persistent settings and Clock/weather/Reviewer
  sidebar modes.
- [x] RC4 has a reproducible ESP32 release ZIP and one-page flashing guide. The
  ZIP omits copyrighted Sierra data and creates `jcdata.bin` locally from the
  user's hash-verified originals.

## Release gates

- [x] Windows Go regression suite, native x64 application/screensaver builds,
  architecture checks, installer build and source/data boundary checks.
- [x] ESP32 Python/catalog tests, uncached Go tests, normal ESP-IDF build,
  package construction, firmware checksums, private data-image equivalence,
  positive N16R8 identification, verified release-script flash and hard reset.
- [x] All 63 ESP32 scenes were physically reviewed and accepted in the closed
  review ledger.
- [ ] Complete direct-panel acceptance of the latest Clock/weather layout,
  colour icons, update timestamp, Reviewer/Off switching, reboot persistence,
  long-run smoothness and stale-weather presentation.
- [ ] Test the Windows installer and screensaver lifecycle on a genuinely clean
  Windows 11 account or machine before promoting to stable.

## Next engineering work

1. Close the remaining ESP32 physical sidebar/control gate without inferring
   visual success from serial, framebuffer hashes or QEMU.
2. Finish the remaining ESP32 interpreter/lifecycle parity items listed in
   `esp32/ALL_SCENES_FIDELITY.md`, then repeat affected deterministic and panel
   checks.
3. Complete the Windows CRT performance matrix on the documented resolutions
   and at least one lower-powered GPU.
4. Promote the verified candidates to stable only after their remaining
   physical smoke tests pass.

## Deferred scope

- ESP32 audio, SD-card resources, OTA, public-internet exposure, Bluetooth,
  screenshots and advanced CRT effects remain out of scope.
- macOS, Linux/XScreenSaver and WebAssembly ports remain deferred until
  the stable Windows and ESP32 release gates are closed.
- Missing original `FLAME.BMP` and `FLURRY.BMP` artwork remains documented; no
  generated replacement will be represented as recovered Sierra data.
