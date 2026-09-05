# Johnny Castaway 2026 — Release and fidelity worklist

Updated: 2026-09-04

## ESP32 first-scene milestone — complete with accepted exceptions

Scope: show one authentic, deterministic Johnny scene at native 640x480 on the
Waveshare 800x480 display using the default Right layout. Full-story playback,
sound, settings UI, Wi-Fi, weather, CRT filtering and web management remain out
of scope until this milestone passes.

### Completed

- [x] Created `feature/esp32-first-scene` without committing or absorbing the
  pre-existing dirty files.
- [x] Moved the ESP plan to `esp32/PLAN.md` and kept commit `343c7f5` plus the
  current desktop decoder as behavior references.
- [x] Installed ESP-IDF v5.5.5 and its ESP32-S3 tools under the ignored `.tools/`
  directory; no global dependency installation is required.
- [x] Identified the connected target with `esptool` as COM3 at that time:
  ESP32-S3 revision 0.2, 16 MB flash and 8 MB embedded PSRAM. COM ports must be
  detected again before the next flash rather than assuming COM3 is unchanged.
- [x] Proved the toolchain with clean ESP-IDF configure/build and a complete
  verified device flash. A separate `hello_world` image was unnecessary because
  the real application build and all flash regions were verified directly.
- [x] Added the ESP-IDF project, N16R8 defaults, 3 MB application partition and
  1.5 MB raw `jcdata` partition.
- [x] Added isolated board, resource and graphics components. Board wiring,
  timings, CH422G reset/backlight and GT911 configuration follow the cached
  official Waveshare ESP-IDF demo.
- [x] Added separate default-scene and color-bar/touch-cursor build
  configurations. Both compile cleanly with 92 percent of the application
  partition free.
- [x] Added `make_jcdata.py`; generation rejects anything except the canonical
  source MD5 pair and records a payload CRC32 in the container.
- [x] Added read-only raw-partition mapping, boot-time header/CRC/source-MD5
  validation, resource lookup, and the required LZW/RLE, palette and screen
  decoding.
- [x] Added native indexed 4-bpp to RGB565 composition, clipping, all three
  layout offsets, and the default Right layout with a black `x=640..799`
  placeholder sidebar.
- [x] Added deterministic `INTRO.SCR` rendering and serial diagnostics for
  firmware, flash/PSRAM, data validation, selected resource, decoded SHA-256,
  render timing and touch coordinates.
- [x] Added the shared settings contract for `left | center | right`, independent
  `clock` and `weather`, and later `crt = off | soft` meanings.
- [x] Passed six Python host tests, including canonical data and settings
  validation. Passed the desktop Go decoder comparison byte-for-byte:
  packed-screen SHA-256
  `6eb9bed1fa948b537652cc4f37da9f4733e828d174a52900dfea98932eafaea1`
  and complete Right-layout RGB565 SHA-256
  `be6b74850333a3b2e9f1ecfadbfea7ada47d5b17b0e92b4ba09ff2f02e25ee40`.

### Superseded blocker

- [x] After the first verified flash and hard reset, the board temporarily stopped
  enumerating and Windows showed no present COM ports. The later corrected
  palette/panel build was subsequently flashed and observed at boot after the
  board was recovered in download mode.
- [x] Physical display and touch acceptance was kept separate from build, host,
  QEMU, flash and serial evidence.

### QEMU verification gate — completed 2026-08-27

ESP-IDF v5.5.5 supports ESP32-S3 QEMU and Espressif's virtual framebuffer.
The repo-local ESP-IDF tools now include `qemu-xtensa`, and both virtual
framebuffer profiles pass:

1. [x] Install `qemu-xtensa` under the ignored repo-local ESP-IDF tools
   directory; do not install it globally.
2. [x] Add a separate QEMU build configuration and board backend using
   `espressif/esp_lcd_qemu_rgb`.
3. [x] In the QEMU backend, bypass only the physical RGB-panel initialization,
   CH422G reset/backlight access, GT911 touch and related I2C operations.
4. [x] Keep the real partition table, `jcdata` mapping, resource decoder,
   palette conversion, graphics composition and deterministic scene path shared
   with the physical firmware.
5. [x] Generate the QEMU flash image with `jcdata.bin` at its real raw-partition
   offset and boot it through ESP-IDF's ESP32-S3 QEMU target.
6. [x] Capture bounded serial evidence for boot, partition mapping, container
   CRC, both source MD5 checks, decompression, selected resource, render timing
   and absence of panics or reboot loops.
7. [x] Assert the packed `INTRO.SCR` SHA-256
   `6eb9bed1fa948b537652cc4f37da9f4733e828d174a52900dfea98932eafaea1`
   and complete Right-layout RGB565 SHA-256
   `be6b74850333a3b2e9f1ecfadbfea7ada47d5b17b0e92b4ba09ff2f02e25ee40`
   from the embedded C execution path.
8. [x] Display and inspect both the 800x480 color bars and the native 640x480
   Right-layout scene in QEMU's virtual framebuffer.

QEMU evidence does not prove physical RGB wiring or red/blue ordering, panel
timing, orientation, drift or flicker, CH422G behavior, GT911 touch, real octal
PSRAM performance, power behavior or cold-boot stability. Those remain physical
hardware gates.

### Physical next steps — after the QEMU gate

1. [x] Reconnect or power-cycle the Waveshare board. If needed, hold BOOT, tap
   RESET, then release BOOT to enter download mode.
2. [x] Enumerate current Windows serial devices and run `esptool flash_id` on the
   actual port. Reconfirm ESP32-S3, 16 MB flash and 8 MB PSRAM; do not assume the
   port remains COM3.
3. [x] Flash the isolated `esp32/build-board-test` image and capture a bounded
   serial log showing a stable boot and the expected N16R8 memory values.
4. [x] Physically verify full-screen 800x480 color bars, correct orientation and
   no visible panel drift. Repeat across at least three cold boots.
5. [x] Touch the corners and center, confirm serial coordinates, cursor position
   and repeatability after reboot.
6. [x] Flash the corrected default build from `esp32/build`, including the
   generated `jcdata.bin` raw partition, and capture its complete serial boot
   diagnostics.
7. [x] Confirm boot-time container CRC and both source MD5 checks, decoded
   `INTRO.SCR` SHA-256, selected Right layout and render timing without panic or
   reboot loops.
8. [x] Physically verify the authentic Johnny scene occupies `x=0..639` at
   native resolution and the black placeholder sidebar occupies `x=640..799`.
   The user visually confirmed layout, colors, orientation and stability on the
   panel; a physical panel photo has not been captured for the milestone record.
9. [x] Close physical acceptance with an explicit exception: the repeated
   five-point touch check passed, and the user chose to waive the three
   scene-firmware cold boots. The skipped cold-boot evidence remains documented.

## ESP32 animated-story milestone — active

Scope: complete and verify authentic playback across the ordered 63-scene
catalog, then close the remaining engine-wide fidelity, lifecycle and physical
performance gates before adding later UI or network features.

The consolidated, ordered record of all scene-review findings, completed
repairs and remaining engine-wide fidelity work is
[`esp32/ALL_SCENES_FIDELITY.md`](esp32/ALL_SCENES_FIDELITY.md).
Use [`esp32/RUNBOOK.md`](esp32/RUNBOOK.md) for the current pickup, validated
command profiles and the required end-of-run handoff.

### Completed

- [x] Parse all 41 canonical TTM resources and all 10 ADS resources on the host.
- [x] Add bounded ESP32 TTM/ADS loaders, tag tables and release paths.
- [x] Match the first `MJAMBWLK.TTM` tag-1 interpreter tick to the desktop opcode,
  instruction-pointer and timing baseline.
- [x] Add canonical multi-sprite BMP decoding with strict sprite-table, packed-size
  and SHA-256 validation.
- [x] Render the two tag-1 sprites over `ISLETEMP.SCR`, including transparency
  and horizontal flip, and match the complete Right-layout RGB565 framebuffer
  SHA-256 `c073f88c2dfeb087d09d86097833704c2d3169f375794076d2c1c51071cb275a`
  in both host tests and ESP32-S3 QEMU.
- [x] Re-identify the physical board on its current port as ESP32-S3 revision
  0.2 with 16 MB flash and 8 MB PSRAM, flash the new first-frame firmware with
  verified writes, and capture a stable serial boot where every new parser,
  sprite and framebuffer hash gate passes. The user visually confirmed the new
  island background and first ambient Johnny pose on the physical panel.

### Next steps

1. [x] Route the verified TTM draw/load commands through the runtime command sink
   instead of constructing only the first frame directly in the boot gate.
2. [x] Add the bounded 10 Hz logic / 30 fps presentation loop for one complete
   `MJAMBWLK.TTM` subscene, including UPDATE, delay and pending GOTO behavior.
3. [x] The complete nine-frame sequence now matches the desktop reference in
   host tests, ESP32-S3 QEMU and physical-device runtime hashes. The timed replay
   passed on the device at 848,974 us, and the verified app image was flashed to
   the identified Waveshare board. On 2026-08-28 the user physically confirmed
   smooth motion without flashing after the off-screen composition fix.
