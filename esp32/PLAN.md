# Johnny Castaway ESP32 Port — Plan

## Login acceptance — closed 2026-09-05

Publication follow-up — 2026-09-05: user authorized committing and pushing the
accepted changes to main and refreshing RC4 assets. Package the physically
accepted `eca23cb3...` firmware with the exact committed source revision; verify
public downloads and preserve the existing RC4 tag and Windows artifacts.

The user confirms real browser login has passed, along with physical
Clock/weather presentation, smooth playback, Reviewer/Off switching, controls,
reboot persistence and release-critical fidelity checks. No new automated,
build or flash evidence is claimed. Diagnostic source/tests remain uncommitted.
Weather refresh after reboot and status wording remain open in the release
readiness tracker. The original diagnostic scope follows for context.

Diagnose the RC4 browser login HTTP 500 without erasing NVS or changing session
persistence. Add safe open/write/commit error references and serial NVS counts,
exercise the actual login handler with focused failure injection, build normal
firmware, identify the N16R8 device, app-flash and reboot, then capture the user's
browser retry. Do not close the task until the failing operation/error is known
or a real login succeeds without reproducing the fault. Published RC4 assets,
startup timing and weather retry behavior remain unchanged by this task.

## Goal

### Weather wording follow-up — started 2026-09-05

Replace `STALE DATA` with a last-update label/time when the clock and timestamp
are valid, `SAVED WEATHER` otherwise, and `WAITING FOR WEATHER` without a saved
forecast. Align the controller wording. Build normal firmware, identify and
app-flash/reboot the board, then inspect serial evidence of a fresh forecast.
Keep existing refresh scheduling and the published RC4 package unchanged.
Status: panel/controller wording implemented; five focused icon/login tests,
controller JavaScript syntax and normal build passed. Image `0x127510`, 62%
app free; data image fits its partition. COM4 positively identified as ESP32-S3
rev 0.2, 16 MB flash and 8 MB embedded PSRAM. App-only flash hash verified and
RTS reboot completed with NVS preserved. Serial confirms a fresh saved-location
forecast at 91.683 seconds after reboot (web ready at 86.873 seconds).
Controller timestamp/saved/waiting state execution passed. Refresh gate closed
2026-09-05. User confirmed the new wording "looks good" on 2026-09-05;
physical wording acceptance passed and this follow-up is closed.

Full-story *Johnny Castaway* playback on the Waveshare ESP32-S3-Touch-LCD-7 with a
touch-driven settings and scene-browsing UI. Graphics only — the board has no audio
hardware, so sound is explicitly out of scope. Sierra data is embedded in internal
flash; no SD card is required. Wi-Fi provides first-boot setup, authenticated
local control, NTP time sync and the implemented Open-Meteo weather sidebar.

## Hardware Target (verified)

| Item | Detail |
|---|---|
| Board | Waveshare ESP32-S3-Touch-LCD-7 (800×480 version) |
| SoC | ESP32-S3-WROOM-1-N16R8 — dual LX7 @ 240 MHz, 16 MB flash, 8 MB octal PSRAM |
| Panel | 7″ 800×480 IPS, EK9716 driver, RGB565 parallel interface |
| Touch | GT911 capacitive, 5-point; I2C on GPIO8 (SDA) / GPIO9 (SCL); reset via CH422G EXIO1; IRQ GPIO4 |
| IO expander | CH422G @ I2C 0x24 — LCD reset, backlight, touch reset, USB/CAN select |
| Audio | None on board — sound out of scope |
| Radio | 2.4 GHz Wi-Fi 802.11 b/g/n + BLE 5 onboard; BLE unused |
| RTC battery | None — wall clock resets on power loss; recovered via NTP when Wi-Fi is configured, else NVS/manual fallback |

## Toolchain & Formats

