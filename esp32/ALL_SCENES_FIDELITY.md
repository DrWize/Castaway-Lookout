# ESP32 All-Scenes Fidelity Worklist

This document is the ordered engineering record for making every entry in the
63-scene ESP32 catalog render, complete and restart correctly. `SCENE_REVIEW.md`
remains the authoritative physical-review ledger; this file records the defects,
repairs and remaining engine-wide work discovered by that review.

An item is complete only when its deterministic fixtures pass and the affected
scene is accepted on the physical Waveshare panel. Build, QEMU, flash and serial
results are supporting evidence and never replace direct visual acceptance.

## Ordered findings and required changes

1. [x] **Use the canonical data unchanged.** Read `RESOURCE.MAP` and
   `RESOURCE.001` through bounded ESP32 loaders, reject non-canonical source
   hashes, and keep `jcdata` byte-identical across normal and REVIEW-only builds.
2. [x] **Drive review from the complete catalog.** Compile all 63 ordered ADS
   identities, friendly titles and eligibility metadata into the firmware so no
   scene is silently omitted from the physical pass.
3. [x] **Persist review decisions safely.** Store mutually exclusive OK/REVIEW
   masks, schema version, catalog fingerprint and recoverable failure codes in
   NVS; commit each decision before advancing and resume at the next undecided
   scene after reset.
4. [x] **Synchronize real panel buffers.** Compose off-screen, wait for the RGB
   frame-complete signal before reusing a scanout buffer, then switch to the
   completed inactive buffer. This removed repeat-run split figures and flashing.
5. [x] **Own bitmap slots per TTM resource.** Do not share mutable BMP slots
   between concurrent `GJDIVE.TTM`, `MJDIVE.TTM` or other threads; one resource
   loading `JOHNWALK.BMP` must not replace sprites still used by another.
6. [x] **Retain the authored final frame.** A TTM layer that reaches `PURGE` or
   completion must remain composited until its final timer expires. This restored
   contextual endings and removed premature island/background returns.
7. [x] **Preserve authored timing.** Reload delays after every `UPDATE`, carry
   real elapsed centiseconds into the 30 fps presentation loop and avoid arbitrary
   scene-specific slowdowns. The fast rowing event is accepted as authored.
8. [x] **Keep enough ADS played-state history.** Retain completed records needed
   by `IF_NOT_PLAYED` and allow the 28-scene fishing chain to finish without
   `ESP_ERR_NO_MEM`; the bounded capacity is 64 scene records.
9. [x] **Render retained line geometry.** Store and composite palette-coloured
   TTM `DRAW_LINE` commands so the pirate ship and tied rope remain complete.
10. [x] **Apply stored-area replacement semantics.** Translate `STORE_AREA` and
    stored `DRAW_SCREEN` coordinates by the active island offset, clear replaced
    destinations correctly and preserve saved pixels during transparent copies.
    This repaired the pirate ship, sail and lower-left sandcastle sections.
11. [x] **Bound negative `ADD_SCENE` lifetimes.** End negatively referenced
    child scenes at the correct parent lifetime so `COOK AND EAT` and
    `NIGHT LILLIPUTIANS` do not leave duplicate actors or run forever.
12. [x] **Separate island placement from TTM origin.** Keep the background at its
    authored location while applying the desktop-compatible TTM offset for
    `LEFT_ISLAND` scenes. This aligned the three shore-fishing scenes without
    increasing the 32-draw bound.
13. [x] **Load every authored SCR through one runtime path.** Execute repeated
    `LOAD_SCREEN 0xF01F` commands in stream order, support all ten canonical SCR
    resources, release temporary decoded screens and clear the stored overlay on
    replacement.
14. [x] **Implement the complete island background stack.** Recreate ocean/night,
    island, tree, shadow, shore, rock, tide, raft, waves, clouds and holiday
    layers with deterministic event state and clean non-island teardown.
15. [x] **Keep island and ADS random seeds independent.** Derive repeatable
    background state from ADS identity and story day without changing authored
    ADS selection. Restart and Back 10 Frames must restore the event snapshot.
16. [x] **Resolve canonical cross-resource dependencies.** Execute the authored
    `GJVIS5.TTM` slot-1 dependency instead of skipping a missing bitmap, while
    retaining bounded resource ownership and cleanup.
17. [x] **Add geometry and replacement behavior exposed by the final scenes.**
    Support signed clip/store coordinates, `DRAW_CIRCLE` and bounded
    `DRAW_GETPUT` replacement, and retain the intended layer order and final
    frames for Mermaid and Tanker playback.
18. [x] **Correct fixed-left visitor metadata.** For `COCONUT HITS PLANE`, place
    the island at `x=-272`, retain TTM origin `x=0`, and keep the authored
    `GJVIS5` tag-8 entry sprite off-screen. This removed the frame-2 artifact and
    kept the plane visible through the strike.
