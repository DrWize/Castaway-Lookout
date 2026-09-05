# ESP32 63-Scene Review Ledger

This file is the authoritative human record for the physical review of the
ordered 63-entry ESP32 story catalog. Device-side NVS protects work in progress;
the final serial `REVIEW_EXPORT_BEGIN` / `REVIEW_EXPORT_END` block supplies the
identities copied here after its totals match the panel summary.

## Final schema-2 closure — 2026-09-03

| Field | Value |
| --- | --- |
| Status | Complete: all 63 scenes physically accepted and serial-confirmed OK |
| Firmware/build | `johnny_esp32.bin` `0x5ee20` bytes, SHA-256 `55f791db3a3a4af7962e9e4a9eb89202d87b09375eb02f36c3a63bdd6ede6f2b` |
| Board | Waveshare ESP32-S3-Touch-LCD-7, ESP32-S3 rev 0.2, N16R8, COM4 |
| Catalog fingerprint | `eb04eb66746ae074` |
| Review schema | 2 |
| Final serial-confirmed state | 63 OK / 0 REVIEW / 0 unreviewed; `ALL RESOLVED` |

The user physically accepted global Scene 36 (`LEAVE THE ISLAND`) and advanced
to the final three retained scenes. The app-only flash preserved NVS and the
unchanged `jcdata` partition. Mermaid and Tanker were accepted first. Serial
then restored `ok=62 review=1 complete=1` and opened Coconut Plane as
`SCENE 1/1`. The user physically accepted the corrected final scene.

On the final hard reset, the identified COM4 ESP32-S3 restored
`ok=63 review=0 complete=1` with catalog fingerprint `eb04eb66746ae074`,
emitted an empty schema-2 shortlist export and reported
`REVIEW-ONLY: ALL RESOLVED`. The boot confirmed 16 MB flash, 8 MB PSRAM,
canonical `jcdata`, all embedded scene/runtime gates and all 63 catalog starts
without watchdog, panic, memory/draw-limit, missing-bitmap or restart-loop
errors.

| Order | Scene | Title | ADS | Tag | Latest physical finding / acceptance target |
| ---: | ---: | --- | --- | ---: | --- |
| Resolved | 36 | LEAVE THE ISLAND | MARY.ADS | 5 | Looks OK; physically accepted on 2026-09-03. |
| Resolved | 34 | FIRST MERMAID GLIMPSE | MARY.ADS | 2 | Physically accepted on 2026-09-03: the scene looks correct; the apparent issue was Johnny dreaming, not a detached fish. |
| Resolved | 56 | TANKER VISIT | VISITOR.ADS | 3 | Physically accepted on 2026-09-03: the hull is complete and the tanker traversal works. |
| Resolved | 60 | COCONUT HITS PLANE | VISITOR.ADS | 5 | Physically accepted on 2026-09-03: fixed-left placement removes the frame-2 artifact, keeps the plane in frame and shows the complete coconut strike. |

The combined candidate implements `DRAW_CIRCLE`, signed clip/store coordinates,
`DRAW_GETPUT` replacement, authored layer ordering and final-frame retention,
and the canonical GJVIS5 slot-1 dependency. Deterministic fixtures lock Mermaid
frame 356, Tanker frame 211, all three final framebuffers, completion hashes,
retained-draw peaks, store-area count and bitmap ownership. Host tests, uncached
Go tests, normal and REVIEW-only builds, QEMU, verified app flash, and physical
serial boot gates pass without memory, draw-limit, missing-slot, watchdog,
panic or restart-loop errors. These checks do not replace direct panel review.

The schema-2 physical review and serial closure gate are complete. Future
engine-wide changes must preserve these identities and reuse the recorded frame
locations as regression checkpoints.

## Prior SCR/island schema-2 review run