| What | Choice |
|---|---|
| Framework | ESP-IDF v5.5.5 (board wiki requires ≥ v5.3) |
| Language / std | C (GNU17), CMake components + Kconfig |
| Engine baseline | JohnnyCx64 @ `343c7f5` (`v2026.1.0-rc.1-30-g343c7f5`); jc_reborn (C) as semantic reference where the Go code is ambiguous |
| Data files | Original Sierra `RESOURCE.MAP` (MD5 `374e6d05c5e0acd88fb5af748948c899`) + `RESOURCE.001` (MD5 `8bb6c99e9129806b5089a39d24228a36`) |
| Build output | `bootloader.bin`, `partition-table.bin`, `johnny_esp32.bin`, `jcdata.bin`; ELF/map retained for debugging |
| Framebuffer | RGB565 800×480, double-buffered in PSRAM via `esp_lcd` RGB panel DMA |
| Internal graphics | 4-bpp palettized (16 colors), matching the original |

## Data Strategy: Flash Embedding (locked)

`jcdata.bin` is a small headered container flashed to a raw partition:

```
offset  size   content
0       20     header: magic 'JCDT', u8 version=1, u8 reserved[3],
               u32 mapLen (=1461), u32 totalLen, u32 crc32(payload)
20      mapLen RESOURCE.MAP verbatim
20+map  rest   RESOURCE.001 verbatim
```

- Runtime maps the partition with `esp_partition_mmap`; resource lookup =
  `partition_base + 16 + mapLen + offset_from_RESOURCE.MAP`.
- Canonical MD5s are verified twice: by `make_jcdata.py` at build time and at every boot.
- Data updates require reflashing from PC (~15–25 s over UART @921600). Accepted trade-off
  for zero SD dependency.
- SD-card loading deliberately dropped from the critical path; may return as an optional loader.

### Partition layout (16 MB flash)

| Partition | Size | Content |
|---|---|---|
| nvs | 24 KB | Settings, clock offset, story-day |
| factory | ~3 MB | Application |
| jcdata | 1.5 MB | Raw Sierra archives (~1.15 MB used) |

## Memory Budget

| Pool | Usage |
|---|---|
| Flash | Current app ~1.16 MB plus 1.15 MB private data image; the 3 MB app partition remains 62% free |
| PSRAM | ~3.0 MB of 8 MB: 2× framebuffer 1.5 MB + composition 800×480 RGB565 0.75 MB (native scene plus optional side bar) + background layers ~0.3 MB + sprite/decompression buffers <0.5 MB |
| Internal SRAM | Stacks, DMA descriptors, GT911/I2C buffers and bounded Wi-Fi/HTTP/TLS work; framebuffers stay in PSRAM and weather fetches run outside playback |

Rendering always preserves the native 640×480 canvas with no scaling or cropping. Three
layout presets control its horizontal placement on the 800×480 panel:

- **Left bar:** a 160×480 information bar occupies `x=0..159`; Johnny occupies `x=160..799`.
- **Center:** Johnny occupies `x=80..719`, with 80 px black margins on both sides and no
  information bar.
- **Right bar:** Johnny occupies `x=0..639`; a 160×480 information bar occupies `x=640..799`.

The current normal firmware locks Johnny to the native 640x480 left scene
position (`x=0..639`) and persists one mutually exclusive right-sidebar mode:
Off, combined Clock/weather, or Reviewer. Off is the factory default. The
earlier general Left/Center/Right layout proposal remains deferred, as does
optional nearest-neighbor 1.25x stretch mode.

## Project Structure

```
esp32/
├── PLAN.md
├── tools/
│   ├── gen_scene_names.py       # curated CSV → scene_names.c + menu tree data + QA checklist source
│   └── make_jcdata.py           # RESOURCE files → jcdata.bin (+ MD5 gate)
└── johnny-esp32/
    ├── main/                    # boot flow (smart clock prompt), main loop, NVS settings
    └── components/
        ├── jcboard/             # CH422G expander, EK9716 panel timing, backlight, GT911 touch
        ├── jcrez/               # mmap resource access, LZW/RLE decompression, PAL/BMP/SCR/TTM/ADS parsers
        ├── jcgfx/               # palette LUT, indexed blitter, clipping, zones, 8×8 font, menu renderer
        ├── jcengine/            # TTM interpreter, ADS scheduler, story/island/walk-A*, force APIs, scene_names.c
        └── jcnet/                # Wi-Fi setup, SNTP, weather, authenticated local web/API
    └── sdkconfig.defaults       # OPI PSRAM @120 MHz, cache/performance flags per Waveshare notes
```