4. [x] Route the verified ambient animation through the first bounded
   `STAND.ADS` scheduling slice, then validate its deterministic scene selection
   in host tests, QEMU and on the physical board before expanding ADS behavior.
   Tag 1 now schedules `MJAMBWLK.TTM` tag 42 followed by tag 1 through the ESP32
   ADS interpreter. Ten Python tests, Go tests, physical and QEMU builds, QEMU
   runtime, verified COM4 app flashing and physical serial execution passed. The
   physical timed replay completed in 847,775 us without panic or reboot.
5. [x] Expand the bounded ADS scheduler with scene-completion triggers and
   concurrent scene state. The canonical `ACTIVITY.ADS` tag-4 chain now advances
   from scene 2:1 to 2:3 to 2:2, while tag 1 schedules scenes 1:12 and 1:13
   concurrently. Eleven Python tests, Go tests, physical and QEMU builds, QEMU
   runtime, verified app flashing and physical serial execution passed. The
   physical timed replay completed in 870,026 us without panic or reboot.
6. [x] Materialize concurrent ADS scene actions as independent, ordered TTM
   runtime layers and composite their bounded draw lists through the existing
   off-screen framebuffer. The canonical `ACTIVITY.ADS` tag-1 actions now create
   `GJDIVE.TTM` tags 12 and 13 as separate runtimes, with composite RGB565
   SHA-256 `0085c76f798dd4d065caba2fcd294281a3c18a2b1b7664ef8e15767bfad1db30`.
   Thirteen host tests, Go tests, physical and QEMU builds, QEMU runtime, verified
   app flashing and physical serial execution passed. The device replay took
   873,659 us without panic or reboot; direct visual no-flash acceptance of this
   newly flashed build remains pending. A follow-up changes the verification
   handoff to retain the runtime and loop the ambient animation continuously
   instead of leaving its final pose on screen.
7. [x] Tick every running ADS-owned TTM runtime, apply completion/stop actions
   back into the runtime set, and validate the first complete multi-scene ADS
   event. Preserve the ten-thread and sixteen-draw-per-layer bounds; the
   canonical archive currently peaks at thirteen retained draws in one layer.
   With deterministic random value zero, `ACTIVITY.ADS` tag 1 completes the
   twelve-scene chain `1:12, 1:13, 1:8, 1:14, 2:2, 1:13, 1:9, 1:14, 2:2,
   1:13, 1:9, 1:11` across `GJDIVE.TTM` and `MJDIVE.TTM`, then returns to the
   exact island-background RGB565 SHA-256
   `a68119006596c3a6402f4fadadc945df10fb32481d2996b7d8b81cdb1c8be4f1`.
   Fourteen Python tests, uncached Go tests, physical and QEMU builds, QEMU
   runtime, verified COM4 app flashing and physical serial execution passed.
   The physical ambient replay completed in 866,200 us without watchdog,
   panic or reboot.
8. [x] Promote the verified ADS runtime set from the accelerated boot gate into
   the live native-centisecond timing / 30 fps presentation loop, initially for
   `ACTIVITY.ADS` tag 1. Present the complete multi-scene event on the physical
   panel and confirm ordered composition without flashing before adding a
   broader story scheduler. The implementation, 14 host tests, uncached Go
   tests, physical and QEMU builds, QEMU runtime, verified COM4 flashing and
   physical serial execution pass. A physical review exposed incorrect TTM
   timer reloads and 100 ms rounding during the tree climb. The corrected build
   reloads the active delay after every UPDATE and carries real elapsed
   centiseconds into the 30 fps presentation loop. A second physical review
   showed that stale copies remained because presentation copied into the RGB
   driver's active scanout buffer even though two panel buffers were allocated.
   The new build copies each completed composition into the inactive panel
   buffer and then selects it for scanout. A further review localized split
   figures to the second event, showing that nominal frame spacing alone did
   not guarantee that the old front buffer was safe to reuse. The current build
   waits for the RGB driver's frame-complete ISR before every reuse and passed
   two complete physical serial event cycles without timeout, watchdog, panic
   or reboot. A later physical review still found split Johnny figures during
   the second tree climb. The repeat-run-specific cause was shared BMP slots:
   `MJDIVE.TTM` loaded `JOHNWALK.BMP` into slot 0 and replaced the
   `MJDIVE.BMP` sprites still needed by `GJDIVE.TTM`. BMP slots are now owned by
   each TTM resource. The accelerated boot gate proves that the two resources
   retain different slot-0 images, QEMU passed repeated events, and the corrected
   app was flashed and hard-reset on the identified ESP32-S3. Physical serial
   completed the first live event in 25,352,067 us and progressed through the
   second event in order without timeout, watchdog, panic or reboot. The user
   then physically confirmed that Johnny remains a single correctly assembled
   figure during the second tree climb, completing visual acceptance.
9. [x] Start the broader story-scheduler port with the exact 63-entry desktop
   story catalog, including ADS name/tag, walk endpoints, story-day constraints
   and all eight eligibility flags. Add bounded eligibility and in-order
   selection APIs in `jcengine`; the existing live `ACTIVITY.ADS` loop remains
   unchanged. Fourteen Python host tests, the uncached Go suite (with the
   repo-local Raylib DLL on `PATH`) and the normal ESP-IDF 5.5.5 physical build
   pass. The new selector compiles into the firmware but has not yet been run in
   QEMU or on the physical board, so this is scheduler foundation rather than a
   full-story playback claim.
10. [x] Add deterministic selector fixtures for catalog bounds, day and flag
   filtering, no-match behavior and in-order cursor wrap. The firmware boot gate
   passes in QEMU and on the physical ESP32-S3. Load all six bounded
   `ACTIVITY.ADS` TTM resources and validate the second catalog event, tag 12,
   as the deterministic nine-completion `MJREAD.TTM` chain `4:110, 4:24, 4:98,
   4:100, 4:101, 4:105, 4:106, 4:103, 4:107`; it returns to the same exact
   island-background framebuffer hash as tag 1. Replace the hard-coded tag-1
   restart with a deliberately bounded in-order catalog slice alternating only
   `ACTIVITY.ADS` tags 1 and 12. Fourteen Python tests, uncached Go tests, the
   normal physical build and QEMU pass. The identified COM4 ESP32-S3 was
   app-flashed and hard-reset; physical serial completed tag 1 in 25,352,909 us
   and tag 12 in 11,582,720 us without timeout, watchdog, panic or reboot.
11. [x] Directly inspect repeated tag-1/tag-12 transitions on the physical panel.
   Confirm the existing dive/tree-climb event remains smooth and correctly
   assembled, and that the new reading event has correct sprites, ordering and
   a clean return to the island background. Do not add a third live catalog
   event until this visual gate is accepted. The first review accepted the
   transitions, ending, restart and stable black sidebar, but reported beach
   artifacts after the dive. Numbered graphical QEMU localized the report to
   `GJDIVE.TTM` tag 9 around presentation 165. The desktop scheduler retains a
   finished TTM layer for its final timer, while ESP32 had removed it immediately
   on `PURGE`; this skipped tag 9's 210-centisecond contextual finale. The
   runtime now presents and holds the final layer until that timer expires. A
   regression fixture proves the hold, QEMU completed tag 1 in 32,078,986 us and
   tag 12 in 14,910,394 us, and the corrected physical build was app-flashed and
   hard-reset. Physical serial completed tag 1 in 32,335,868 us and tag 12 in
   15,113,927 us without timeout, watchdog, panic or reboot. The user then
   confirmed the corrected sequence looks good, completing visual acceptance.
12. [x] Expand the bounded live catalog slice with its third in-order event,
   `ACTIVITY.ADS` tag 11. Its 28-scene `MJFISH.TTM` chain exposed the original
   sixteen-entry ADS played-state history bound; retain the desktop-compatible
   `IF_NOT_PLAYED` history and raise the bounded state capacity to 64 rather
   than discarding completed records. The accelerated firmware gate checks the
   exact completion order and accepted island-background hash. Fourteen Python
   tests, uncached Go tests, the normal ESP-IDF build and a full native-timed
   QEMU cycle pass. The board was re-identified as an ESP32-S3 with 16 MB flash
   and 8 MB PSRAM, app-flashed with verified writes and hard reset. Physical
   serial completed tags 1, 12 and 11 in 32,368,769 us, 15,113,824 us and
   15,435,034 us respectively, then restarted the slice without timeout,
   watchdog, panic or reboot. Direct visual acceptance of the fishing sequence
   was completed after a fresh hard reset; the user confirmed it looks good.