| Field | Value |
| --- | --- |
| Status | Device restored 59 OK / 4 REVIEW; Scene 34 failed at the draw bound and advanced to Scene 36 |
| Date completed | In progress; last review 2026-09-02 |
| Firmware/build | Left-island alignment REVIEW-only `johnny_esp32.bin` `0x5dc90` bytes, SHA-256 `d4ec4cf4a8d660509bc4c74d5226cb17b4fe8cefccc93a7e4308e525b0e4abac`; flashed 2026-09-02 |
| Board | Waveshare ESP32-S3-Touch-LCD-7, ESP32-S3 rev 0.2, N16R8, COM4 at flash time |
| Catalog fingerprint | `eb04eb66746ae074` |
| Review schema | 2 |
| OK count | 59 |
| Needs Review count | 4 |
| Unreviewed count | 0 |

The schema-2 normal firmware restored and exported all 63 persistent decisions
with empty automatic-failure fields. The freshly rebuilt REVIEW-only firmware
then restored the same `ok=48 review=15 unreviewed=0` state, passed its embedded
SCR/island, ADS, catalog and shortlist gates, and opened global Scene 12 as
shortlist position `1/15`. Firmware and unchanged `jcdata` writes were
hash-verified. The serial catalog probe still reports a missing
`GJVIS5.TTM` bitmap slot 1; the full catalog start gate nevertheless passes.

Scene 1's stored-area repair translates authored TTM `STORE_AREA` and
`DRAW_SCREEN` rectangles by the active island offset. `STORE_AREA` replacement
clears its destination before copying. The user physically accepted frames 320
and 681; serial subsequently
confirmed NVS restored `49 OK / 14 REVIEW` and removed Scene 1.

The first Scene 2 clip-retention candidate still showed the missing lower-left
sandcastle section and was rejected physically. The revised candidate preserves
saved pixels when a transparent `DRAW_SCREEN` follows a cleared composition
layer, matching the desktop renderer. Build, verified app flash and serial boot
gates passed; QEMU was skipped at the user's request. The user physically
accepted the complete lower-left sandcastle around frame 195 on 2026-09-01;
device-side OK persistence was confirmed after reset on 2026-09-02.

Scene 2 subsequently restored as OK after reset, giving `50 OK / 13 REVIEW`.
Scene 3 (`COOK AND EAT`) was physically accepted after the bounded negative-
`ADD_SCENE` lifetime repair removed the duplicate Johnny and allowed the event
to finish. Serial observed two clean 66-completion runs and then restored
`51 OK / 12 REVIEW`. Scene 4 (`NIGHT LILLIPUTIANS`) also finished and restarted
cleanly twice; the reviewer marked it OK on the panel, advancing to Scene 5.
Scene 5 initially failed automatically with `TTM layer draw limit reached` /
`ESP_ERR_NO_MEM`. The repair now keeps a separate TTM origin and applies the
desktop engine's +272 X adjustment for `LEFT_ISLAND` events while leaving the
island background at its authored offset. With the original 32-draw bound,
Scene 5 completed and restarted three times, measured a peak of 6 retained
draws, and was physically accepted on 2026-09-02. A subsequent app-only reflash
preserved NVS and restored `59 OK / 4 REVIEW`, confirming that this and six
additional panel decisions were persisted.

After that reflash, global Scene 34 (`FIRST MERMAID GLIMPSE`) reached
`SJGLIMPS.TTM` tag 108 with 32 retained draws. The bounded runtime reported
`ESP_ERR_NO_MEM`, kept the scene REVIEW, and advanced automatically to global
Scene 36 (`LEAVE THE ISLAND`).

The following table retains the original physical observations from that
intermediate run. Its pending wording is historical and is superseded by the
final closure section above; every listed identity was later physically
accepted. Keep the frame locations as regression checkpoints.