## Controls and deferred touch UI

The implemented authenticated webpage controls Normal/Review playback, all 63
scenes, Sky, Special Days and the mutually exclusive Off/Clock/Reviewer sidebar.
Reviewer touch hitboxes are enabled only while that sidebar is visible. A full
touch settings overlay, Left/Center/Right layout picker, manual clock picker and
three-level sub-TTM browser remain deferred. Any future overlay must keep
playback running and reuse the hand-rolled renderer rather than adding LVGL.

## Connectivity, Time & Weather

**Wi-Fi and time (implemented):**

- Provisioning: first-boot `Johnny-XXXX` softAP portal at `192.168.4.1`; the
  displayed numeric password protects setup and credentials are stored in NVS.
- The radio remains available for the authenticated LAN control page, SNTP and
  background weather refresh. Network work is bounded and kept outside playback.
- Timezone stored in NVS as a POSIX TZ string; DST handled by the libc time layer.
- SNTP provides wall-clock time after Wi-Fi connection. Unsynchronized states are
  explicit and do not alter the native scene layout.

**Side-bar weather (implemented focused slice — 2026-09-04):**

- Source: Open-Meteo (`api.open-meteo.com`) — free for non-commercial use, no API key.
  Alternative providers kept behind one small client interface so they are swappable.
- Current conditions and today's high/low are fetched about every 45 minutes by
  a background HTTPS task; playback never waits for a weather request.
