# ESP32 Development Runbook

This is the operational entry point for an ESP32 work session. It records the
read order, current pickup, validation profiles and evidence required before a
task can be closed. Keep long-term design in `PLAN.md`, ordered fidelity work in
`ALL_SCENES_FIDELITY.md`, and physical results in `SCENE_REVIEW.md`.

## Start every run

1. Open `E:\ai\Johnny\JohnnyCx64\esp32` and read, in order:
   `../AGENTS.md`, `AGENTS.md`, the active ESP32 section of `../TODO.md`,
   `PLAN.md`, this runbook and the relevant component sources.
2. Run `git status --short --branch`. The checkout is intentionally dirty;
   preserve unrelated modified and untracked files.
3. Use the repo-local ESP-IDF v5.5.5 installation. Do not rely on a global IDF
   environment or assume a previously observed COM port is still correct.
4. Keep canonical `RESOURCE.MAP` and `RESOURCE.001` ignored and uncommitted.
5. Before editing code, select the first unchecked item in
   `ALL_SCENES_FIDELITY.md` whose prerequisites are complete and record any new
   scene finding in `SCENE_REVIEW.md` with identity, frame and visible symptom.

## Current pickup — provision and visually accept web-control firmware

The schema-2 review closure passed on 2026-09-03. COM4 was re-identified as an
ESP32-S3 rev 0.2 with 16 MB flash and 8 MB PSRAM. The REVIEW-only image is
`johnny_esp32.bin` size `0x5ee20`, SHA-256
`55f791db3a3a4af7962e9e4a9eb89202d87b09375eb02f36c3a63bdd6ede6f2b`.
After a hard reset, serial restored `ok=63 review=0 complete=1`, matched catalog
fingerprint `eb04eb66746ae074`, emitted the empty final shortlist and reported
`REVIEW-ONLY: ALL RESOLVED` without fatal runtime errors.

The 2026-09-03 normal image was flashed after a full erase on the identified
COM4 ESP32-S3 N16R8. It boots randomized 63-scene playback and setup AP
`Johnny-59D8`. Read the current numeric setup password from the panel or serial;
it is regenerated while the device remains unprovisioned.

1. Connect to the displayed AP, open `http://192.168.4.1`, and submit 2.4 GHz
   Wi-Fi credentials plus a new administrator password.
2. Confirm the station address/SNTP sync and `johnny-59d8.local` in serial.
3. Exercise login/logout, status, scenes, settings, selected-scene and Random
   routes with a cookie jar, then reboot and confirm settings/session behavior.
4. Ask for direct panel confirmation of smoothness, switching, Day/Night, each
   holiday, suppression, stale pixels and drift. Do not infer visual acceptance
   from serial or API responses.

## Activate the repo-local toolchain

Run from the ESP32 directory in PowerShell:

```powershell
$johnnyRepo = (Resolve-Path "..").Path
$johnnyIdf = Join-Path $johnnyRepo ".tools\esp-idf-v5.5.5"
$env:IDF_TOOLS_PATH = Join-Path $johnnyRepo ".tools\espressif"
. (Join-Path $johnnyIdf "export.ps1")
```

Enumerate candidate ports, choose the connected board's actual port, then
identify it. Replace `COMx` only after inspecting the current list:

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
$johnnyPort = "COMx"
python -m esptool --chip esp32s3 --port $johnnyPort flash_id
idf.py -B build-review-only -p $johnnyPort monitor
```

Use `Ctrl+]` to exit the ESP-IDF monitor. A successful `flash_id` proves device
identity and flash capacity; PSRAM capacity must also be confirmed in the
firmware boot log.

## Validation profiles

### Host and metadata

```powershell
python -m unittest discover -s tools/tests -v
python tools/gen_scene_names.py --check
python tools/inspect_jcdata.py --source data
```

From the repository root, run uncached Go tests with the repo-local Raylib DLL
available on `PATH`:

```powershell
go test -count=1 ./...
```

### Normal web-control firmware

```powershell
idf.py -B build-web build
```

### REVIEW-only firmware

```powershell
$johnnyReviewConfig = Join-Path (Get-Location) "build-review-only.sdkconfig"
idf.py -B build-review-only `
  -D "SDKCONFIG=$johnnyReviewConfig" `
  -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.review-only" build
```

### QEMU

```powershell
.\tools\Run-JohnnyQemu.ps1
.\tools\Run-JohnnyQemu.ps1 -Graphics
```

The runner creates separate build/configuration paths and a complete 16 MB
virtual flash image containing `jcdata`. `-Graphics` opens the 800x480 virtual
framebuffer. QEMU never proves physical panel timing, colour, flicker, touch,
PSRAM performance or cold-boot stability.

## Flash and physical acceptance

Before any flash, repeat board/port identification and verify that generated
`jcdata.bin` fits its partition. Use the build directory for the intended
profile; do not call a normal build the latest REVIEW-only candidate.

```powershell
python -m esptool --chip esp32s3 --port $johnnyPort erase_flash
idf.py -B build-web -p $johnnyPort flash
idf.py -B build-web -p $johnnyPort monitor
```

After an update, flash, hard-reset and capture serial boot. For visible or
control changes, inspect the physical panel directly. The final all-scenes gate
must cover the affected regression frames plus Pause/Play, exact Back 10 Frames,
touch, scene completion/restart, stable 30 fps presentation and absence of stale
layers or pixels.

## End every run

1. Update checked state and exact pickup in `ALL_SCENES_FIDELITY.md` and the root
   ESP32 TODO section.
2. Put visual decisions and serial totals in `SCENE_REVIEW.md`; never infer them
   from QEMU or hashes.
3. Update `PLAN.md` only when sequencing, scope or an accepted design changes.
4. Record the branch, dirty state, builds/tests run, flashed profile and hash,
   actual board/port, serial result, physical result, blocker and exact next
   command.
5. Run whitespace/link checks and `git diff --check` without cleaning or staging
   unrelated work.