13. [x] Expand the bounded live catalog slice with its fourth in-order event,
   `ACTIVITY.ADS` tag 10. Add its exact thirteen-completion `MJREAD.TTM` chain
   `4:110, 4:24, 4:114, 4:108, 4:92, 4:93, 4:109, 4:84, 4:85, 4:119,
   4:96, 4:118, 4:116` to the accelerated firmware and host resource fixtures;
   require the accepted island-background hash at completion. Fourteen Python
   tests, uncached Go tests, the normal ESP-IDF build and a complete native-timed
   QEMU cycle pass. The normal app is 0x49250 bytes with 90 percent of its 3 MB
   partition free. COM4 was re-identified as ESP32-S3 revision 0.2 with 16 MB
   flash and 8 MB PSRAM, the app flash was hash-verified and hard-reset, and
   physical serial completed tags 1, 12, 11 and 10 in 32,336,206 us,
   15,114,131 us, 15,433,020 us and 17,335,118 us before restarting at event 1/4
   without timeout, watchdog, panic or reboot.
14. [x] Directly inspect tag 10 and the tag-10-to-tag-1 transition on the physical
   panel. Confirm correct sprites and ordering, no flashing or leftover pixels,
   a clean return to the island background, and the stable black sidebar. Do not
   add a fifth catalog event until this visual gate is accepted. The user
   confirmed the complete event and transition look good, closing the gate.
15. [x] Replace the accepted-event loop with the next five-event validation
   batch only: `ACTIVITY.ADS` tags 5, 6 and 9, then `BUILDING.ADS` tags 4 and 2.
   Show 1-5 in the right sidebar, reset to 1 on batch wrap, and add touch buttons
   for Pause/Play and deterministic Back 10 Frames. Fixed one-centisecond
   completion fixtures verify counts/order hashes of 12, 8, 11, 13 and 229.
   Fourteen Python tests, uncached Go tests, `git diff --check`, physical and
   headless/graphical QEMU builds, a graphical sidebar capture, and a complete
   native-timed QEMU cycle pass. COM4 was re-identified as ESP32-S3 revision 0.2
   with 16 MB flash and 8 MB PSRAM. The final `0x4a9a0` app was hash-verified,
   hard-reset and completed all five physical serial events in 21,277,625 us,
   7,367,995 us, 18,412,611 us, 30,963,052 us and 23,701,917 us before restarting
   at event 1/5 without timeout, watchdog, panic or reboot. Pause and Back 10
   Frames touch actions were also observed in serial on event 3.
16. [x] The five-event scene gate is accepted: events 1-3 look good, event 4's
   ship and tied-down rope remain visible without flashing, and event 5's boat
   remains visible throughout rowing and return. The rope fix retains and
   composites palette-colored TTM `DRAW_LINE` commands; its deterministic gate
   executes 700 lines while preserving the 13-completion hash. Fourteen host
   tests, normal and QEMU builds, accelerated QEMU regression, verified COM4
   full flash/hard-reset, and a full physical serial cycle pass. Scene 4 and 5
   complete in 31,006,794 us and 23,814,763 us. Event 5's pace is accepted, with
   a note that it feels very fast; it matches the desktop engine's authored
   centisecond rules, so no arbitrary slowdown was added.
17. [x] Expand the five-event validation sidebar with a 1-based displayed-frame
   counter, friendly and technical scene descriptions, and wrapping Scene -1 /
   Scene +1 controls. Manual navigation starts the selected event playing at
   frame 1; Back 10 Frames now targets the displayed frame exactly and remains
   paused. Deterministic firmware checks cover both wrap boundaries, frame-to-
   centisecond conversion and all four touch targets. Fourteen Python tests,
   uncached Go tests, normal and headless/graphical QEMU builds, native-timed
   QEMU transitions and graphical framebuffer inspection pass. COM4 was
   re-identified as ESP32-S3 revision 0.2 with 16 MB flash and 8 MB PSRAM. The
   complete image and `jcdata` partition were hash-verified, hard-reset, and the
   physical serial boot passed the new gate. A complete native-timed serial
   cycle then passed all five events and wrapped from Scene 5 back to Scene 1 at
   frame 1 without timeout, watchdog, panic or reboot.
18. [x] Turn the five-event batch into a RAM-only guided review. Natural
   completion now verifies and restarts the same scene at frame 1 until OK or
   Investigate is pressed. Review decisions advance to the next unreviewed
   scene, manual Scene -1 / Scene +1 still visits exact adjacent scenes, and an
   all-reviewed summary supports revisiting and replacing results. Deterministic
   firmware checks cover review advancement, wrap, replacement, completion and
   all six touch targets. Fourteen Python tests, uncached Go tests, normal and
   headless/graphical QEMU builds, graphical normal/summary captures and
   `git diff --check` pass. COM4 was re-identified as ESP32-S3 revision 0.2 with
   16 MB flash and 8 MB PSRAM; the complete `0x4f8e0` image and `jcdata` were
   hash-verified and hard-reset. Physical serial passed the new boot gate and
   confirmed Scene 1 completes with its unchanged hash and restarts at frame 1.
19. [x] Expand the guided review to the exact ordered 63-scene catalog. Rename
   Investigate to REVIEW and persist mutually exclusive OK/REVIEW masks, the
   catalog fingerprint and per-scene recoverable failure codes in NVS. Commit
   each decision before advancing, resume at the next unreviewed scene after a
   reset, and emit a Markdown-ready identity table at completion. The dedicated
   human ledger is `esp32/SCENE_REVIEW.md`; REVIEW-only playback remains gated
   on checking that completed ledger. Fourteen Python tests, uncached Go tests,
   normal and headless/graphical QEMU builds and graphical summary inspection
   pass. COM4 was re-identified as ESP32-S3 revision 0.2 with 16 MB flash and
   8 MB PSRAM; the `0x55ba0` app and `jcdata` were hash-verified and hard-reset.
   The first physical 63-entry boot exposed a watchdog warning in the catalog
   probe; yielding between records fixed it. The final serial boot passed every
   fixture, restored the compatible empty NVS review with catalog fingerprint
   `eb04eb66746ae074`, and started Scene 1/63 without watchdog, panic or reboot.
20. [x] Complete and confirm the direct 63-scene classification. The serial
   export and `esp32/SCENE_REVIEW.md` contain 29 OK, 34 REVIEW and zero
   unreviewed identities; none came from automatic failure handling. Keep
   Pause/Play, Back 10 Frames, reset/resume, wrap and artifact checks as direct
   physical acceptance gates rather than inferring them from serial evidence.