- Display: the selected 160x480 side bar shows a colour-layered 32x32 pixel
  condition icon at exact 2x scale, followed by current temperature, today's
  high/low, the `DATA FROM METEO` source label, last-successful local update
  time and a subdued stale/unavailable status. The
  embedded masks are adapted from Dhole's CC BY-SA 4.0
  [`weather-pixel-icons`](https://github.com/Dhole/weather-pixel-icons): cloud
  layers use sidebar blue, sun and lightning use yellow, and moon,
  rain/snow and highlights use white. Unknown and fog codes use a neutral cloud
  fallback. No weather artwork is fetched at runtime.
- The embedded control and setup pages use the same Castaway Lookout SVG source
  as the Windows application icon for their browser favicon.
- The side bar and native scene placement do not change when weather becomes stale or
  unavailable; Center continues to hide all side-bar content.
- Location: authenticated three-result city/postal-code search uses Open-Meteo
  geocoding. The chosen display name, coordinates and timezone are stored in
  NVS, along with the last successful forecast for stale-data fallback.

## Scene Catalog Integration (johnny-castaway.com)

No web pages are archived. The catalog was consumed once at implementation time:

1. Audit `list.html` plus 13 reference/detail pages into `scene_catalog.csv`.
   The inventory records each URL, classification and site revision. Special
   Days from `annivers.html` are stored separately in `special_days.csv` as date
   ranges because they are calendar overlays, not playable scenes.
   Ambiguities are resolved against JohnnyCx64 `TTM_SCENES.md`
   (41 collections / 616 subscenes).
2. `gen_scene_names.py` validates 63/63 ordered ADS identities and generates:
   - `jcengine/scene_names.c` — TTM/ADS → friendly-title lookup (<10 KB flash), enabled via
     `CONFIG_JC_SCENE_NAMES`, logged at each scene start (`SCENE: Pirates - walk the plank`);
   - menu tree data for the 3-level browser;
   - the QA checklist source.
3. Events that cannot be mapped unambiguously appear in the checklist only, never in
   firmware name tables.

The generated title table is now the single friendly-name source for the guided
63-scene sidebar and serial exports. Special Day rules cover St. Patrick,
Halloween, Christmas and New Year; Easter remains checklist-only because the
live site supplies no usable rule and the original bitmap has four overlays.

**QA gate:** Phase 5 sign-off requires every documented catalog event observed playing
correctly on device. The menu browser doubles as test navigation.

## Soft CRT Filter (future — Phase 8 backlog)

- Provide one deliberately small display setting: **CRT Filter: Off / Soft**, persisted in
  NVS. Soft is the factory default; Off is the pixel-perfect reference mode.
- Apply the filter only to Johnny's native 640×480 canvas. The clock/weather side bar and
  touch UI always remain sharp and unfiltered.
- Soft uses a low-cost three-pixel horizontal blend, subtly dims alternate rows by about
  10%, and compensates slightly for the lost brightness. It adds no curvature, bloom, RGB
  shadow mask, or multi-pass shader effects.
- Implement it as a bounded RGB565 composition pass with no additional full-screen buffer.
  Retain Off automatically if the measured render cost prevents a stable 30 fps output.

## Local Web Interface (implemented control slice — 2026-09-03)

- Host a small self-contained webpage and JSON API on the ESP32 for use from the local
  network; embed all HTML, CSS, and JavaScript in firmware with no external web assets.
- Show device, network/time, shuffle and current-scene status and all 63 stable
  catalog identities. Provide Random, one-shot scene, Sky and Holiday controls.
- On an erased device, expose WPA2 setup as `Johnny-XXXX` at `192.168.4.1`,
  then join 2.4 GHz Wi-Fi and advertise `johnny-xxxx.local`. A failed station
  connection returns to setup mode.
- Store a salted PBKDF2 administrator verifier and only a hashed opaque session.
  Keep HTTP-to-display mutations behind a bounded acknowledged FreeRTOS queue.
- Use Stockholm CET/CEST for SNTP-backed Automatic Sky and Special Days. Invalid
  time falls back to day with no automatic holiday.
- The Clock/weather/Reviewer sidebar and location/timezone selection are now
  implemented. General layout, CRT and touch-menu parity remain later slices.

## Runtime review, bug logging and story blocks — implemented 2026-09-04

The normal web firmware has a persisted Normal/Review playback choice.
Review mode walks the complete 63-scene catalog in order and repeats each event
until Looks OK, Bug, Previous or Next is selected. Bug captures update a
separate bounded `jc_bug` NVS record for that scene and never modify the closed
`jc_review` ledger. The authenticated webpage exposes the bug log with
copy-one, Copy All, resolve and confirmed Clear All actions. Each sanitized
record includes the complete scene identity, frame, playback and sky/holiday
settings, effective island state, firmware identity, catalog fingerprint and a
valid local timestamp or uptime.

Sky gains a persisted Cycle choice without changing the existing Automatic,
Day or Night meanings. Cycle starts with ten completed scene transitions in
daylight, then ten at night, and repeats; replay and rewind do not advance it and
a reboot begins a new daytime block. The same block owns the variable island
anchor. A new legal anchor is chosen only at a block boundary and is limited to
64 pixels horizontally and 32 vertically from the previous anchor. Authored
fixed-left placement remains authoritative.

The physical 160-pixel reviewer sidebar has its own persisted authenticated web
switch, independent of Normal/Review playback. Normal firmware defaults it Off,
clears the reserved area to black and disables reviewer touch hitboxes. The
unprovisioned Wi-Fi setup sidebar and compile-time REVIEW-only diagnostics ignore
that switch and remain visible.

Implementation and closure evidence is tracked in numbered `../TODO.md` entries.
Each records Added, Status, Changes, Validation, Remaining gate and Closed using
Stockholm `YYYY-MM-DD` dates. Automated, flash/serial and direct-panel gates stay
distinct, and the overall leg closes only after physical acceptance.

## Phases

### Current ordered execution plan — 2026-09-04

The detailed defect and repair record for complete catalog playback is
[`ALL_SCENES_FIDELITY.md`](ALL_SCENES_FIDELITY.md). Keep
`SCENE_REVIEW.md` as the physical evidence ledger and use this plan for execution
order. Follow [`RUNBOOK.md`](RUNBOOK.md) for exact session commands, validation
profiles and handoff evidence. Do not reopen accepted scenes unless a later
engine-wide change affects their rendering, timing, ownership or completion
semantics.

1. **Completed — close the 63-scene review state.** The current REVIEW-only image
   was hard-reset on the re-identified ESP32-S3. Serial restored
   `63 OK / 0 REVIEW`, matched catalog fingerprint `eb04eb66746ae074`, reported
   `ALL RESOLVED` and emitted the empty final shortlist export. The physical and
   persisted 63-scene review is closed.
2. **Completed — deterministic island/SCR validation slice.** Exact rewind is
   compared with fresh deterministic replay for MARY tag 2 and VISITOR tag 3;
   the rendered fixture now advances and batches island waves/clouds before
   locking checkpoint/final RGB565 hashes. The ten SCR host fixtures, 23 Python
   tests, uncached Go tests, normal and REVIEW-only builds, and strict headless
   QEMU pass. Graphical capture remains in the complete release gate rather than
   being treated as physical proof.
3. **Implemented — randomized playback, settings and local web control.** Normal
   firmware shuffles all 63 scenes, applies persisted Sky/Special Days/sidebar
   settings, serves the authenticated UI/API, stores review/bug state and keeps
   a ten-Day/ten-Night block sequence with stable island anchors.
4. **Implemented — Clock/weather sidebar and release flasher.** The 160-pixel
   sidebar provides SNTP time/date, city weather, colour pixel icons, stale state
   and local update time. RC4 includes a installation guide and double-click Windows
   flasher that generates private `jcdata.bin` locally and positively identifies
   the supported N16R8 board before writing.
5. **Close the current physical acceptance gate.** Directly confirm sidebar
   layout/colours, smooth playback, authenticated switching, persistence,
   weather staleness, review actions, Day/Night blocks and holidays. Serial,
   HTTP, framebuffer and flash evidence do not close this gate.
6. **Complete remaining Phase 3 interpreter parity.** Implement random-range
   `TIMER`, root/current tag separation, clip and missing draw primitives,
   scoped saved zones, local/global `IF_LASTPLAYED`, stable semantic z-order and
   the remaining story/walk/pathfinding lifecycle. Add focused regressions before
   another full physical pass.
7. **Run an engine-wide regression pass.** Exercise all 63 catalog identities,
   every SCR class and representative island variants in fixtures and QEMU, then
   repeat direct panel checkpoints for every behavior touched by Phase 3 parity.
   Require bounded heap/draw usage and no watchdog, panic or restart loop.
8. **Implement only the remaining deferred UI slices after the engine gate.**
   Add the touch settings overlay, general layout picker, manual clock flow,
   sub-TTM browser and optional Soft CRT only with focused physical performance
   gates.

#### Phase 3 completion criteria

- Every catalog identity starts, renders, completes, cleans up and can restart
  without stale layers, missing resources or changed authored timing.
- Restart and Back 10 Frames reproduce the same deterministic event state;
  normal live playback retains non-constant authored random behavior.
- NVS review/story namespaces survive app-only flashes and reject incompatible
  schema/catalog data without damaging the other namespace.
- Python, uncached Go, normal and REVIEW-only ESP-IDF, strict QEMU and physical
  serial gates pass with canonical data and bounded memory.
- The physical panel sustains smooth presentation and shows correct placement,
  clipping, z-order, backgrounds and transitions. Only this observation closes
  visible fidelity and performance findings.

### Approved SCR and island-lifecycle architecture — 2026-09-01

The ESP32 must consume the same ten SCR resources as the Windows engine from the
unchanged canonical `RESOURCE.MAP` and `RESOURCE.001`. `INTRO.SCR` remains the
boot screen. TTM `LOAD_SCREEN 0xF01F` selects `ISLETEMP.SCR`, `ISLAND2.SCR`,
`SUZBEACH.SCR`, `JOFFICE.SCR`, or `THEEND.SCR`; no parallel lookup table may
override the authored bytecode. Full island sequences use `OCEAN00.SCR`,
`OCEAN01.SCR`, or `OCEAN02.SCR` by day and `NIGHT.SCR` at night.

One runtime loader owns SCR replacement. It finds and decompresses the resource
through `jcrez`, renders it into the RGB565 background through `jcgfx`, records
the active name and hash, releases the temporary decoded allocation, and clears
the saved-area overlay even when the same screen is loaded again. `F01F`
executes at its original position in each TTM stream.

Island state is initialized before an island catalog event and completely torn
down before a non-island event. It contains sky mode, story day, tide, raft,
island offset, holiday, cloud state, wave phase and a deterministic event seed.
Automatic sky follows the Windows 06:00/18:00 boundaries when local time is
valid and falls back to day when it is not. The seed derives from ADS name/tag
and story day; restart and Back 10 Frames restore the same event snapshot rather
than rerandomizing it.

The background implementation includes the Windows stack, not bare ocean SCRs:
raft stages from `MRAFT.BMP`; island, tree, shadow, shore, rock, waves and clouds
from `BACKGRND.BMP`; and calendar overlays from `HOLIDAY.BMP`. Scene flags govern
low tide, variable/left placement, raft suppression and holiday suppression.
Composition order is fixed as SCR/island -> clouds -> stored area -> ordered TTM
layers -> holiday -> sidebar. Island offsets apply to both the background assets
and event TTM drawing.

Story progression uses a separate versioned NVS namespace with day 1-11 and the
last valid local calendar date. The first valid date initializes tracking; each
later date transition advances exactly once and wraps 11 to 1. Invalid time
does not change NVS. The existing review namespace and catalog fingerprint are
unchanged. Automatic/Day/Night engine states are provided for tests and future
settings, but this phase adds no touch or web control.

Validation must cover all ten decoded and rendered SCR hashes, short-screen
padding, representative `F01F` commands, time/date boundaries, deterministic
state and replay, every island layer and z-order. QEMU evidence is followed by a
freshly identified physical flash, hard reset, serial boot gate and direct panel
review; only physical observations may update `SCENE_REVIEW.md`.

#### Implementation checkpoint — session closed 2026-09-01

The canonical loader, authored `F01F`, island lifecycle and compositor, separate
`jc_story` NVS schema, injected island seed, Automatic/Day/Night policy, full
raft/island/tide/wave/cloud/holiday assets and TTM island offsets are now in the
working tree. ADS scheduling deliberately retains its existing seed; the
event/day seed belongs only to island state so background replay cannot alter
authored ADS choices. Wave state retains a phase for every three/four shoreline
zone and advances one zone per desktop eight-centisecond tick.

Host validation currently passes all 20 Python tests, including ten canonical
SCR fixtures, and uncached Go tests. The normal ESP-IDF build passed before the
final island-seed/ADS-seed separation. Headless QEMU passed the new lifecycle
and five `LOAD_SCREEN` boot gates, then exposed the expected obsolete validated-
event framebuffer hash. The first replacement hash was captured, but the source
gate was restored rather than weakened. The next task is to rebuild the final
seed separation, run headless QEMU, capture and lock all five per-event island
hashes while preserving completion counts/order, then finish graphical QEMU,
flash/serial and direct panel gates. No NVS was written on hardware and
`SCENE_REVIEW.md` remains untouched.

Follow-up on 2026-09-01 locked the five new island-aware framebuffer hashes as
per-event fixtures. Strict headless QEMU and the normal ESP-IDF build pass. The
normal firmware uses review schema 2 to start a fresh 63-scene review without
erasing the separate story namespace. The identified COM4 ESP32-S3 was flashed
with verified firmware and unchanged `jcdata`, hard-reset, and reached Scene
1/63. Physical serial observed the authored event complete and restart from
frame 1 twice with stable island state; direct panel acceptance is still open.

### Implementation history through 2026-09-03

This section preserves the review/runtime history. The current RC4 state and
remaining gates are the ordered plan above.

Phases 0-2 and the first bounded part of Phase 3 are operational. The physical
firmware repeatedly plays one complete `ACTIVITY.ADS` tag-1 event at native
timing through the multi-thread TTM runtime and true panel front/back buffers.
The user has directly accepted smooth presentation and the corrected repeated
tree-climb composition.

Phase 3 now also contains an exact C copy of the desktop engine's 63 story-scene
records plus bounded eligibility and in-order selection. The live loop presents
all 63 records as a guided review. OK and REVIEW decisions are committed before
advancing and persist across resets as mutually exclusive NVS masks guarded by
a schema and ordered-catalog fingerprint. Per-scene failure codes preserve
recoverable start, runtime, composition and rewind failures as REVIEW results.
After all 63 are classified, playback pauses on a persistent summary and emits
a Markdown-ready identity table over serial; `SCENE_REVIEW.md` is the
authoritative human ledger. Adjacent Scene -1 / Scene +1 navigation can revisit
any event and replace its result. The sidebar retains Pause/Play, exact Back 10
Frames, the 1-based displayed frame, friendly title and ADS filename/tag.
Deterministic firmware fixtures cover review masks, resume selection, wrap,
replacement, completion, catalog fingerprint and all six touch targets while
preserving every existing completion-order/hash fixture. Earlier physical
inspection accepted five bounded events, including event 4's
ship and tied-down rope, and event 5's continuously visible boat without
flashing. The rope fix retains and composites palette-colored TTM `DRAW_LINE`
segments, and its deterministic gate executes 700 line commands with the
completion order unchanged. Event 5's approximately 23.8-second run is accepted
with a note that it feels very fast; it matches the desktop engine's authored
centisecond scheduling, so no artificial slowdown was introduced. The Scene
4/5 visual gate is closed. Fourteen Python tests, uncached Go tests, normal and
headless/graphical QEMU builds, graphical sidebar/summary inspection and a full
verified physical flash pass for that bounded review. The original complete
63-scene classification and serial export were confirmed at 29 OK, 34 REVIEW
and zero unreviewed. The separate REVIEW-only build uses
`sdkconfig.review-only`, keeps
the catalog identity behind a filtered `1/M` list, removes OK results, retains
REVIEW results and exports the shortlist after each pass. Its first pass starts
at STAND EVENT 1 to keep the STAND sequence together. Serial evidence remains
separate from visual acceptance. Focused repairs and direct panel re-review have
since resolved all 34 REVIEW decisions. The final candidate was physically
accepted at 63 OK on 2026-09-03. The final hard reset restored
`ok=63 review=0 complete=1`, matched catalog fingerprint `eb04eb66746ae074`,
emitted an empty final shortlist and reported `REVIEW-ONLY: ALL RESOLVED`.
The review ledger is reconciled and the immediate pickup advances to the final
smoothness and deterministic rewind/z-order gates.

Since this checkpoint, randomized Normal/Review playback, persistent web
controls and bug log, Sky/Special Days/Cycle settings, Wi-Fi/SNTP, weather and
Off/Clock/Reviewer sidebars have been implemented. Full interpreter parity,
touch-menu/layout parity, optional Soft CRT and the current physical acceptance
gate remain open; `ALL_SCENES_FIDELITY.md` carries the exact items.

### Windows screensaver reuse by phase

The Windows Go/Raylib implementation is the behavioral reference for game semantics,
but it is not directly compilable as ESP-IDF C code. Port the platform-neutral behavior
and static data into the existing ESP32 component boundaries, then keep validating the
result against the shared desktop fixture hashes.

- **Phase 1:** Use `resource.go` and `uncompress.go` as the reference for resource parsing
  and LZW/RLE semantics. The existing `jcrez` implementation is the ESP32-native port.
- **Phase 2:** Use `graphics.go` as the reference for palette handling, clipping,
  transparency, sprite flipping, zones, and composition behavior. Implement those
  semantics with the ESP32 RGB565 renderer rather than Raylib calls.
- **Phase 3:** Use `ttm.go`, `ads.go`, `story.go`, `island.go`, `walk.go`, `calcpath.go`,
  and their static data tables as the engine reference. Complete the port in this order:
  TTM behavior, ADS scheduling, story/island state, then walking/pathfinding.
- **Phase 4:** Reuse the metadata in `ttm_catalog.go` by generating compact read-only C
  tables for the touch scene browser.

Do not port the Win32 screensaver shell, Raylib window/input handling, audio, installer
code, or desktop GLSL shaders. These are platform-specific or outside the ESP32 scope.

| Phase | Work | Verification gate |
|---|---|---|
| 0 | Board bring-up: IDF project, PSRAM config, panel + CH422G + backlight, GT911 touch | Test pattern on panel; touch draws cursor |
| 1 | `jcdata` partition, `make_jcdata.py`, mmap access, LZW/RLE port, parsers | **MD5 gate:** all decompressed resources hash-match the desktop reference before any further work |
| 2 | Palette LUT, blitter, clipping, zones, three-position native composite, 160×480 side-bar surface, 8×8 font | Johnny's 640×480 region is pixel-identical to desktop dumps at `x=160` (Left bar), `x=80` (Center), and `x=0` (Right bar); no scaling, cropping, or side-bar overlap |
| 3 | TTM/ADS/story/island/walk ports, NVS persistence, force APIs, scene-name logging | Serial log shows correct names during full-story run |
| 4 | Persisted web Sky/Special Days/sidebar controls implemented; touch overlay, layout picker, clock picker and 3-level browser deferred | Implemented controls survive reboot; future touch menus remain fully navigable while playback continues |
| 5 | Integration: 10 Hz logic / 30 fps render loop, watchdog, heap monitor, INTRO.SCR boot flow | **QA gate:** all catalog events verified |
| 6 | Wi-Fi softAP provisioning, SNTP, NVS credentials and authenticated LAN control implemented | Clock self-sets after reboot; setup and authenticated API behavior remain bounded and local |
| 7 | Open-Meteo weather, colour icons, temperature/high-low/update rendering and staleness handling implemented in combined Clock mode | Stale/unavailable data keeps layout stable; fetches cause no visible playback impact |
| 8 | *(future)* Optional Soft CRT composition pass and persisted Off/Soft setting | Soft reduces harsh pixel edges without filtering the side bar or touch UI; Off remains pixel-identical; both modes sustain 30 fps on device and survive reboot |
| 9 | Authenticated local webpage/API, password claiming, mDNS, scene control, settings and bug log implemented; web-disable/touch mirroring deferred | Unauthenticated or malformed changes are rejected; current physical switching/persistence gate remains open |

## Risks & Mitigations

| Risk | Mitigation |
|---|---|
| Panel timing varies per unit | Start from Waveshare demo timings; adjust PCLK/VCOM if drift appears |
| LZW port correctness | Phase 1 MD5 gate blocks everything until clean |
| Flash/PSRAM share SPI1 bandwidth | Read-only mmap after boot; no flash writes during playback (NVS writes only at scene/day changes) |
| Wi-Fi/web traffic competes with RGB panel for memory and bandwidth | Use bounded HTTP connections, small compressed assets, rate-limited status polling, and lower-priority network work; retain the short duty cycle when web access is disabled and verify stable 30 fps on device |
| Weather provider unavailable/changed | Single small client interface isolates the API; the side bar retains its layout and reports stale/unavailable data without disturbing playback |
| Soft CRT pass reduces render throughput (future) | Keep the filter single-pass and RGB565-native, measure it on device, and retain Off as the guaranteed pixel-perfect fallback |
| First-visitor web claiming is exposed on an untrusted LAN | Document trusted-LAN-only use; keep stored credentials hidden; physical disable/reset controls remain deferred |
| Clock lost on power cycle | SNTP recovery when Wi-Fi is configured; else NVS-persisted story-day + smart clock prompt |
| Catalog→TTM mapping gaps | Unmappable events stay checklist-only; desktop cross-reference resolves disputes |

## Explicitly Out of Scope

Audio output (no board hardware) · SD-card data path · advanced CRT effects (curvature,
bloom, RGB shadow masks, or multi-pass shaders) · screenshots · Windows screensaver
integration · Bluetooth/BLE features · OTA updates. Wi-Fi, NTP, the weather
sidebar and authenticated local web interface are implemented; public-internet
exposure is explicitly unsupported.
