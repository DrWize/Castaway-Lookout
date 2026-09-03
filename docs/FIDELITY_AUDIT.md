# Fidelity and implementation audit

Updated: 2026-09-01

This audit compares observable behavior and defensive implementation choices. It
does not copy or redistribute game data, artwork, recordings, or source from the
reviewed projects.

## First regression pass

Primary references:

- [tallPete/JohnnyCastaway](https://github.com/tallPete/JohnnyCastaway), a
  GPL-3.0-or-later Swift/Metal implementation with documented multi-hour and
  rendering fixes.
- [jno6809/jc_reborn](https://github.com/jno6809/jc_reborn), the direct C engine
  reference used to verify formulas and slot limits.
- [alexbevi/xbak](https://github.com/alexbevi/xbak), a historical GPLv3 parser
  reference for Dynamix-era resource formats.

| Area | Go engine result | Action |
| --- | --- | --- |
| TTM slot count | `MaxTTMSlots` is 10, matching the C reference. | Confirmed; no change. |
| Clip zones | Opcode `SET_CLIP_ZONE` was decoded, but `grSetClipZone` did not constrain drawing. | Added per-layer scissor state for pixels, lines, rectangles, circles, sprites, and buffer copies. Full-layer clear intentionally ignores the active clip. |
| Walk to Spot.A / same spot | `walkInit` already enters the final turn when start and destination match. | Added a state regression test. |
| Walking/tree stability | Walking inherited mutable global draw offsets and advanced routes/frames with unsafe pointer arithmetic. That could shift Johnny and the foreground palm-tree copy after unrelated drawing. | Reset the island-relative offset every walking frame, replace pointer arithmetic with bounds-checked indexes, verify every route and contiguous frame sequence, and lock occlusion order to Johnny, trunk, then leaves. The 41-TTM/2,460-advance sweep remains green. |
| Cloud Y placement | The Go translation used `rand() % (100-height) + 25`; the C reference uses `rand() % (135-height)`. | Corrected all three cloud sizes and added limit tests. |
| Cloud motion | Cloud state and scheduling existed, but `islandAnimateClouds` returned unconditionally. Consecutive frames in the 11-hour recording show horizontal drift. | Re-enabled motion and wrapping; load `BACKGRND.BMP` once per island instead of once per animation tick. |
| Day/night selection | Full Story selected `NIGHT.SCR` before 06:00 and from 18:00, but never reset a previous night state during daytime and offered no clock-independent preview. | Always assign the clock-derived state, add boundary tests, and add a Settings-panel Day/Night/Automatic preview. |
| Scene-start flicker | No deterministic framebuffer assertion exists. | Keep open for representative-scene capture or image-hash testing. |
| Intro resource failure | Missing `INTRO.SCR` terminates through the explicit `scr resource: INTRO.SCR not found` path. | Added a focused failure-injection regression test. |
| Settings persistence | Config parsing, CLI precedence, and screensaver isolation have regression coverage. | Confirmed for the Windows process model. |
| Multi-monitor audio | The Windows port runs one selected-monitor instance under a single-instance lock, unlike the macOS per-display host model. | Duplicate per-monitor audio is not applicable; true simultaneous multi-monitor playback is not supported. |

## Johnny Castaway Enhanced comparison

[`fbreve/Johnny-Castaway-Enhanced`](https://github.com/fbreve/Johnny-Castaway-Enhanced)
was inspected at commit
[`ad676571eb1bba210a74aa205fe0a760fc13a672`](https://github.com/fbreve/Johnny-Castaway-Enhanced/commit/ad676571eb1bba210a74aa205fe0a760fc13a672).
It is useful supporting evidence for the ESP32 fidelity work, but it is not a
canonical behavior source. Final resource and playback semantics must still be
verified against the desktop decoder, deterministic fixtures, QEMU, and the
physical panel.

The transferable findings are:

- `TIMER 0x2022` selects a uniform random duration from its inclusive minimum
  and maximum rather than their fixed average;
- TTM `0x1101`/`0x1111` transitions require separate original/root and current
  tags so ADS stop, repeat, running, and completion ownership remain stable;
- local and global `IF_LASTPLAYED` completion chains are part of full ADS event
  playback;
- clip zones and the pixel, rectangle, circle, save-zone, and restore-zone
  commands need complete renderer integration;
- persistent artwork needs saved-zone composition that survives background and
  wave updates;
- plane/tree and WALKSTUF/WOULDBE scenes need explicit identity- and
  direction-aware layer ordering rather than transient thread-index ordering;
- non-island transitions must tear down island, cloud, wave, holiday, and
  retained-layer state cleanly.

Limitations matter: no automated Go tests or declared repository license were
found at the inspected commit, saved-zone restore behavior remains incomplete,
and several fixes use hard-coded scene exceptions. Do not copy, port, or
redistribute source from this repository without explicit licensing clarity;
use its behavior only to form independently validated regression cases.

The ESP32 friendly scene names are separate work. They are generated from the
audited `johnny-castaway.com` catalog and canonical ADS identities, not from
Johnny Castaway Enhanced.

## xBaK parser comparison

xBaK uses fixed-size buffers with guarded copy, seek, and skip operations. It is
useful historical context, but it targets a broader Betrayal at Krondor resource
stack rather than this exact Johnny Castaway archive contract. JohnnyCx64 already
adds constraints that are more directly useful here:

- only the two verified archive hashes are accepted;
- map offsets are checked before slicing;
- declared decompressed output is capped at 64 MiB;
- RLE literal and run lengths are checked against input and output bounds;
- LZW input/output exhaustion produces explicit corruption failures;
- every resource in the verified archives is decompressed by the regression
  suite.

No xBaK source or data is imported.

## Remaining observation work

- Continue systematic transition sampling against the documented 11-hour
  recording without downloading or bundling it. Direct observations at the
  beginning and at roughly 1, 5, 10, and 11 hours confirmed the 640x480 composition,
  16-color/dithered palette, cloud band, staged raft, holiday overlay, and
  continuous island-scene presentation. Consecutive 11-hour frames also confirm
  horizontal cloud drift. Every sampled point was daytime, so this recording is
  not evidence for the night palette or an automatic day/night transition.
  Those samples are useful visual checks, but they do not prove the complete
  random ordering or every transition.
- Add a deterministic scene-transition/framebuffer test if the renderer can
  expose a stable capture path without making tests depend on a physical GPU.

## Browser implementation comparison

The live [castaway.xesf.net](https://castaway.xesf.net/) viewer was inspected at
its native 640x480 canvas and compared with its MIT-licensed
[`xesf/castaway`](https://github.com/xesf/castaway) source. It loads the original
resource archives and renders representative ADS scenes, so it remains useful
for palette, resource-decoding, and basic composition checks. It is not a
full-story behavior oracle: its current `Story.play()` chooses one random scene,
and its own roadmap still lists full-story sequencing, moving clouds, waves,
day/night, and tides as future work. No JohnnyCx64 behavior should be changed
solely to match an omission in that viewer.