| Position | Scene | Title | ADS | Tag | Physical observation |
| ---: | ---: | --- | --- | ---: | --- |
| 01 | 12 | PIRATES TIE JOHNNY | BUILDING.ADS | 4 | Resolved: the ship remains complete around frame 320 and no sail pieces remain around frame 681. Physically accepted and persisted OK on 2026-09-01. |
| 02 | 14 | LILLIPUTIAN BOAT | BUILDING.ADS | 2 | Resolved: transparent `DRAW_SCREEN` copies preserve the saved lower-left sandcastle section. Physically accepted around frame 195 and subsequently persisted OK. |
| 03 | 16 | COOK AND EAT | BUILDING.ADS | 7 | Resolved: bounded negative-`ADD_SCENE` lifetime handling removes the duplicate Johnny and ends the event after 66 completions. Physically accepted and persisted OK on 2026-09-02. |
| 04 | 17 | NIGHT LILLIPUTIANS | BUILDING.ADS | 6 | Resolved: event completed and restarted cleanly twice; physically marked and persisted OK on 2026-09-02. |
| 05 | 21 | FISHING CATCH ONE | FISHING.ADS | 4 | Resolved: the desktop-compatible left-island TTM origin aligns Johnny with the island. Completed and restarted repeatedly under the original 32-draw bound with a measured peak of 6; physically accepted on 2026-09-02, panel OK persistence pending. |
| 06 | 24 | SHORE FISHING JUNK | FISHING.ADS | 7 | Prior physical pass confirmed Johnny left of the right-positioned island. The shared left-island origin repair is flashed; direct panel re-review remains required. |
| 07 | 25 | SHORE FISH KEEPERS | FISHING.ADS | 8 | Prior pass showed Johnny and the island misaligned. The shared left-island origin repair is flashed; direct panel re-review remains required. |
| 08 | 32 | DINNER WITH MERMAID | MARY.ADS | 1 | Table disappears around frame 223; the record player also disappears when dancing starts around frame 858. |
| 09 | 34 | FIRST MERMAID GLIMPSE | MARY.ADS | 2 | Fish was previously misaligned in the tree around frame 383. On 2026-09-02 the reflashed runtime failed earlier at `SJGLIMPS.TTM` tag 108 with the 32-draw bound, retained REVIEW, and advanced automatically. |
| 10 | 35 | MERMAID BREAKUP | MARY.ADS | 4 | Two Johnnys appear around frame 129: one builds the boat while the other talks to the mermaid. |
| 11 | 36 | LEAVE THE ISLAND | MARY.ADS | 5 | Resolved: the user reported that it looks OK and physically accepted it on 2026-09-03. |
| 12 | 56 | TANKER VISIT | VISITOR.ADS | 3 | Around frame 232 the tanker bow should be whole, but only its front is shown. |
| 13 | 57 | COCONUT FALLS | VISITOR.ADS | 4 | Scene is incomplete; no coconut falls. |
| 14 | 59 | CHASE THE COCONUT | VISITOR.ADS | 7 | Scene is incomplete; no coconut falls. |
| 15 | 60 | COCONUT HITS PLANE | VISITOR.ADS | 5 | Johnny and the island are misaligned. |

All other catalog identities were physically classified OK in that schema-2
pass. The shortlist was subsequently repaired and accepted in full; only the
final reset/serial export subsequently passed as recorded above.

## Previous review run

| Field | Value |
| --- | --- |
| Status | First physical classification confirmed; REVIEW-only pass started |
| Date completed | 2026-08-31 |
| Firmware/build | `johnny_esp32.bin` `0x55ba0` bytes, SHA-256 `ea68c2ec261a3ca601ff7f06d9b9d23db0054809b1224b7e75fa396055d1e499`; flashed 2026-08-31 |
| Board | Waveshare ESP32-S3-Touch-LCD-7, ESP32-S3 rev 0.2, N16R8, COM4 at flash time |
| Catalog fingerprint | `eb04eb66746ae074` |
| OK count | 29 |
| Needs Review count | 34 |
| Unreviewed count | 0 |

The complete image and `jcdata` partition were hash-verified during flashing.
Physical serial then passed every boot fixture, loaded/started all 63 catalog
records, restored the compatible empty NVS review (`ok=0 review=0`), and entered
live `ACTIVITY.ADS` tag 1 as `SCENE 1/63`. A watchdog warning found during the
first boot was fixed by yielding between catalog probes; the final boot reached
live playback without a watchdog, panic or reboot.

The user completed the direct panel classification. Serial reported
`ok=29 review=34 unreviewed=0`; every exported failure field was empty, so none
of these classifications came from the automatic recoverable-error path. Build,
QEMU, flash and serial evidence remain supporting evidence rather than a
substitute for the user's visual decisions.