21. [x] Fix and re-review the schema-2 REVIEW-only shortlist. The current
   configuration reads the persisted REVIEW mask, numbers only those scenes
   `1/M`, removes an identity when marked OK, retains it when marked REVIEW,
   stops on ALL RESOLVED and exports the revised shortlist. The freshly rebuilt
   `0x5d400` REVIEW-only firmware was hash-verified and flashed to the identified
   ESP32-S3 with unchanged `jcdata`. Embedded boot gates passed and NVS restored
   `48 OK / 15 REVIEW / 0 unreviewed`. The user's direct physical-panel pass
   found these blockers; use the frame locations as regression checkpoints:
   **Current final-three slice (2026-09-03):** the user physically accepted
   `LEAVE THE ISLAND`, bringing the persisted device state to `60 OK / 3 REVIEW
   / 0 unreviewed`. A combined REVIEW-only image (`0x5eb40`, SHA-256
   `e274c15f7334fdcd912d7bf25aafcffb5f07b6c765ffe3ca6e141ad5f07cebf2`)
   was app-flashed to the re-identified COM4 ESP32-S3 rev 0.2 N16R8 without
   erasing NVS or rewriting `jcdata`. Serial restored all 60/3 decisions and
   opened `FIRST MERMAID GLIMPSE` as `SCENE 1/3`. Embedded and QEMU fixtures
   pass all three repaired completion chains and framebuffer checkpoints with
   no memory, draw-limit, missing-bitmap, watchdog, panic or restart-loop error.
   The final direct panel pass used this order:
   1. `FIRST MERMAID GLIMPSE` (`MARY.ADS#2`): at frame 356, confirm the fish is
      attached to the rod and shares its correct translated coordinates.
   2. `TANKER VISIT` (`VISITOR.ADS#3`): near frame 211, confirm the hull is
      complete and the large tanker bitmap moves left to right.
   3. `COCONUT HITS PLANE` (`VISITOR.ADS#5`): confirm the full continuation
      visibly shows Johnny hitting the plane with the coconut.
   The user completed these visual checks and reported all three resolved.
   **Final-one completion (2026-09-03):** the user physically accepted
   `FIRST MERMAID GLIMPSE` (the apparent fish issue was Johnny dreaming) and
   `TANKER VISIT` (complete hull and working traversal). The first
   `COCONUT HITS PLANE` candidate still placed the island too far right and
   showed a Johnny artifact at frame 2. The revised metadata
   fixes this event at island `x=-272` with TTM origin `x=0`; this also keeps
   GJVIS5 tag 8's authored `x=-185` entry sprite off-screen. Frame-2 and final
   framebuffer fixtures, completion order and peak retained draws are locked.
   REVIEW-only image `0x5ee20`, SHA-256
   `55f791db3a3a4af7962e9e4a9eb89202d87b09375eb02f36c3a63bdd6ede6f2b`,
   was app-flashed with NVS and `jcdata` preserved. Before the last decision,
   serial restored `62 OK / 1 REVIEW` and opened `COCONUT HITS PLANE` as
   `SCENE 1/1`; the user then physically confirmed the corrected framing,
   absence of the frame-2 artifact and complete plane strike, resolving the
   last scene. Final closure on 2026-09-03 re-identified COM4 as ESP32-S3 rev
   0.2 N16R8 and hard-reset the existing REVIEW-only image. Serial restored
   `ok=63 review=0 complete=1`, matched catalog fingerprint
   `eb04eb66746ae074`, emitted the empty schema-2 shortlist export and reported
   `REVIEW-ONLY: ALL RESOLVED` without watchdog, panic, memory/draw-limit,
   missing-bitmap or restart-loop errors. `SCENE_REVIEW.md` is reconciled.
   The numbered shortlist below is the historical repair ledger. Any older
   "re-review required" wording is superseded by the final accepted status
   above, but its scene identities and frame checkpoints remain regression data.
   - Shortlist 1 / global Scene 12, `PIRATES TIE JOHNNY`
     (`BUILDING.ADS#4`): ship pieces are missing around frame 320; around frame
     681, parts of the sail remain behind. A candidate repair now translates
     stored-area operations by the active island offset and applies buffer
     replacement semantics. REVIEW-only image `0x5d5b0`, SHA-256
     `4c94f7293f1397393d59015a767a1a13f93cba5cd4a8b9cb592d9bdbff3fea85`,
     was verified and flashed. The user physically accepted both checkpoints on
     2026-09-01; serial confirmed NVS persisted OK and removed it from the shortlist.
   - Shortlist 2 / global Scene 14, `LILLIPUTIAN BOAT`
     (`BUILDING.ADS#2`): the lower-left sandcastle section is missing around
     frame 195. A first clip-retention candidate failed direct panel review.
     The revised candidate preserves saved pixels during transparent
     composition-to-stored `DRAW_SCREEN`, matching the desktop renderer.
     REVIEW-only image `0x5da40`, SHA-256
     `b771909a700875022211426accb77527fe2724f8feacdedf5e59e0f14cdfffdf`,
     was verified and flashed; QEMU was skipped. The user physically accepted
     the complete lower-left section around frame 195 on 2026-09-01; confirm
     that the device persists OK and removes it from the shortlist. Confirmed
     after reset on 2026-09-02: NVS restored `50 OK / 13 REVIEW`.
   - Shortlist 3 / global Scene 16, `COOK AND EAT` (`BUILDING.ADS#7`): two
     Johnnys remain visible after frame 340 and playback continues beyond frame
     2000 without finishing. Bounded negative-`ADD_SCENE` lifetime handling now
     ends the event after 66 completions. Two consecutive runs completed and
     restarted without panic, watchdog or reboot; the user physically accepted
     the scene and NVS restored `51 OK / 12 REVIEW` on 2026-09-02.
   - Shortlist 4 / global Scene 17, `NIGHT LILLIPUTIANS`
     (`BUILDING.ADS#6`): the boat leaves pieces behind around frame 690 and the
     scene never ends. With bounded scene lifetimes it completed and restarted
     cleanly twice; the reviewer marked it OK on the panel on 2026-09-02.
   - Shortlist 5 / global Scene 21, `FISHING CATCH ONE` (`FISHING.ADS#4`): the
     island and Johnny are out of sync; Johnny starts partly outside the frame
     on the left. A separate TTM origin now matches the desktop engine's +272 X
     adjustment for `LEFT_ISLAND` scenes without moving the island background.
     REVIEW-only image `0x5dc90`, SHA-256
     `d4ec4cf4a8d660509bc4c74d5226cb17b4fe8cefccc93a7e4308e525b0e4abac`, was
     verified, flashed and hard-reset. The scene completed and restarted three
     times with the original 32-draw bound and a measured peak of 6; the user
     physically accepted the alignment on 2026-09-02. A subsequent app-only
     reflash restored `59 OK / 4 REVIEW`, confirming persistence of this and six
     additional panel decisions.
   - Shortlist 6 / global Scene 24, `SHORE FISHING JUNK` (`FISHING.ADS#7`):
     Johnny starts left of the island while the island is positioned right.
     The user confirmed the alignment mismatch again on the physical panel on
     2026-09-02. The shared left-island origin repair is now flashed; keep it
     REVIEW until the repaired scene is accepted directly on the panel.
   - Shortlist 7 / global Scene 25, `SHORE FISH KEEPERS` (`FISHING.ADS#8`):
     Johnny and the island were misaligned. The same shared repair is flashed;
     direct panel re-review remains required.
   - Shortlist 8 / global Scene 32, `DINNER WITH MERMAID` (`MARY.ADS#1`): the
     table disappears around frame 223; the record player also disappears when
     dancing starts around frame 858.
   - Shortlist 9 / global Scene 34, `FIRST MERMAID GLIMPSE` (`MARY.ADS#2`): on
     2026-09-03 the fish was not attached to the rod and its coordinates were
     wrong at frame 356. The combined candidate implements the missing circle
     geometry and bounded `DRAW_GETPUT` replacement. Physically accepted: the
     apparent issue was Johnny dreaming and the scene looks correct.
   - Shortlist 10 / global Scene 35, `MERMAID BREAKUP` (`MARY.ADS#4`): two
     Johnnys appear around frame 129, one building the boat and one talking to
     the mermaid.
   - Shortlist 11 / global Scene 36, `LEAVE THE ISLAND` (`MARY.ADS#5`): resolved;
     the user reported that it looks OK and physically accepted it on
     2026-09-03.
   - Shortlist 12 / global Scene 56, `TANKER VISIT` (`VISITOR.ADS#3`): at frame
     211 the hull is incomplete, and the large tanker bitmap should move from
     left to right. Physically accepted after repair: the hull is complete and
     the traversal works.
   - Shortlist 13 / global Scene 57, `COCONUT FALLS` (`VISITOR.ADS#4`): the
     scene is incomplete and no coconut falls.
   - Shortlist 14 / global Scene 59, `CHASE THE COCONUT` (`VISITOR.ADS#7`): the
     scene is incomplete and no coconut falls.
   - Shortlist 15 / global Scene 60, `COCONUT HITS PLANE` (`VISITOR.ADS#5`):
     the scene is incomplete; it needs the remaining frames that visibly show
     Johnny hitting the plane with the coconut. Physically accepted after the
     fixed-left-island repair removed the frame-2 artifact and kept the plane
     in frame through the strike.
   The combined candidate resolves the `GJVIS5.TTM` slot-1 dependency through
   canonical resource execution. Do not change any result to OK until the
   corrected scene is directly accepted on the physical panel.
22. [ ] **Phase 4:** Add the `HOLIDAY.BMP` Special Days overlay as a
   persisted selector: Off / Automatic / Halloween / St. Patrick / Christmas /
   New Year. Automatic uses the curated `esp32/special_days.csv` calendar
   ranges; forced modes preview one overlay regardless of date.
   `JCENGINE_STORY_HOLIDAY_NOK` scenes suppress the overlay temporarily without
   changing the saved selection. Verify every date boundary, the New Year
   year-wrap, reboot persistence and physical composition. Easter remains
   checklist-only until a canonical date rule and fifth overlay are available.
23. [ ] **Random playback and authenticated web control:** The implementation
   now uses a 63-entry Fisher-Yates bag in normal firmware, ignores review masks,
   resumes the bag after one-shot scene selection, skips failed scenes, and
   persists immediately applied Sky/Special Days controls. The bounded HTTP
   component provides setup Wi-Fi, PBKDF2 administrator verification, opaque
   hashed sessions, acknowledged display commands and all `/api/v1/` routes.
   On 2026-09-03 COM4 was re-identified as ESP32-S3 rev 0.2 N16R8, fully erased,
   flashed and hard-reset. Serial passed canonical resource and embedded gates,
   started `Johnny-59D8` at `192.168.4.1`, and showed multiple randomized scene
   transitions without panic/watchdog/reboot. Twenty-seven Python tests plus
   normal (`0xf82e0`, 68% free) and REVIEW-only (`0x5f910`, 88% free) builds pass;
   QEMU was intentionally skipped. The Go suite is blocked by the absent local
   `raylib.dll`. Wi-Fi/API/reboot persistence and direct visual acceptance of
   smoothness, switching, Day/Night, all holidays, suppression, stale pixels
   and drift remain pending.

### ESP32 next leg: runtime review, bug log and story blocks — added 2026-09-04

24. [ ] **Settings and authenticated API contract**
    - Added: 2026-09-04
    - Status: implemented; authenticated live mutation/persistence pending
    - Changes: Add persisted `normal | review` playback mode and the fourth Sky
      value `cycle`; migrate existing Sky/Holiday settings without loss. Extend
      status and settings JSON and add authenticated bug-log routes.
    - Validation: 2026-09-04 host source/API contract tests pass in the 31-test
      Python suite; both ESP-IDF profiles and strict QEMU pass. On the flashed
      board, unauthenticated status and bug requests both return HTTP 401;
      the configured administrator password authenticates through the real
      Chrome webpage and loads the protected status, settings, scene catalog
      and bug-log views.
    - Remaining gate: authenticated live API mutation and reboot-persistence test
    - Closed:
