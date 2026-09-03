# Johnny Castaway ESP32 Port — Plan

## Goal

Full-story *Johnny Castaway* playback on the Waveshare ESP32-S3-Touch-LCD-7 with a
touch-driven settings and scene-browsing UI. Graphics only — the board has no audio
hardware, so sound is explicitly out of scope. Sierra data is embedded in internal
flash; no SD card is required. The SoC's Wi-Fi is used opportunistically for NTP time
sync, with a weather overlay (Open-Meteo) planned as a future phase.

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
| Build output | `bootloader.bin`, `partition-table.bin`, `johnny-esp32.bin`, `jcdata.bin`; ELF/map retained for debugging |
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
| Flash | ~14 % of 16 MB (app ~0.8 MB + data 1.15 MB + fonts/menus ~0.05 MB, with a future allowance of <0.1 MB for compressed web assets, API routes, and authentication) |
| PSRAM | ~3.0 MB of 8 MB: 2× framebuffer 1.5 MB + composition 800×480 RGB565 0.75 MB (native scene plus optional side bar) + background layers ~0.3 MB + sprite/decompression buffers <0.5 MB |
| Internal SRAM | Stacks, DMA descriptors, GT911/I2C buffers; framebuffers stay in PSRAM. Wi-Fi activity adds ~30–50 KB transient (plus ~40–50 KB TLS heap during weather fetches in the future phase). The future web server must use bounded connections and remain within an additional 40 KB active-heap budget. |

Rendering always preserves the native 640×480 canvas with no scaling or cropping. Three
layout presets control its horizontal placement on the 800×480 panel:

- **Left bar:** a 160×480 information bar occupies `x=0..159`; Johnny occupies `x=160..799`.
- **Center:** Johnny occupies `x=80..719`, with 80 px black margins on both sides and no
  information bar.
- **Right bar:** Johnny occupies `x=0..639`; a 160×480 information bar occupies `x=640..799`.

Clock and weather are independent side-bar options. Center hides both without changing their
saved enabled states, so returning to Left bar or Right bar restores the selected content. The
factory default is Right bar with both clock and weather enabled. Layout, clock visibility, and
weather visibility are persisted in NVS. A selected side bar remains present even when both
content options are disabled. Optional nearest-neighbor 1.25× stretch mode is deferred.

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
        └── jcnet/                # (Phase 6+) Wi-Fi, SNTP; future weather and local web management
    └── sdkconfig.defaults       # OPI PSRAM @120 MHz, cache/performance flags per Waveshare notes