The ledger and totals were confirmed by the reviewer on 2026-08-31. The first
REVIEW-only pass starts at STAND EVENT 1 so the consecutive STAND identities can
be inspected together. STAND tags 1 and 2 are separate positional idle scenes;
the review runner advances between them only after REVIEW or Scene +1.

## OK

| Scene | Title | ADS | Tag | Observation |
| ---: | --- | --- | ---: | --- |
| 01 | ACTIVITY EVENT 1 | ACTIVITY.ADS | 1 | User classified OK |
| 02 | ACTIVITY EVENT 12 | ACTIVITY.ADS | 12 | User classified OK |
| 03 | ACTIVITY EVENT 11 | ACTIVITY.ADS | 11 | User classified OK |
| 04 | ACTIVITY EVENT 10 | ACTIVITY.ADS | 10 | User classified OK |
| 05 | ACTIVITY EVENT 4 | ACTIVITY.ADS | 4 | User classified OK |
| 06 | RAIN DANCE | ACTIVITY.ADS | 5 | User classified OK |
| 07 | READING GAG | ACTIVITY.ADS | 6 | User classified OK |
| 08 | ACTIVITY EVENT 7 | ACTIVITY.ADS | 7 | User classified OK |
| 09 | ACTIVITY EVENT 8 | ACTIVITY.ADS | 8 | User classified OK |
| 10 | NATIVE BOAT | ACTIVITY.ADS | 9 | User classified OK |
| 11 | BUILDING EVENT 1 | BUILDING.ADS | 1 | User classified OK |
| 12 | TIED DOWN | BUILDING.ADS | 4 | User classified OK |
| 13 | BUILDING EVENT 3 | BUILDING.ADS | 3 | User classified OK |
| 14 | LILLIPUTIAN BOAT | BUILDING.ADS | 2 | User classified OK |
| 17 | BUILDING EVENT 6 | BUILDING.ADS | 6 | User classified OK |
| 18 | FISHING EVENT 1 | FISHING.ADS | 1 | User classified OK |
| 19 | FISHING EVENT 2 | FISHING.ADS | 2 | User classified OK |
| 20 | FISHING EVENT 3 | FISHING.ADS | 3 | User classified OK |
| 22 | FISHING EVENT 5 | FISHING.ADS | 5 | User classified OK |
| 23 | FISHING EVENT 6 | FISHING.ADS | 6 | User classified OK |
| 28 | JOHNNY EVENT 3 | JOHNNY.ADS | 3 | User classified OK |
| 29 | JOHNNY EVENT 4 | JOHNNY.ADS | 4 | User classified OK |
| 37 | MISCGAG EVENT 1 | MISCGAG.ADS | 1 | User classified OK |
| 38 | MISCGAG EVENT 2 | MISCGAG.ADS | 2 | User classified OK |
| 51 | STAND EVENT 15 | STAND.ADS | 15 | User classified OK |
| 52 | STAND EVENT 16 | STAND.ADS | 16 | User classified OK |
| 55 | VISITOR EVENT 1 | VISITOR.ADS | 1 | User classified OK |
| 61 | WALKSTUF EVENT 1 | WALKSTUF.ADS | 1 | User classified OK |
| 63 | WALKSTUF EVENT 3 | WALKSTUF.ADS | 3 | User classified OK |

## Needs Review