25. [ ] **Runtime ordered Review mode**
    - Added: 2026-09-04
    - Status: implemented; authenticated live and physical validation pending
    - Changes: Run scenes 1-63 in order, repeat the current event until Looks
      OK, Bug, Previous or Next, and resume the preserved shuffle on exit.
      Keep the completed `jc_review` ledger unchanged.
    - Validation: 2026-09-04 host contract tests, normal/review builds and strict
      QEMU pass; runtime self-checks cover Review parsing/navigation wiring.
    - Remaining gate: authenticated live navigation and physical control review
    - Closed:
26. [ ] **Persistent webpage bug log**
    - Added: 2026-09-04
    - Status: implemented; authenticated live and physical web validation pending
    - Changes: Keep one latest sanitized `jc_bug` record per scene; expose
      capture, copy-one, Copy All, resolve and confirmed Clear All actions.
      Allocate full-log capture/read/clear buffers from the heap so the web task
      does not carry a catalog-sized array on its stack.
    - Validation: 2026-09-04 host tests confirm separate `jc_bug` persistence,
      authenticated routes, report metadata and secret exclusion; flashed-board
      unauthenticated access returns HTTP 401 as required.
    - Remaining gate: persistence, sanitization, copy fallback and physical web QA
    - Closed:
27. [ ] **Ten-scene Day/Night Cycle**
    - Added: 2026-09-04
    - Status: implemented; physical sequence validation pending
    - Changes: Alternate ten completed scene transitions in Day and ten in
      Night; do not count repeats or rewinds and restart at Day after reboot.
    - Validation: 2026-09-04 firmware self-check covers Day at boot, Night after
      ten transitions and Day again after twenty; strict QEMU and physical-board
      boot self-checks pass.
    - Remaining gate: direct-panel ten-transition sequence review
    - Closed:
28. [ ] **Block-stable island placement**
    - Added: 2026-09-04
    - Status: implemented; firmware and direct-panel validation pending
    - Changes: Reuse a variable island anchor for each ten-scene block; limit a
      new block anchor to 64 horizontal and 32 vertical pixels of movement while
      retaining authored fixed-left behavior.
    - Validation: 2026-09-04 firmware self-check covers the 64-pixel horizontal
      and 32-pixel vertical block-boundary limits; QEMU and physical boot logs
      show the same variable anchor reused across independent scene starts.
    - Remaining gate: direct-panel continuity and composition acceptance
    - Closed:
29. [x] **Automated validation**
    - Added: 2026-09-04
    - Status: complete
    - Changes: Cover migration, APIs, review navigation, bug records, cycle
      boundaries, replay/rewind invariants and island placement; run catalog,
      Python, uncached Go, normal/review builds and strict QEMU fixtures.
    - Validation: 2026-09-04: 31 Python tests, including reconstruction and
      Node syntax validation of the embedded page script, catalog generation
      check, `git diff --check`, uncached Go tests, normal firmware (`0xfa770`, 67%
      free), legacy REVIEW-only firmware (`0x5fdc0`, 88% free) and strict QEMU
      all pass. Initial QEMU checks exposed and closed a stack-allocation fault
      and cross-fixture island-anchor leak before the final passing run.
    - Remaining gate: none
    - Closed: 2026-09-04
30. [x] **Physical firmware flash and serial gate**
    - Added: 2026-09-04
    - Status: complete
    - Changes: Re-identify the ESP32-S3, flash normal firmware, hard-reset and
      capture complete serial boot evidence.
    - Validation: 2026-09-04 COM4 was positively identified as an ESP32-S3
      revision 0.2 with 16 MB flash and 8 MB embedded PSRAM. The normal image
      and `jcdata` hashes verified during flash, RTS performed the hard reset,
      PSRAM memory test passed, all 63 scenes loaded/started, normal shuffled
      playback began and the authenticated web service joined the LAN. Later
      the same day, the user-authorized `0x9000`/`0x6000` NVS partition reset
      and normal reflash both passed; serial confirmed a fresh `Johnny-59D8`
      setup AP at `192.168.4.1` for new Wi-Fi/admin-password provisioning. The
      final corrected `0xfa770` image was then reflashed without erasing the new
      settings; serial confirmed LAN service at `192.168.1.230`.
    - Remaining gate: none
    - Closed: 2026-09-04
31. [ ] **Direct-panel and webpage acceptance**
    - Added: 2026-09-04
    - Status: awaiting user acceptance on the flashed board
    - Changes: Verify review controls, copied bug reports, reboot persistence,
      Day/Night blocks, island continuity, composition and smooth playback.
    - Validation: 2026-09-04 flash and serial gates pass; the live service is
      reachable and rejects unauthenticated status/bug requests with HTTP 401.
      The real Chrome login succeeds with the configured password and renders
      current status, settings, all 63 scenes and Bug Log with no console
      warnings or errors. The earlier `login is not a function` collision and
      embedded-script newline syntax fault are repaired and regression-tested.
      Before resuming acceptance work, COM4 was re-identified again as the same
      ESP32-S3 revision 0.2, MAC ending `59:D8`, with 16 MB flash and 8 MB
      embedded PSRAM; identification changed neither firmware nor NVS. A
      subsequent monitor reset produced a complete clean boot, passed the PSRAM
      test, resource hashes, deterministic runtime fixtures, exact rewind and
      all 63 load/start checks, restored station credentials, rejoined the LAN
      at `192.168.1.230` and restarted SNTP without panic or watchdog. Browser
      session/settings and bug-record persistence still require authenticated
      verification. After that reset, the root page still returned HTTP 200
      with the administrator login and unauthenticated `/api/v1/status` still
      returned HTTP 401. No controllable browser surface was available in this
      run, so no credential or authenticated state was read or changed.
    - Remaining gate: copied-report/action QA, authenticated settings/session
      and bug-record persistence, and user physical display acceptance
    - Closed:
32. [ ] **Web-controlled physical reviewer sidebar**
    - Added: 2026-09-04
    - Status: RC4 implementation, automated, build, package, flash and serial
      gates complete; authenticated physical acceptance remains
    - Changes: Persist `sidebar_mode = off | clock | review`, default Off, and
      expose it through the authenticated webpage/API independently of
      Normal/Review playback. Off is black with no sidebar touch controls;
      Clock combines SNTP time/date and Open-Meteo weather; Review shows the
      existing reviewer panel. Wi-Fi setup credentials and dedicated
      REVIEW-only diagnostics remain visible regardless of this setting. Clock
      keeps Johnny at native 640x480 and renders city, time/date, colour weather
      icon, condition, temperature/high-low, `DATA FROM METEO`, local update
      time and stale state. The web/setup pages use the Windows Castaway Lookout
      SVG favicon.
    - Validation: All 41 Python tests, catalog generation, embedded JavaScript
      syntax validation, uncached Go tests, `git diff --check` and the normal
      RC4 build pass. The `0x126fc0` image has 62% app-partition free and SHA-256
      `82cc2dcb528532d5f5eeec866d1676c588f181afa102e6e57ce30caa88c28267`.
      The release flasher produced byte-identical canonical data, identified
      COM4 as ESP32-S3 revision 0.2 N16R8, verified every write and hard-reset
      without erasing NVS. Serial previously passed the icon, PSRAM/data/runtime
      and all 63 catalog-start gates; live root and favicon returned HTTP 200.
      The user accepted the default-Off black sidebar.
    - Remaining gate: log in, search/select the intended city, choose Clock and
      physically confirm icon clarity/colours, `DATA FROM METEO`, the displayed
      local update time, time/date/weather layout and
      native scene smoothness;
      then verify Reviewer, Off, reboot persistence, stale handling and disabled
      Off/Clock reviewer hitboxes. Do not rerun broad automated suites.
    - Closed:

### ESP32 Windows-aligned SCR and island lifecycle — approved 2026-09-01

Scope: replace the guided review's fixed `ISLETEMP.SCR` assumption with the
canonical Windows resource and island lifecycle. Keep `RESOURCE.MAP` and
`RESOURCE.001` byte-identical between targets; do not create ESP32-only SCR
copies or a TTM-to-screen lookup table.

1. [x] Add one runtime SCR loader which decodes through `jcrez`, renders through
   `jcgfx`, records the active resource name and framebuffer hash, releases the
   temporary decoded screen, and clears the stored-area overlay on every load.
2. [x] Execute TTM `LOAD_SCREEN 0xF01F` in authored order, including repeated
   loads. Cover the five TTM-selected backgrounds: `ISLETEMP.SCR`,
   `ISLAND2.SCR`, `SUZBEACH.SCR`, `JOFFICE.SCR`, and `THEEND.SCR`.
3. [x] Add island-event initialization and non-island teardown. Automatic sky
   uses `NIGHT.SCR` before 06:00 and from 18:00 when local time is valid;
   otherwise it deterministically selects `OCEAN00.SCR`, `OCEAN01.SCR`, or
   `OCEAN02.SCR`. An invalid clock must fall back to daytime.