```

## Touch UI (overlay — playback continues behind it)

- **Tap anywhere** → control dock/settings panel slides up; animation keeps running
  underneath (mirrors the desktop screensaver behavior).
- **Sky:** Automatic (clock) / Day / Night.
- **Holiday:** Off / Automatic (calendar) / Halloween / St. Patrick / Christmas / New Year.
- **Layout:** Left bar / Center / Right bar. **Clock** and **Weather** toggles independently
  control the selected side bar. Center always hides both while retaining their saved states;
  the default is Right bar with both enabled. All three choices persist in NVS.
- **Clock:** HH:MM picker; smart boot prompt only when no trusted time source exists
  (no Wi-Fi configured and NVS time invalid), otherwise instant start; manual re-set via
  menu button. With Wi-Fi configured, SNTP sets the clock automatically after sync.
- **Scene browser, 3 levels:** Category → Event → Subscene/TTM, generated from the scene
  catalog (below); selecting plays immediately. Level 3 lists subscenes dynamically from
  parsed TTM chunk metadata.
- All menus operable by touch alone; hand-rolled renderer on top of the indexed blitter.
  No LVGL — saves ~300 KB flash and avoids a second render pipeline.

## Connectivity, Time & Weather

**Wi-Fi (Phase 6):**

- Provisioning: first-boot softAP portal (connect from phone, enter SSID/password) with a
  serial-console fallback; credentials stored in NVS.
- Duty cycle: radio powers on briefly for an SNTP sync at boot and then every 12 h;
  disabled between syncs to keep PSRAM bandwidth clear for the RGB panel. The future web
  interface overrides this policy while enabled and keeps Wi-Fi active for local access.
- Timezone stored in NVS as a POSIX TZ string; DST handled by the libc time layer.
- Fallback chain: SNTP → NVS last-known time + offset estimate → smart boot clock prompt
  → manual set via touch UI.

**Side-bar weather (future — Phase 7 backlog):**

- Source: Open-Meteo (`api.open-meteo.com`) — free for non-commercial use, no API key.
  Alternative providers kept behind one small client interface so they are swappable.
- Current conditions (+ optionally today's forecast) fetched every 30–60 min during brief
  Wi-Fi windows; HTTPS via mbedtls (~50 KB transient heap).
- Display: the selected 160×480 side bar shows a condition icon, current temperature, today's
  high/low, last-update time, and a subdued stale/unavailable status when current data cannot
  be fetched. The Weather toggle controls this content independently of the Clock toggle.
- The side bar and native scene placement do not change when weather becomes stale or
  unavailable; Center continues to hide all side-bar content.
- Location: latitude/longitude entered during provisioning, editable in the menu, stored
  in NVS.

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
- Layout, clock/weather, timezone selection, CRT and touch-menu parity remain
  later slices rather than being coupled to this controller.

## Phases

### Current ordered execution plan — 2026-09-03

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
3. **Implemented — randomized playback and local web control.** Normal firmware
   now shuffles every one of the 63 scenes once per cycle, applies persisted
   Sky/Special Days settings immediately and serves the authenticated local UI
   and JSON API. The ESP32 was fully erased and the normal image flashed on
   2026-09-03; serial proved clean boot, setup AP startup and multiple shuffled
   scene transitions. Wi-Fi provisioning, direct API/reboot persistence checks
   and all visual gates remain pending. QEMU was excluded from this run.
4. **Close the physical performance gate.** Flash and hard-reset the rebuilt
   image, verify serial boot, and directly confirm smooth wave/cloud batching,
   transitions, Pause/Play and exact Back 10 Frames. Serial timing or framebuffer
   hashes alone do not close this gate.
5. **Complete remaining Phase 3 interpreter parity.** Implement random-range
   `TIMER`, root/current tag separation, clip and missing draw primitives,
   scoped saved zones, local/global `IF_LASTPLAYED`, stable semantic z-order and
   the remaining story/walk/pathfinding lifecycle. Add focused regressions before
   another full physical pass.
6. **Run an engine-wide regression pass.** Exercise all 63 catalog identities,
   every SCR class and representative island variants in fixtures and QEMU, then
   repeat direct panel checkpoints for every behavior touched by Phase 3 parity.
   Require bounded heap/draw usage and no watchdog, panic or restart loop.
7. **Implement remaining Phase 4 touch controls and catalog browsing.** Add the persisted
   Special Days selector first, followed by the touch overlay, Sky controls,
   layout and independent Clock/Weather toggles, manual clock flow and the
   generated three-level scene browser. Playback must continue behind the menu.
8. **Proceed with remaining Phase 5-9 slices after the engine gate is closed.** Complete
   long-run integration/monitoring, then Wi-Fi/SNTP, weather, Soft CRT and the
   local authenticated web interface in that order, with a physical 30 fps gate
   after each feature that affects rendering, memory or radio activity.

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

### Current implementation status — 2026-09-03

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

Full interpreter parity, rendered rewind/z-order coverage, the final smoothness
gate, complete story/walk/pathfinding behavior and the Phase 4-9 user features
remain open. Their execution order is defined above and their individual
findings are tracked in `ALL_SCENES_FIDELITY.md`.

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
| 4 | Menu overlay, Sky/Holiday pickers, 3-level browser, clock picker, layout preset and independent Clock/Weather toggles | Every menu is fully navigable by touch; playback continues behind open menus; layout and both toggles survive reboot; Center hides side-bar content without clearing its saved states |
| 5 | Integration: 10 Hz logic / 30 fps render loop, watchdog, heap monitor, INTRO.SCR boot flow | **QA gate:** all catalog events verified |
| 6 | Wi-Fi provisioning (softAP + serial fallback), SNTP sync, timezone, NVS credential storage, side-bar clock/date | When enabled on either side, the clock self-sets after reboot with configured Wi-Fi; Center hides it while retaining the toggle; panel shows no drift or layout movement during sync bursts |
| 7 | *(future)* Side-bar weather via Open-Meteo: client interface, condition/temperature/high-low/update rendering, independent toggle, staleness handling | Weather works alone and with the clock on either side; stale/unavailable data shows status without changing layout; Center hides it while retaining the toggle; fetches cause no visible playback impact |
| 8 | *(future)* Optional Soft CRT composition pass and persisted Off/Soft setting | Soft reduces harsh pixel edges without filtering the side bar or touch UI; Off remains pixel-identical; both modes sustain 30 fps on device and survive reboot |
| 9 | *(future)* Local webpage and JSON API, common timezone selector, mirrored settings, scene browser/control, password claiming, persistent login, mDNS, and web enable/disable control | Device information is accurate; touch and web changes remain synchronized and survive reboot; scene selection works; unauthenticated or malformed changes are rejected; login/logout/reset behave correctly; web activity stays within flash/heap budgets and sustains 30 fps without panel drift |

## Risks & Mitigations

| Risk | Mitigation |
|---|---|
| Panel timing varies per unit | Start from Waveshare demo timings; adjust PCLK/VCOM if drift appears |
| LZW port correctness | Phase 1 MD5 gate blocks everything until clean |
| Flash/PSRAM share SPI1 bandwidth | Read-only mmap after boot; no flash writes during playback (NVS writes only at scene/day changes) |
| Wi-Fi/web traffic competes with RGB panel for memory and bandwidth | Use bounded HTTP connections, small compressed assets, rate-limited status polling, and lower-priority network work; retain the short duty cycle when web access is disabled and verify stable 30 fps on device |
| Weather provider unavailable/changed (future) | Single small client interface isolates the API; the side bar retains its layout and reports stale/unavailable data without disturbing playback |
| Soft CRT pass reduces render throughput (future) | Keep the filter single-pass and RGB565-native, measure it on device, and retain Off as the guaranteed pixel-perfect fallback |
| First-visitor web claiming is exposed on an untrusted LAN (future) | Document trusted-LAN-only use, allow web access to be disabled physically, and provide a touch-menu password reset without exposing stored credentials |
| Clock lost on power cycle | SNTP recovery when Wi-Fi is configured; else NVS-persisted story-day + smart clock prompt |
| Catalog→TTM mapping gaps | Unmappable events stay checklist-only; desktop cross-reference resolves disputes |

## Explicitly Out of Scope

Audio output (no board hardware) · SD-card data path · advanced CRT effects (curvature,
bloom, RGB shadow masks, or multi-pass shaders) · screenshots · Windows screensaver
integration · Bluetooth/BLE features · OTA updates. Wi-Fi is in scope only for NTP sync
(Phase 6), the future side-bar weather feature (Phase 7), and the future local web interface
(Phase 9).