| Scene | Title | ADS | Tag | Failure or observation |
| ---: | --- | --- | ---: | --- |
| 15 | BUILDING EVENT 5 | BUILDING.ADS | 5 | User classified REVIEW |
| 16 | BUILDING EVENT 7 | BUILDING.ADS | 7 | User classified REVIEW |
| 21 | FISHING EVENT 4 | FISHING.ADS | 4 | User classified REVIEW |
| 24 | FISHING EVENT 7 | FISHING.ADS | 7 | User classified REVIEW |
| 25 | FISHING EVENT 8 | FISHING.ADS | 8 | User classified REVIEW |
| 26 | JOHNNY EVENT 1 | JOHNNY.ADS | 1 | User classified REVIEW |
| 27 | JOHNNY EVENT 2 | JOHNNY.ADS | 2 | User classified REVIEW |
| 30 | JOHNNY EVENT 5 | JOHNNY.ADS | 5 | User classified REVIEW |
| 31 | JOHNNY EVENT 6 | JOHNNY.ADS | 6 | User classified REVIEW |
| 32 | MARY EVENT 1 | MARY.ADS | 1 | User classified REVIEW |
| 33 | MARY EVENT 3 | MARY.ADS | 3 | User classified REVIEW |
| 34 | MARY EVENT 2 | MARY.ADS | 2 | User classified REVIEW |
| 35 | MARY EVENT 4 | MARY.ADS | 4 | User classified REVIEW |
| 36 | MARY EVENT 5 | MARY.ADS | 5 | User classified REVIEW |
| 39 | STAND EVENT 1 | STAND.ADS | 1 | User classified REVIEW |
| 40 | STAND EVENT 2 | STAND.ADS | 2 | User classified REVIEW |
| 41 | STAND EVENT 3 | STAND.ADS | 3 | User classified REVIEW |
| 42 | STAND EVENT 4 | STAND.ADS | 4 | User classified REVIEW |
| 43 | STAND EVENT 5 | STAND.ADS | 5 | User classified REVIEW |
| 44 | STAND EVENT 6 | STAND.ADS | 6 | User classified REVIEW |
| 45 | STAND EVENT 7 | STAND.ADS | 7 | User classified REVIEW |
| 46 | STAND EVENT 8 | STAND.ADS | 8 | User classified REVIEW |
| 47 | STAND EVENT 9 | STAND.ADS | 9 | User classified REVIEW |
| 48 | STAND EVENT 10 | STAND.ADS | 10 | User classified REVIEW |
| 49 | STAND EVENT 11 | STAND.ADS | 11 | User classified REVIEW |
| 50 | STAND EVENT 12 | STAND.ADS | 12 | User classified REVIEW |
| 53 | SUZY EVENT 1 | SUZY.ADS | 1 | User classified REVIEW |
| 54 | SUZY EVENT 2 | SUZY.ADS | 2 | User classified REVIEW |
| 56 | VISITOR EVENT 3 | VISITOR.ADS | 3 | User classified REVIEW |
| 57 | VISITOR EVENT 4 | VISITOR.ADS | 4 | User classified REVIEW |
| 58 | VISITOR EVENT 6 | VISITOR.ADS | 6 | User classified REVIEW |
| 59 | VISITOR EVENT 7 | VISITOR.ADS | 7 | User classified REVIEW |
| 60 | VISITOR EVENT 5 | VISITOR.ADS | 5 | User classified REVIEW |
| 62 | WALKSTUF EVENT 2 | WALKSTUF.ADS | 2 | User classified REVIEW |

## Test procedure

1. Confirm the sidebar reads `SCENE 1/63` on a clean review or resumes at the
   first unreviewed scene after reset.
2. Exercise Pause/Play, Frame -10, Scene -1 and Scene +1 early in the run.
3. Mark every scene OK or REVIEW. Each decision is committed before advancing.
4. At completion, compare the panel OK/REVIEW/LEFT totals with the serial export.
5. Replace the placeholder tables above with the exported identities, grouped
   by result, and record any visual observations.
6. Check this ledger before enabling the separate REVIEW-only firmware build.

## REVIEW-only follow-up

The ledger is confirmed. The separate `build-review-only` configuration reads
the persisted REVIEW mask. Its
Scene controls must wrap only within that shortlist; changing a scene to OK
removes it, and an empty list stops on `ALL RESOLVED`.

## Web-controlled reviewer sidebar — 2026-09-04

- Default-Off physical gate: accepted by the user. The normal firmware shows a
  completely black reserved 160x480 sidebar.
- Still pending: turn the Reviewer sidebar On through the authenticated webpage,
  confirm the complete controls appear without changing the native 640x480
  scene, turn it Off again, and confirm the saved state after reboot. RC4's
  packaged flasher, board-identification, verified-write and hard-reset gates
  pass, but they do not replace this direct panel check.