4. [x] Derive a stable event seed from ADS identity and story day. Preserve the
   chosen ocean, tide, placement, clouds, and animation state across restart and
   exact Back 10 Frames replay. Apply `LOWTIDE_OK`, `VARPOS_OK`, `LEFT_ISLAND`,
   `NORAFT`, and `HOLIDAY_NOK` with the desktop rules.
5. [x] Port the complete background stack: `MRAFT.BMP` raft stages;
   `BACKGRND.BMP` island, tree, shadow, shore, rock, waves and clouds; and
   `HOLIDAY.BMP` calendar overlays. Retain the authored animation cadence and
   compose SCR/island, clouds, stored area, TTM layers, holiday, then sidebar.
   Apply the scene's island offset to both island assets and TTM drawing.
6. [x] Add a separate versioned NVS story namespace containing day 1-11 and the
   last valid local calendar date. First valid time initializes the date without
   advancing; later date changes advance once and wrap 11 to 1. Invalid time
   never mutates the persisted day. Do not alter the review-mask schema.
7. [x] Add injectable engine state tests for sky boundaries, invalid time,
   story-day initialization/increment/wrap, holiday boundaries and suppression,
   tide, raft, placement, stable randomization, cadence and rewind.
8. [x] Add fixtures for all ten canonical SCR dimensions, decoded hashes,
   640x350 dominant-bottom padding and Right-layout RGB565 hashes. Add runtime
   gates for representative `F01F` loads, island variants and z-order.
9. [ ] Run Python tests, uncached Go tests, normal ESP-IDF and QEMU builds, all
   boot gates and graphical captures. Confirm that canonical `jcdata` is
   unchanged and that the sidebar remains outside the 640x480 scene.
10. [ ] Re-identify the ESP32-S3 and serial port, flash firmware plus `jcdata`,
    hard-reset, and confirm clean serial boot. Physically inspect every SCR
    class, island/tide/raft/cloud/wave/holiday layer, transitions, pause/replay
    and Back 10 Frames before changing `SCENE_REVIEW.md` results.

The following remain outside this phase: audio, Wi-Fi/SNTP provisioning,
walking/pathfinding, missing geometric TTM opcodes, and user-facing sky or
holiday controls. Internal Automatic/Day/Night state defaults to Automatic.

Session-close checkpoint, 2026-09-01: items 1-6 are implemented. Item 7 has
firmware fixtures for deterministic state, sky boundaries, invalid time,
story-day progression, holiday suppression, raft/placement and eight-
centisecond wave/cloud cadence, but exact rewind and rendered z-order gates are
still open. Item 8 has all ten host SCR/hash/padding fixtures and five `F01F`
boot fixtures; rendered island variants/z-order remain open. Python (20 tests),
uncached Go and the normal ESP-IDF build passed. Headless QEMU passed the new
SCR and island-state boot gates, then correctly rejected the obsolete shared
`ISLETEMP` hash at validated event 1. One diagnostic run captured the new event
1 hash as `6c2daed21572dff3a5e5ccf6ff12e440500e34542d9c55a1fd7f01f1261d8ba8`.
The subsequent fix separates the injected island seed from the unchanged ADS
decision seed; that final source state has not yet been rebuilt. Graphical QEMU,
device identification, flash, serial and every panel check remain undone. No
review-ledger result was changed.

The next normal firmware uses review schema 2, intentionally invalidating the
old device-side `jc_review` masks so the SCR/island code receives a fresh
63-scene pass. The separate `jc_story` namespace and the historical
`esp32/SCENE_REVIEW.md` ledger remain unchanged. Each undecided scene restarts
from frame 1 after authored completion; the completed 63-scene pass stops on its
summary rather than silently wrapping.

Follow-up validation, 2026-09-01: all five replacement island-aware framebuffer
hashes are now strict per-event fixtures. Python, uncached Go, normal ESP-IDF and
headless QEMU gates pass. COM4 was re-identified as ESP32-S3 revision 0.2 with
16 MB flash and 8 MB PSRAM; normal firmware plus unchanged `jcdata` were
hash-verified, hard-reset, and serial confirmed the clean schema-2 Scene 1/63
start. Scene 1 completed and restarted from frame 1 twice with stable island
state and no panic, watchdog or reboot. Graphical QEMU and direct panel review
remain open; `SCENE_REVIEW.md` is still unchanged.

The first direct panel run reported very low FPS. The live and rewind paths now
batch island wave/cloud background rebuilding: all centisecond engine ticks are
retained, but only the final state is rendered once per catch-up/presentation
pass. Follow-up on 2026-09-03 added exact fresh-replay Back 10 Frames gates at
MARY tag 2 frame 346 and VISITOR tag 3 frame 201, plus rendered island
progression/checkpoint/final hashes across the accelerated eight-event fixture.
Twenty-three Python tests, uncached Go tests, normal and REVIEW-only builds, and
strict headless QEMU pass. Item 9 remains open for graphical QEMU/captures and
item 10 remains open for the new flash and direct panel smoothness/control gate.

### Johnny Castaway Enhanced fidelity audit — Phase 3 gates