19. [x] **Close the completed physical review record.** Hard-reset the current
    REVIEW-only firmware, capture serial proof of `63 OK / 0 REVIEW` and
    `ALL RESOLVED`, export the final identity table, and reconcile
    `SCENE_REVIEW.md` with the device state. Completed on 2026-09-03: COM4 was
    re-identified as ESP32-S3 rev 0.2 N16R8; serial restored
    `ok=63 review=0 complete=1`, matched fingerprint `eb04eb66746ae074`, emitted
    the empty final shortlist and reported `REVIEW-ONLY: ALL RESOLVED` without
    watchdog, panic, memory/draw-limit, missing-bitmap or restart-loop errors.
20. [ ] **Prove panel performance after background batching.** Re-run hashes,
    flash and hard-reset the candidate, then directly confirm smooth playback.
    Rendering only the final wave/cloud state per presentation pass must retain
    every centisecond engine tick and exact rewind behavior.
21. [x] **Complete deterministic island/SCR coverage.** Exact Back 10 Frames
    replay now compares MARY tag 2 at frame 346 and VISITOR tag 3 at frame 201
    against fresh deterministic replays. The accelerated rendered-event fixture
    advances waves/clouds, rebuilds the island background once per presentation
    and locks the final/checkpoint RGB565 hashes for representative normal,
    left-island, fixed-left visitor and non-island events. Existing host gates
    cover all ten canonical SCR dimensions/decoded hashes, short-screen bottom
    padding and Right-layout framebuffers. On 2026-09-03, 23 Python tests,
    uncached Go tests, normal and REVIEW-only builds, and strict headless QEMU
    passed these gates. Graphical and physical evidence remain items 30-31.
22. [ ] **Implement full `TIMER 0x2022` semantics.** Choose a uniform inclusive
    duration between authored minimum and maximum values, use a non-constant live
    seed, and retain injectable deterministic seeds for tests.
23. [ ] **Separate ADS root and current tags.** Preserve the original/root tag
    for ownership, stop, running and completion checks while `0x1101`/`0x1111`
    update only the current TTM tag. Cover MARY tag 3 and plane ghosting.
24. [ ] **Finish clip and primitive drawing parity.** Wire
    `SET_CLIP_ZONE 0x4004` through every runtime draw, add
    `DRAW_RECT 0xA104` and `DRAW_PIXEL 0xA002`, and extend the already-bounded
    `DRAW_CIRCLE 0xA404` path to full clipping and opcode parity.
25. [ ] **Finish scoped saved-zone parity.** Implement bounded
    `SAVE_ZONE 0xA054` and `RESTORE_ZONE 0xA064` for sprites, rectangles, lines, clears and
    repeated restores based on canonical data and desktop behavior.
26. [ ] **Implement local and global `IF_LASTPLAYED`.** Match root tags and
    preserve correct repeat, stop, chaining and completion behavior.
27. [ ] **Make z-order explicit and stable.** Identify layers by ADS/resource,
    root tag and direction—not transient thread index—and cover plane/tree plus
    WALKSTUF/WOULDBE Johnny-on-top cases.
28. [ ] **Finish the full story lifecycle.** Complete story selection, walking
    and pathfinding, island entry/exit, clouds, waves, day/night transitions,
    non-island teardown and safe resource reuse across long full-story runs.
    The background assets exist; this item closes their full-story transitions
    and ownership rather than recreating the renderer from item 14.
29. [ ] **Add the persisted Special Days selector.** Provide Off, Automatic,
    Halloween, St. Patrick, Christmas and New Year. Apply `special_days.csv`,
    year-wrap and `HOLIDAY_NOK` suppression without changing the saved choice;
    keep Easter checklist-only until canonical assets and rules exist.
30. [ ] **Run the complete automated release gate.** Pass Python and uncached Go
    tests, normal and REVIEW-only ESP-IDF builds, strict headless and graphical
    QEMU, boot fixtures, framebuffer captures, unchanged-`jcdata` verification,
    heap/draw bounds and absence of watchdog, panic or restart loops.
31. [ ] **Run the final physical acceptance gate.** Re-identify the ESP32-S3 and
    serial port, verify N16R8, flash and hard-reset, capture a clean serial boot,
    then inspect every SCR class, representative island/tide/raft/cloud/wave and
    holiday state, transitions, Pause/Play, exact Back 10 Frames, touch controls,
    long-run completion and stable 30 fps presentation.
32. [ ] **Only after fidelity passes, add diagnostic polish.** Consider iris
    transitions, single-frame/max-speed controls and 1/4/8-layer performance
    benchmarks without weakening deterministic or physical acceptance gates.

## Completion definition

All-scenes fidelity is complete when items 19-31 are checked, the physical
ledger and NVS both report 63 OK and zero REVIEW/unreviewed, all deterministic
and hardware gates pass, and a long full-story run completes without visual
artifacts, resource leaks, watchdogs, panics or unintended restart loops.

Touch settings, Wi-Fi/SNTP, weather, Soft CRT and the local web interface remain
separate later phases. They must reuse the accepted engine and must not be used
to defer or weaken the all-scenes fidelity gates above.
