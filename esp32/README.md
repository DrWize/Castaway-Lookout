# Johnny Castaway ESP32-S3

This directory contains the native ESP-IDF port for the Waveshare
ESP32-S3-Touch-LCD-7 (ESP32-S3, 16 MB flash, 8 MB PSRAM). It renders the
original 640x480 Johnny Castaway resources without scaling inside the 800x480
panel and reserves 160 pixels for the optional side bar.

The current firmware has physically presented and accepted all 63 ordered story
scenes. The final hard reset and serial export confirmed
`63 OK / 0 REVIEW / 0 unreviewed`, catalog fingerprint `eb04eb66746ae074` and
`REVIEW-ONLY: ALL RESOLVED`. Broader interpreter parity, rewind/z-order
coverage, full story/walk lifecycle and final smoothness testing remain active
work.

## Documentation map

- [`RUNBOOK.md`](RUNBOOK.md) — start here for the current pickup, exact commands,
  validation profiles and end-of-run handoff.
- [`ALL_SCENES_FIDELITY.md`](ALL_SCENES_FIDELITY.md) — ordered list of every
  discovered scene repair and remaining engine-wide fidelity task.
- [`SCENE_REVIEW.md`](SCENE_REVIEW.md) — authoritative physical review evidence
  and historical 63-scene classifications.
- [`PLAN.md`](PLAN.md) — architecture, phase order, later touch/network features
  and phase completion gates.
- [`SCENE_CATALOG.md`](SCENE_CATALOG.md) — generated friendly-name catalog and
  source mapping.
- [`../TODO.md`](../TODO.md) — repository-level status and resumable checkpoints.

`AGENTS.md` and `../AGENTS.md` contain the working rules for this directory.
Original Sierra/Dynamix data, generated binaries and local build output must not
be committed.

## Implemented foundation

- Repo-local ESP-IDF v5.5.5 builds for the ESP32-S3 N16R8 target, with separate
  normal, REVIEW-only, board-test and QEMU profiles.
- Canonical `RESOURCE.MAP` / `RESOURCE.001` validation, bounded raw `jcdata`
  access, LZW/RLE decoding, palettes, SCR, BMP, TTM and ADS loading.
- Native indexed 4-bpp to RGB565 rendering with clipping, transparency, sprite
  flipping, stored-area composition and three Left/Center/Right layouts.
- True RGB-panel front/back synchronization, per-TTM bitmap ownership, retained
  final frames and bounded multi-thread ADS/TTM execution.
- Exact 63-entry story catalog with friendly titles, adjacent navigation,
  Pause/Play, exact Back 10 Frames and persistent OK/REVIEW decisions.
- Runtime loading of all ten canonical SCR resources and authored `F01F` screen
  changes.
- Deterministic island state with ocean/night, island placement, tide, raft,
  waves, clouds, holiday layer, story day/date NVS and non-island teardown.
- Focused repairs for ship/rope persistence, transparent stored-area copies,
  negative child-scene lifetimes, left-island TTM origin, cross-resource bitmap
  dependencies, circle/get-put geometry and fixed-left visitor framing.
- Continuous normal-mode playback through a randomized 63-scene shuffle bag,
  including safe failure skips, no repeats at bag boundaries and one-shot scene
  selection before the existing shuffle resumes.
- A bounded local Wi-Fi setup portal, authenticated JSON API and responsive
  embedded scene controller. Sky and Special Days settings apply immediately
  and persist independently of scene timing.

See `ALL_SCENES_FIDELITY.md` for the numbered findings and their completion
state. A successful build, QEMU run or serial boot does not by itself prove
physical display fidelity.

## Current run

The normal firmware now starts all 63 scenes in an `esp_random()`-seeded
shuffle without consulting REVIEW masks. On a freshly erased board, connect to
the large `Johnny-XXXX` name shown in the sidebar with the displayed numeric
password, open `http://192.168.4.1`, and enter 2.4 GHz Wi-Fi credentials plus a
new administrator password. After joining the LAN the device advertises as
`johnny-xxxx.local`. The administrator password protects the control page and
all `/api/v1/` control/status routes through an opaque session cookie.

The 2026-09-03 physical flash is awaiting Wi-Fi provisioning and direct visual
inspection. QEMU was deliberately skipped for this run. Panel acceptance must
still cover smooth playback, scene switching, Day/Night, every holiday overlay,
`HOLIDAY_NOK` suppression, stale pixels and display drift.

## Prerequisites

- Windows PowerShell and the repo-local ESP-IDF v5.5.5 installation under
  `../.tools/`.
- Canonical `RESOURCE.MAP` and `RESOURCE.001` in the ignored data input path.
- Waveshare ESP32-S3-Touch-LCD-7 connected through its USB data connector for
  physical validation.
- The repository's Raylib 5.5 DLL on `PATH` when running desktop Go tests.

Activate the local toolchain from this directory:

```powershell
$johnnyRepo = (Resolve-Path "..").Path
$johnnyIdf = Join-Path $johnnyRepo ".tools\esp-idf-v5.5.5"
$env:IDF_TOOLS_PATH = Join-Path $johnnyRepo ".tools\espressif"
. (Join-Path $johnnyIdf "export.ps1")
```

## Build and validation

Host and metadata checks:

```powershell
python -m unittest discover -s tools/tests -v
python tools/gen_scene_names.py --check
python tools/inspect_jcdata.py --source data
```

Normal web-control firmware:

```powershell
idf.py -B build-web build
```

REVIEW-only firmware:

```powershell
$johnnyReviewConfig = Join-Path (Get-Location) "build-review-only.sdkconfig"
idf.py -B build-review-only `
  -D "SDKCONFIG=$johnnyReviewConfig" `
  -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.review-only" build
```

QEMU uses isolated build directories and the complete virtual flash image:

```powershell
.\tools\Run-JohnnyQemu.ps1
.\tools\Run-JohnnyQemu.ps1 -Graphics
```

From the repository root, run the desktop comparison tests with:

```powershell
go test -count=1 ./...
```

## Physical board workflow

Never reuse an old COM port without checking. Enumerate ports and identify the
actual ESP32-S3 before flashing:

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
$johnnyPort = "COMx"
python -m esptool --chip esp32s3 --port $johnnyPort flash_id
```

Build, flash and monitor the intended profile. For normal web-control work:

```powershell
python -m esptool --chip esp32s3 --port $johnnyPort erase_flash
idf.py -B build-web -p $johnnyPort flash
idf.py -B build-web -p $johnnyPort monitor
```

After every firmware update, flash, hard-reset and confirm the complete serial
boot. Direct panel observation is required for colour, orientation, flicker,
touch, layer placement, timing, smoothness and scene acceptance. QEMU remains a
supporting regression environment, not a replacement for the physical panel.

## Deferred features and boundaries

Touch-menu parity, weather, layout/clock controls, the optional Soft CRT pass,
OTA, HTTPS/public exposure and sub-TTM browsing remain deferred. Audio, SD-card
resource loading, Bluetooth, screenshots and advanced CRT effects also remain
outside the current ESP32 scope.