Reference: compare behavior with
[`fbreve/Johnny-Castaway-Enhanced`](https://github.com/fbreve/Johnny-Castaway-Enhanced)
at commit `ad676571eb1bba210a74aa205fe0a760fc13a672` (2026-08-18). Treat it as
evidence, not as unquestioned canonical behavior; validate resource semantics
against the desktop decoder, fixtures, QEMU and the physical panel.

1. [ ] Implement `TIMER 0x2022` as a uniform random duration from minimum to
   maximum. Give live playback a non-constant seed while retaining an injectable
   deterministic seed for tests; every current ESP32 story starts with seed 0.
2. [ ] Separate each ADS thread's original/root tag from its current TTM tag.
   Update only the current tag on `0x1101`/`0x1111`, and use the root tag for
   stop, running and completion ownership. Add regressions for MARY tag 3 and
   plane-visitor ghosting.
3. [ ] Wire TTM `SET_CLIP_ZONE 0x4004` into runtime drawing and implement the
   missing `DRAW_RECT 0xA104`, `DRAW_PIXEL 0xA002` and `DRAW_CIRCLE 0xA404`
   commands. Include tanker `VISITOR.ADS` tag 3 and dive/climb resources in the
   fixtures.
4. [ ] Complete saved-zone behavior for `SAVE_ZONE 0xA054` and
   `RESTORE_ZONE 0xA064`, preserving the required pixels across clears and for
   sprites, rectangles and lines. The upstream README promises scoped restore,
   but its current implementation still saves nothing and clears the whole
   saved layer on restore, so derive the final contract from canonical data and
   verified playback rather than copying that implementation.
5. [ ] Add local and global `IF_LASTPLAYED` ADS trigger chaining/gating with
   root-tag matching and correct repeat, stop and completion handling.
6. [ ] Add explicit scene z-order policies for direction-aware plane/tree
   ordering and WALKSTUF/WOULDBE Johnny-on-top behavior. Identify layers by
   ADS/resource/root tag and direction, never by transient thread index.
7. [ ] Finish full story lifecycle behavior: island, walk, clouds, waves,
   day/night and story transitions, including clean island-layer teardown for
   non-island scenes and safe resource reuse.
8. [ ] After the fidelity gates above pass, consider iris transitions,
   single-frame/max-speed debug controls and 1/4/8-layer performance benchmarks.

Do not duplicate work already present: RGB565 offscreen/panel buffering,
per-resource bitmap slots, retained final TTM frames, the PSRAM stored overlay,
`DRAW_LINE`, bounded `DRAW_SCREEN`, and the 63-scene review catalog. Windows
screensaver lifecycle, multi-monitor/HDR/console/Mesa/OpenGL support, true
widescreen scaling and sound/silent-time behavior are not ESP32 targets. Keep
the existing simple Off/Soft CRT and touch/web settings phases behind the core
Phase 3 fidelity work.

## Windows 11 installer package

- [x] Add a per-user Inno Setup package for the native x64 EXE and SCR without
  copying files to System32 or requiring administrator rights.
- [x] Add verified Internet Archive `scrantic-run.zip` download/local-import
  support and keep its resource pair separate from the canonical hash profile.
- [x] Add optional verified JCOS WAV installation and a Start Menu setup action
  so the 23 sound effects can be downloaded later.
- [x] Register the screensaver only after required data setup succeeds and
  preserve the previous current-user screensaver for uninstall restoration.
- [ ] User test: first install, app launch, sound playback, `/c`, `/s`, Windows
  preview, idle activation, later sound installation, and uninstall.
- [x] Published the Windows RC3 installer and standalone EXE/SCR on 2026-08-25.
  Clean-account acceptance remains a separate gate before stable promotion.

## ESP32 7-inch Touch release package

Current release: [ESP32 2026.1.0 stable](https://github.com/DrWize/Castaway-Lookout/releases/tag/v2026.1.0),
published and download-verified 2026-09-05. The accepted RC4 runtime was rebuilt
with stable version metadata only. Earlier RC4 publication evidence follows.

Published 2026-09-05 as the [Castaway Lookout ESP32 RC4 prerelease](https://github.com/DrWize/Castaway-Lookout/releases/tag/v2026.1.0-rc.4).
The public ZIP and checksum were downloaded and verified. See the
[release tracker](docs/RELEASE_READINESS_PLAN.md) for source revision and hashes.
The earlier validation below is historical; no tests/builds/flashing were rerun
for publication, and the direct-panel gate remains open.

- [x] Add a single end-user flashing page at
  `docs/FLASH_ESP32_7_TOUCH.md`, covering supported hardware, original-data
  preparation, automatic flashing, first boot, 2.4 GHz Wi-Fi setup and recovery.
- [x] Add a double-click Windows flasher that downloads and SHA-256-verifies
  official Espressif esptool 4.12.0, verifies the canonical resource pair,
  creates private `jcdata.bin`, discovers COM ports and proceeds only after an
  ESP32-S3 N16R8 identity check.
- [x] Add reproducible RC4 ZIP packaging with bootloader, partition table,
  application, firmware checksums, flasher and guide. Do not include
  copyrighted `RESOURCE.MAP`, `RESOURCE.001` or generated `jcdata.bin`.
- [x] Validate the packaged script against an invalid-port no-write path and
  the physical COM4 board. The local data image matched the normal build
  byte-for-byte; all flash regions verified and RTS hard-reset the board without
  erasing NVS.
- [x] Directly accept the latest Clock/weather/Reviewer presentation and control
  persistence on the panel. Closed 2026-09-05 by explicit user confirmation,
  including smooth playback, switching and controls.
- [x] Confirm weather refresh after reboot and revise the `STALE DATA` wording.
  Implemented 2026-09-05: update time/saved/waiting labels on panel/controller;
  five focused checks, controller syntax/state execution and normal build pass.
  Identified N16R8 app-flash verified, NVS preserved and board RTS rebooted.
  Serial confirms a fresh saved-location forecast at 91.683 seconds after boot.
- [x] User visual acceptance of the new weather wording on the updated firmware.
  Closed 2026-09-05: user confirmed "looks good".
- [x] Publish the accepted follow-up to main and refresh RC4 downloads.
  Closed 2026-09-05: source `38a478c`; ZIP/checksum anonymously downloaded
  and byte-verified. See the release tracker for firmware and archive hashes.

## Ordered work queue

Complete these items from top to bottom. Later work should not delay the release
unless it exposes a stability, data-safety, or fidelity regression.

1. [x] **Finish the remaining release-critical fidelity comparison.**
   Closed 2026-09-05 by explicit user confirmation that these checks passed.
   Historical reference scope and limitations below are retained for context.
   Compare long-running scene order, transitions, timing, cloud speed and
   wrapping, waves, tides, holidays, and day/night behavior. The 11-hour video
   samples at the beginning and around 1, 5, 10, and 11 hours confirm the
   640x480 composition and horizontal cloud drift, but all sampled sections are
   daytime. Find a separate trustworthy original night reference before making
   further night-palette or transition changes. If no reliable night reference
   is available, keep the limitation documented and proceed.

2. [x] **Publish the RC4 source snapshot to `main`.**
   Correction recorded 2026-09-05: RC3 is published, but live GitHub main was
   still `343c7f5`; RC4 source and packaging were pending on the ESP32 branch.
   Closed 2026-09-05: `039813d` is verified on main and the repository is now
   `DrWize/Castaway-Lookout`. Follow [the release tracker](docs/RELEASE_READINESS_PLAN.md)
   for RC4 prerelease publication and download verification. Generated
   executables and original game data remain excluded from source control.

3. [x] **Create `v2026.1.0-rc.3` with the Windows installer.**
   Published 2026-08-25 with the paired native binaries, per-user installer,
   verified data setup and Castaway Lookout icon.

4. [ ] **Complete the full separate-account Windows screensaver checklist.**
   Update 2026-09-05: user checked the RC installer and confirmed it is OK;
   Windows stable published with that installer acceptance. The full checklist
   below remains follow-up evidence, not a claim that every scenario was tested.
   Verify clean first launch with the default `scrantic` convention, explicit
   `--data-dir`, saved Data Files selection, native x64 startup, `/c`, `/s`, and
   Windows Screen Saver Settings preview/install behavior. Also verify missing
   data errors, saved settings, unsigned-binary warnings, and normal input exit.

5. [x] **Promote the accepted ESP32 candidate to `v2026.1.0` stable.**
   Closed 2026-09-05: tag/source `4f3db04`, stable build and source CI passed;
   identified N16R8 app flash/reboot and fresh-weather serial verified. Latest
   stable ZIP/checksum publicly downloaded and byte-verified; exact hashes are
   in the release tracker. Windows stable was also published 2026-09-05 after
   user RC installer acceptance: source `f6d2921`, native builds/tests/vet and
   CI passed; public installer/EXE/SCR/checksums/manifest byte-verified. Full
   screensaver follow-up evidence remains in item 4.

6. [ ] **Run the physical CRT performance matrix.**
   Use `F9` at 1920x1080, 3840x2160, 5120x1440, and 7680x2160, including at
   least one integrated or lower-powered GPU. Record Off, Lightweight, Fast,
   HDR Pop, and Lottes results. Confirm that shader work is limited to the
   centered 4:3 viewport rather than the 32:9 pillarboxes.

7. [ ] **Set automatic shader defaults only after physical measurements.**
   Add conservative inadequate-performance thresholds with a user override.
   Keep the existing shader compile/capability fallback regardless of measured
   defaults.

## TTM display labels

No resource, file, or runtime identifier is renamed. Settings uses these display
labels and descriptions inferred from the original TTM tag metadata while
retaining each exact filename for loading, diagnostics, and command-line use.

Implemented presentation and storage decisions:

- The Settings selector is titled **Choose scene collection**.
- Each selection shows `Friendly title (RESOURCE.TTM)` as its primary label and
  the embedded-description evidence in smaller text directly below it.
- The 41-entry catalog is built into the application, not written to
  `JohnnyCastaway.ini`; the INI remains a compact persistent-settings file.
- Settings displays the one shared `JohnnyCastaway.ini` path used by the
  application and screensaver.
- Runtime loading, `--ttm`, logs, and diagnostics continue to use exact TTM
  filenames. PNG metadata records both `Content` and `Content Label`.

| Existing TTM | Display name | Embedded description evidence |
| --- | --- | --- |
| `FIRE.TTM` | Campfire Effects | Small through extra-large flame, smoke, wood, and rubbing sticks |
| `FISHWALK.TTM` | Fishing Walk | Fishwalk |
| `GFFFOOD.TTM` | Food Gag | Load food |
| `GJCATCH2.TTM` | Catch Gag | Catch gag and shaking fist |
| `GJDIVE.TTM` | Diving Gags | Belly flop, flip, cannonball, and bubbles |
| `GJGULIVR.TTM` | Gulliver and the Lilliputians | Sleeping, tied up, and Lilliputians sailing in and ashore |
| `GJGULL1.TTM` | The Seagull and the Book | Seagull, book, cleaning, and Johnny getting mad |
| `GJHOT.TTM` | Hot Summer Day | Hot summer day, fan speeds, and Johnny crumbling |
| `GJLILIPU.TTM` | Lilliputian Attack | Lilliputians sail in, cannon fire, and planes launch |
| `GJNAT1.TTM` | Johnny's Rain Dance | Rain cloud, light-bulb idea, frenzied dance, and rain |
| `GJNAT3.TTM` | Native Boat Visit | Boat arrives, Johnny is undressed, and boat leaves |
| `GJVIS3.TTM` | Submarine and Aircraft Visitors | Periscope, plane, helicopter, and Johnny shrugging |
| `GJVIS5.TTM` | Johnny Jumps at a Plane | Jumping Johnny, collision, and plane starting |
| `GJVIS5W.TTM` | Plane Jump — Short Version | Load visitor 5, jumping Johnny, and collision |
| `GJVIS6.TTM` | Tanker Visit | Johnny watches and waves as the tanker arrives |
| `MEANWHIL.TTM` | Quarky Watch | Quarky watch |
| `MJAMBWLK.TTM` | Ambient Walking | Ambient walk, foot, look, and standing sequences at island spots |
| `MJBATH.TTM` | Ocean Bath and Stolen Clothes | Bathing, hair washing, and a gull taking Johnny's clothes |
| `MJCOCO.TTM` | Chasing the Coconut | Shake tree, falling coconut, bounce, chase, and smash |
| `MJCOCO1.TTM` | Eating the Coconut | Chase, break, eat, chew, and big sigh |
| `MJDIVE.TTM` | Johnny Goes Diving | Dive, bubbles, and walking out of the water |
| `MJFIRE.TTM` | Building a Campfire | Rubbing sticks, growing fire, cooking, eating, and dying embers |
| `MJFISH.TTM` | Fishing by the Tree | Cast, reel, catches, crab, boot, octopus, and tree sequences |
| `MJFISHC.TTM` | Fishing from the Shore | Casting, reeling, catches, starfish, crab, boot, and large fish |
| `MJJOG.TTM` | Johnny Goes Jogging | Stretching, running, out of breath, and the last leg |
| `MJRAFT.TTM` | Building the Raft | Getting boards, building, standing, and dusting off hands |
| `MJREAD.TTM` | Reading with the Seagull | Reading, page turns, sleep, coconut bump, and gull stealing the book |
| `MJSAND.TTM` | Building a Sandcastle | Castle construction, kicking, Lilliputians, planes, and King Kong routine |
| `MJTELE.TTM` | Looking Through the Telescope | Lift, scan left/right, shifting eye, and lower telescope |
| `SASKDATE.TTM` | Johnny Asks Mary on a Date | Mary approaches, Johnny asks, Mary accepts, and they wave goodbye |
| `SBREAKUP.TTM` | Showing Mary the Raft | Johnny builds, Mary arrives, Johnny shows the raft, and breakup begins |
| `SHARK1.TTM` | Here Comes the Shark | Water check and shark arrival |
| `SJGLIMPS.TTM` | Johnny's First Glimpse of Mary | Johnny fishing while Mary swims and dives |
| `SJLEAVES.TTM` | Johnny Leaves the Island | Raft ready, Johnny gets his bags, and leaves |
| `SJMSSGE.TTM` | Message in a Bottle | Johnny writes a letter, bottles it, throws it, and dreams |
| `SJMSUZY.TTM` | Johnny Meets Suzy | Suzy meets Johnny |
| `SJWORK.TTM` | Johnny at Work | Johnny in the office remembers the island |
| `SMDATE.TTM` | Johnny and Mary's Date | Dancing, eating, toast, drinks, and waving |
| `SUZYCITY.TTM` | Suzy's Message | Tanning oil, floating bottle, first message, and thoughts of the island |
| `THEEND.TTM` | Back on the Island | Plane drops Johnny, Johnny dances, returns to the island, and credits |
| `WOULDBE.TTM` | The Would-Be Rescuers | Boat passes, returns, Johnny swims out, and they leave for good |

The clearest existing names that do not need an alternate label are
`FISHWALK.TTM`, `MJRAFT.TTM`, `MJJOG.TTM`, `SJMSSGE.TTM`, and `THEEND.TTM`;
they remain in the table only so the full 41-resource review is auditable.

## Future suggestions and known limitations

- Night fidelity remains observationally unverified because the sampled
  11-hour recording sections are all daytime.
- `FLAME.BMP` and `FLURRY.BMP` remain unavailable; their effects are safely
  omitted.
- Automatic CRT performance defaults remain blocked on physical GPU and
  ultrawide measurements.
- macOS, Linux, WebAssembly, and simultaneous multi-monitor playback remain
  deferred until they have dedicated implementation and QA plans.

## Reference status

- The direct Git upstream and timing baseline is
  [deckarep/Johnny-Castaway-2026-Public](https://github.com/deckarep/Johnny-Castaway-2026-Public).
- The C implementation at
  [jno6809/jc_reborn](https://github.com/jno6809/jc_reborn) remains the formula
  and engine-behavior reference.
- [tallPete/JohnnyCastaway](https://github.com/tallPete/JohnnyCastaway) was
  reviewed for slot limits, clipping, walking, cloud placement, stability,
  persistence, and rendering regressions.
- [castaway.xesf.net](https://castaway.xesf.net/) is useful for palette and
  composition checks but is not a full-story behavior reference.
- [alexbevi/xbak](https://github.com/alexbevi/xbak) is retained only as a
  historical resource/parser reference; no source or assets were copied.
- Continue observation against the
  [11-hour recording](https://www.youtube.com/watch?v=l8D6qppreiI), without
  downloading or bundling it.

Implementation and comparison details are recorded in
[`docs/FIDELITY_AUDIT.md`](docs/FIDELITY_AUDIT.md), while longer-term platform
and display decisions remain in [`ROADMAP.md`](ROADMAP.md).

## Everything fixed and completed

- [x] Replaced raw `T` tag skipping with ADS-driven **Next event** playback for
  selected scene collections. Complete original events retain ADS order,
  timing, companion threads, cross-TTM composition, and sprite state; hidden
  `Ctrl+T` remains available for raw-tag debugging.
- [x] Kept story-driven TTMs aligned with randomized and left-island placement,
  while resetting directly selected TTMs to the fixed origin used by their new
  background. This prevents a selected scene from inheriting the previous
  island coordinates; added normal, left-island, and standalone regression tests.
- [x] Added `scrantic/Johnny-Castaway-Original-Data.sfv` with the verified
  CRC-32 values for both canonical archives. Git tracks only the SFV in that
  folder and continues to ignore all original game data.
- [x] Added the **Choose scene collection** selector with all 41 friendly titles,
  exact TTM filenames, and smaller embedded-metadata descriptions. Kept the
  catalog in source instead of the INI, displayed the active portable or
  LocalAppData INI path, and added separate PNG `Content` and `Content Label`
  metadata.
- [x] Created the focused `agent/fidelity-ui-rc2` branch and committed the
  fidelity, configuration, interface, test, and documentation work.
- [x] Migrated to a Windows 11-only native x64 codebase. One MinGW64/amd64
  toolchain now builds and tests both `JohnnyCastaway.exe` and
  `JohnnyCastaway.scr`; legacy 32-bit build code and CI were removed.
- [x] Verified native x64 `/c`, `/s`, `/p`, and preview-host shutdown behavior,
  and completed the final local renderer, configuration, integration, QA, and
  source/data-boundary review.
- [x] Added a clickable `github.com/DrWize/JohnnyCx64` link to Settings.
- [x] Added a sleek responsive shortcut dock containing the primary playback
  and display controls.
- [x] Auto-hide nonessential shortcut and performance information after eight
  visible seconds plus a two-second fade; mouse or keyboard activity restores
  it immediately without hiding dialogs or focused controls.
- [x] Added `Space` scene pause/resume outside overlays. Animation timers,
  walking, waves, clouds, and sounds resume without consuming paused time;
  Runtime Log retains Space for trace-capture pause.
- [x] Stabilized Johnny's walking with bounded frame indexes, contiguous source
  order, and a reset island-relative offset on every frame.
- [x] Fixed palm-tree occlusion so Johnny passes behind a stationary trunk and
  leaves without moving the tree.
- [x] Completed the 41-TTM stability sweep: 2,460 forced scene advances, all 41
  resources switched and wrapped, and a healthy Full Story run.
- [x] Added `F12` PNG capture to `Pictures\Johnny Castaway` with embedded text
  metadata describing the active filter, sharpness, scaling, aspect, scene
  order, content, sky, holiday, window, resolution, and audio settings.
- [x] Moved persistent preferences to one installer-owned
  `JohnnyCastaway.ini` shared by the co-located EXE and SCR.
- [x] Added persistent scene order, scaling, CRT, window, audio, aspect,
  monitor, and data-directory settings with explicit command-line precedence.
- [x] Added the `F10` Data Files manager with native folder browsing, canonical
  archive verification, persistence, Explorer access, first-run recovery, and
  keyboard and mouse controls.
- [x] Made `scrantic` the verified default data-folder convention while keeping
  explicit `--data-dir` and a saved Data Files selection higher priority.
- [x] Restored moving clouds with one-time sprite loading, reference-correct
  vertical placement, 1–2 pixel movement, wrapping, and regression coverage.
- [x] Added matching Settings `Sky` and `D` keyboard cycles for Day, Night, and
  Automatic while preserving `N` for Next TTM.
- [x] Expanded and reflowed the F1 Settings panel to prevent title, description,
  path, and help-text overlap; made the lower shortcut dock show the live
  `Sky Auto`, `Sky Day`, or `Sky Night` state.
- [x] Fixed automatic day/night state so daytime clears a previous night state;
  automatic mode uses day from 06:00 through 17:59.
- [x] Added clip-zone enforcement and regression tests for drawing operations.
- [x] Added capability-aware CRT fallback, Fast CRT sharpness presets, the `F8`
  performance display, and the `F9` comparison benchmark.
- [x] Added the custom HDR Pop shader for large HDR-capable panels, with local
  clarity, restrained saturation and highlight enhancement, persistence,
  capability fallback, performance reporting, and benchmark integration.
- [x] Added screensaver-safe Settings and Runtime Log interaction, ordinary
  input exit behavior, and `F` window/fullscreen switching while preserving
  Runtime Log `Ctrl+F` search.
- [x] Kept one Raylib window and graphics context while switching between Full
  Story and individual TTMs, avoiding desktop flashes and repeated startup.
- [x] Added automated resource, parser, renderer, configuration, story,
  navigation, screenshot, UI activity, and Windows command-line coverage.
- [x] Kept generated binaries, history, logs, local profiles, screenshots, all
  user-supplied `scrantic` content, and Sierra/Dynamix data out of the source
  repository; only the checksum-only SFV is tracked.
- [x] Decided not to create replacement `FLAME.BMP` or `FLURRY.BMP` artwork for
  stable. Any future optional replacement must document authorship and license
  and must not be represented as recovered Sierra/Dynamix data.
