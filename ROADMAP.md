# Castaway Lookout roadmap

Updated: 2026-09-05

Publication status: [release readiness tracker](docs/RELEASE_READINESS_PLAN.md).
Validation listed below is historical evidence; no tests or builds are being
rerun during the documentation/publication pass.

## Current releases

- [x] Windows 11 x64 2026.1.0 stable is published with the desktop application,
  screensaver, per-user installer, shared INI, verified data setup and Castaway
  Lookout icon.
- [x] The ESP32-S3 firmware supports all 63 catalog scenes on the Waveshare
  ESP32-S3-Touch-LCD-7, with Wi-Fi setup, authenticated LAN control,
  Normal/Review playback, persistent settings and Clock/weather/Reviewer
  sidebar modes.
- [x] ESP32 2026.1.0 stable has a reproducible ESP32 release ZIP and installation guide. The
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
- [x] User accepted ESP32 Clock/weather, sidebar controls, switching,
  persistence, smoothness and release-critical fidelity on 2026-09-05.
- [x] User accepted the Windows installer; stable Windows and ESP32 downloads
  were published and byte-verified on 2026-09-05.
- [ ] Complete the full separate-account Windows screensaver lifecycle checklist;
  installer acceptance does not establish all preview/idle/exit scenarios.

## Next engineering work

1. Finish the remaining ESP32 interpreter/lifecycle parity items listed in
   `esp32/ALL_SCENES_FIDELITY.md`, then repeat affected deterministic and panel checks.
2. Complete the Windows CRT performance matrix and separate-account screensaver
   follow-up checklist without reopening already accepted installer behavior.
3. Consider shorter startup checks and weather failure retries as separate work.

## Deferred scope

- ESP32 audio, SD-card resources, OTA, public-internet exposure, Bluetooth,
  screenshots and advanced CRT effects remain out of scope.
- macOS, Linux/XScreenSaver and WebAssembly ports remain deferred.
- Missing original `FLAME.BMP` and `FLURRY.BMP` artwork remains documented; no
  generated replacement will be represented as recovered Sierra data.
