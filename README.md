# Johnny Castaway 2026

Johnny Castaway 2026 Edition is a Go/Raylib port developed from
[deckarep/Johnny-Castaway-2026-Public](https://github.com/deckarep/Johnny-Castaway-2026-Public),
which in turn ports [jc_reborn](https://github.com/jno6809/jc_reborn). The local
Git history includes deckarep's work through merge commit `ba5ae61`; this Windows
branch extends that baseline with stability, diagnostics, display, and packaging
work. Both upstream projects must retain clear attribution.

Download current builds and read release notes on the
[GitHub Releases page](https://github.com/DrWize/JohnnyCx64/releases).

> ☕ Enjoying Johnny Castaway 2026? If Johnny's island adventures bring back good
> memories or make you smile, you can
> [buy me a coffee](https://buymeacoffee.com/drwize). Your support helps me spend
> more time maintaining and improving this project—thank you!

## At a glance

| Area | Details |
| --- | --- |
| Platform | Windows 11 x64 |
| Outputs | `JohnnyCastaway.exe` and `JohnnyCastaway.scr` |
| Engine | Go with Raylib |
| Original data | Supplied locally by the user and verified by hash |
| Settings | Human-readable `JohnnyCastaway.ini` |

## Contents

* [How it works](#how-it-works)
* [Quick start](#quick-start)
* [Controls and settings](#controls-and-settings)
* [Display and performance](#display-and-performance)
* [Testing and diagnostics](#testing-and-diagnostics)
* [Data files and copyright](#data-files-and-copyright)
* [Project references](#project-references)
* [Project status](#project-status)
* [License](#license)

## How it works

The Windows application is written in Go and uses Raylib for graphics and
audio. Sierra/Dynamix archives and sounds are loaded from a user-selected local
folder and are never embedded in the executable or repository. See
[Data files and copyright](#data-files-and-copyright).
This project targets 64-bit Windows 11 only. macOS, Linux, WebAssembly, and
legacy 32-bit Windows were deliberately deferred or removed so release work
stays focused on one native x64 application and screensaver. See
[the roadmap evaluations](docs/ROADMAP_EVALUATIONS.md).

## Quick start

### Build for Windows 11 x64

Run `build\build.bat` from a Command Prompt to create
`build\JohnnyCastaway.exe` and `build\JohnnyCastaway.scr`, plus timestamped
copies under `build\history\`. Previous copies are retained so different builds
can be tested side by side. The build requires 64-bit Go in
`C:\Program Files\Go` and the 64-bit MSYS2 MinGW GCC toolchain in
`C:\msys64\mingw64`. The script generates amd64 Windows resource objects and
builds both files as native PE32+ GUI
programs. The application requires the user's original data folder at runtime.
Set `MSYS2_ROOT` before running the script when MSYS2 is installed
somewhere other than `C:\msys64`; the CI workflow uses this portable path.
The single Windows 11 CI job tests the project, verifies `GOARCH=amd64` on both
outputs, and uploads the `.exe` and `.scr` together.

### Screensaver modes

The `.scr` implements the Windows screensaver command contract:

* `/s` starts the full-screen screensaver.
* `/p HWND` and `/p:HWND` embed a child window in the supplied Screen Saver
  Settings preview host. Preview mode ignores normal input-to-exit behavior and
  closes when its host window disappears.
* `/c` and `/c:HWND` open Johnny Castaway's windowed settings panel. The optional
  owner handle is accepted for compatibility; configuration remains in the
  application's own Raylib window.

The standard long-form options can follow a screensaver mode for testing, such
as `/s --mute --data-dir C:\Johnny`. Installing or copying the `.scr` does not
install the original data. Unless `--data-dir` or a saved Data Files selection
states otherwise, the EXE and SCR look for a verified folder named `scrantic`
beside the binary, beside its containing project directory, or beside the
current working directory. This lets development builds find a nearby verified
`scrantic` directory automatically.

### Local data and build folders

Local development uses the following directories when applicable:

* `scrantic/` - default local folder for user-supplied archives and sounds. Git
  tracks only `Johnny-Castaway-Original-Data.sfv`; all game data remains ignored
* `build/` - build scripts, Windows manifest/version/icon resources, and output

### Command-line options

Windows command-line options:

* `--windowed` - run in a resizable window instead of borderless fullscreen
* `--fullscreen` - override a saved windowed preference
* `--monitor N` - select a monitor using a 1-based number
* `--screensaver` - use screensaver input behavior while retaining the documented
  F-key and playback controls
* `--mute` - disable audio
* `--sound` - override a saved muted preference
* `--stretch` - stretch 4:3 graphics instead of preserving their aspect ratio
* `--fit` - override a saved stretch preference and preserve 4:3
* `--ttm NAME.TTM` - play one TTM resource directly for comparison or testing
* `--menu` - open the otherwise hidden menu immediately (useful for testing)
* `--crt MODE` - override the display filter with `off`, `lightweight`, `fast`,
  `hdr`, or `lottes` (the option name remains for configuration compatibility)
* `--data-dir PATH` - folder containing the original `RESOURCE.MAP`,
  `RESOURCE.001`, and any optional `sound*.wav` files; this overrides the
  `scrantic` default and the selected path persists

## Controls and settings

### Everyday controls

Double-click `build\JohnnyCastaway.exe` to test the finished application.
Press `F1` for the settings and complete key guide. Press `Esc` twice within
1.5 seconds to quit; the first press also closes an open menu or runtime log.
The Settings panel shows a clickable link to the canonical
`github.com/DrWize/JohnnyCx64` repository.
Press `F` in the normal application to toggle between the centered resizable
window and borderless fullscreen; the selected mode persists. `Ctrl+F` remains
the Runtime Log search shortcut. Screensaver and preview modes do not toggle.
Press `F10` to open the Data Files manager. Inside it, use `Enter` or `B` to
browse, `R` to recheck the current folder, `O` to open it in Explorer, `F1` to
return to Settings, and `Esc` or `F10` to close. The manager verifies both
canonical archive hashes before saving a selection, so an invalid folder never
replaces the last working setting.
Press `F12` to save a timestamped PNG screenshot directly to
`Pictures\Johnny Castaway`. A confirmation containing the saved path appears
after the capture. Each PNG embeds searchable text metadata for the application
version, capture time, display filter and sharpness, image scaling, aspect mode,
scene order, content, sky and holiday modes, window mode, resolution, and mute
setting.
Press `D` to cycle island backgrounds through Day, Night, and Automatic
(clock), using the same sequence as the Settings `Sky` button. `N` remains
assigned to Next TTM. While an individual scene collection is selected, `T`
runs the next complete original ADS event and preserves its timing, companion
layers, cross-TTM resources, and sprite state. Raw TTM tag stepping remains
available only as the hidden `Ctrl+T` debugging control.
Press `Space` outside an open Settings, Data Files, or Runtime Log panel to
pause the entire scene, including animation timers and active sound effects.
Press `Space` again to resume without consuming the time spent paused. Runtime
Log keeps its existing Space shortcut for pausing trace capture.

### Persistent settings

Display, playback, audio, monitor, and data-folder choices persist in the
single human-readable `JohnnyCastaway.ini` installed beside
`JohnnyCastaway.exe` and `JohnnyCastaway.scr`. Both programs read and write that
same file, so choices made in the application also configure the screensaver.
The file can be edited while Johnny Castaway is closed. Installer upgrades
preserve it, and uninstall removes it. Obsolete LocalAppData and home-folder
settings are removed by the installer rather than imported.

### Menus and scene collections

The sleek shortcut dock at the bottom shows every primary playback and display
control in both desktop and screensaver modes, including the current CRT,
scene-order, scaling, and sharpness selections. It wraps cleanly on narrower
windows, fades after ten seconds of inactivity, and returns on mouse or keyboard
input. Fullscreen is shown only where that control is available.
The Settings menu can select Full Story or any embedded TTM under the friendly
heading `Choose scene collection`. Each collection shows a descriptive title,
its exact TTM resource filename, and a smaller description derived from the
original tag metadata. The menu can advance to the next collection or internal
ADS event without recreating the application window; raw `SET_SCENE` tag
navigation remains available through the hidden `Ctrl+T` debugging shortcut.
After a change, a short on-screen notification shows both the friendly title
and exact TTM.

All 41 collections, their ADS event usage, and all 616 embedded named
subscenes are documented in the collapsible
[TTM scene collection and subscene catalog](TTM_SCENES.md).

Settings also shows the active shared `JohnnyCastaway.ini` path.
Content changes retain the same application window and graphics context: the
borderless fullscreen surface no longer closes, flashes the desktop, or repeats
the startup fade when switching between Full Story and individual TTMs. Only
content-owned scene layers, sprite slots, and currently playing sounds reset.

### Runtime diagnostics

The runtime diagnostics panel combines decoded TTM and ADS instructions and
offers a complete ADS script view with the executing condition/instruction
highlighted. Its visible controls provide search, source filters, capture pause,
scrolling, copy, timestamped export, and history clearing.

### Holiday and sky previews

The settings include a holiday preview that cycles through `Halloween`,
`St. Patrick`, `Christmas`, `New Year`, and `Automatic (calendar)` without
changing the system clock. The preview applies
immediately and remains active across Full Story scene changes until the cycle
returns to automatic date selection.

Use the Settings panel's `Sky` button to cycle the Full Story background through
`Day`, `Night`, and `Automatic (clock)` without changing the Windows clock. The
content session is restarted so the new background appears immediately, while the introductory
title screen is skipped for preview changes. Automatic mode uses night before
06:00 and from 18:00 onward. Clouds move independently across either background
and wrap after leaving the native 640-pixel canvas. `D` provides the same
Day/Night/Automatic cycle without opening Settings.

## Display and performance

### Scene order and image scaling

The menu also contains live display-filter, scene-order, image-scaling, Fast CRT
sharpness, performance-HUD, and shader-benchmark controls.

`In order` still observes the story's day and scene compatibility rules, but
chooses the next eligible scene in the original scene table instead of randomly.
The renderer first composites the scene at its native 640x480 resolution.
Nearest and bilinear scale that completed frame directly; Scale2x applies its
edge-aware rules to a sharp 1280x960 intermediate before final monitor scaling.
The filter remains an independent display setting. These settings persist
between runs. The settings header and error dialogs show the product/build ID.

Normal window/fullscreen mode, mute, stretch/4:3 fit, and monitor selection also
persist between sessions. A command-line option explicitly supplied for the new
session takes precedence over the saved value; use `--fullscreen`, `--sound`, or
`--fit` to reverse saved boolean choices. Screensaver launches remain fullscreen
unless `--windowed` is explicitly supplied and do not overwrite the normal
window preference.

### CRT filters

The `Lottes` setting is a Raylib GLSL 330 port of Timothy Lottes' public-domain
single-pass CRT shader. It adds brightness-shaped scanlines, horizontal beam
filtering, a small bloom, an RGB shadow mask, gamma conversion, and subtle tube
curvature. Lottes works from the native completed frame and performs its own
resampling; the selected Nearest/Bilinear/Scale2x mode resumes when CRT is Off
or Lightweight. If the GPU cannot compile Lottes, the application logs the
failure and uses Lightweight automatically.

The original `Fast` mode uses one inexpensive Raylib-native pass with manual
sharp interpolation, brightness-dependent scanline width, gamma-aware shading,
and a three-column RGB aperture grille. It always consumes the native completed
frame, like Lottes, and performs its own scaling. `F7` changes its sharpness
without changing the active filter, and the selected preset persists between sessions.
Shader compilation failure falls back to Lightweight. Unsupported shader modes
are also removed from the live filter cycle and the F9 benchmark for that run.

`HDR Pop` is a custom single-pass GLSL 330 enhancement for large HDR-capable
screens. It preserves the native pixel-art silhouette while adding restrained
local clarity, color separation, and a soft highlight shoulder without crushing
black. Raylib currently presents an SDR Windows swapchain, so the mode supplies
an HDR-style SDR image for Windows HDR and the display to tone-map; it does not
claim HDR10 output or emit HDR metadata. Compilation failure falls back safely
and removes HDR Pop from the live filter cycle and benchmark for that run.

### Performance HUD and benchmark

Changing filter mode, scaling mode, or Fast CRT sharpness displays the performance
HUD for ten seconds. `F8` pins it across sessions. It reports actual FPS against
the 30 FPS target, CPU frame-submission time, percentage of the 33.3 ms CPU
budget, active modes, output resolution, measured CPU impact, and the expected
relative GPU cost. The settings panel also shows the comparison guide:
`Off: Minimal`, `Lightweight: Very low`, `Fast: Low`, `HDR Pop: Moderate`, and
`Lottes: High`.
GPU shader execution is asynchronous, so CPU submission time is labeled
separately and is not presented as a false uncapped-GPU FPS estimate.

`F9` runs a twenty-second comparison without stopping playback: Off,
Lightweight, Fast, HDR Pop, and Lottes are each measured for four seconds. The
original filter mode is restored afterward and the result table remains visible
for twenty seconds. Run fullscreen on the target monitor—for example with `--monitor N`
and `--fullscreen`—to collect meaningful native 4K or Neo G9 results.

See [the display performance matrix](docs/PERFORMANCE.md) for recorded physical
output results, hardware details, limitations, and the remaining resolution
gates.

### Fullscreen and ultrawide displays

Borderless fullscreen and screensaver modes use the selected monitor's native
size and origin, including 5120x1440 and 7680x2160 Samsung Odyssey Neo G9-class
32:9 displays. The default aspect-preserving mode centers the original 4:3
artwork with opaque black pillarboxes; `--stretch` remains available when filling
the entire panel is preferred. Multi-monitor placement and cursor hiding remain
active in screensaver mode.

### Screensaver interface

In screensaver mode, the settings and runtime overlays remain interactive while
the animation continues behind them. Menu navigation, runtime-log navigation,
and mouse interaction over an open overlay do not close the screensaver.
Unlisted input retains normal screensaver exit behavior; Escape follows the
same two-press quit rule as the desktop application.

At screensaver startup, a compact footer displays the controls plus the live
CRT, scene-order, and scaling selections. It remains fully visible for eight
seconds, fades during the next two seconds, and then disappears without pausing
the animation. Pressing any recognized screensaver control reveals the footer
for another ten seconds.

Nonessential performance and shortcut information follows the same inactivity
rule in every mode: it remains fully visible for eight seconds, fades during the
next two, and wakes immediately on mouse movement, a mouse button, or keyboard
input. Settings, Data Files, Runtime Log, and error/status messages remain
visible and interactive independently of that idle timer.

### Future display work

CRT-Royale remains a future experimental option rather than a selectable mode.
The maintained [reference preset](https://github.com/libretro/slang-shaders/blob/master/crt/crt-royale.slangp)
uses 12 rendering passes and six mask LUTs; even
[CRT-Royale Fast](https://github.com/libretro/slang-shaders/blob/master/crt/crt-royale-fast.slangp)
uses eight passes and three LUTs. Those libretro `.slangp` pipelines require a
Raylib/OpenGL port and performance testing at 4K and dual-4K. The current
scanline/vignette overlay remains the inexpensive, broadly compatible default.

## Testing and diagnostics

### Local testing

Run `build\test.bat` from a Command Prompt to execute the automated regression
suite with the same Go and MSYS2 CGO toolchain used by the x64 build. Run
`powershell -ExecutionPolicy Bypass -File build\stability_sweep.ps1` for the
longer interactive sweep across every embedded TTM and Full Story. Both Windows
QA scripts discover the same nearby `scrantic` convention as the binaries; use
their `-DataDirectory` parameter to test another verified folder explicitly.

### Windows x64 CI

The `Windows x64` GitHub Actions workflow performs the same archive
verification and regression suite on a Windows 2025 runner, installs the
MinGW64 CGO toolchain, builds the GUI executable, verifies its Go metadata is
`windows/amd64`, and uploads the executable as a 14-day workflow artifact.

### Logs and rendering safeguards

The application log in `%LOCALAPPDATA%\JohnnyCastaway` starts each session with
the product version and build revision. At 1 MiB it rotates to
`JohnnyCastaway.log.1`, keeping current failures separate from stale history.

Original `.SCR` backgrounds shorter than 640x480 are padded to the full canvas
using their dominant bottom-edge color. Source artwork is not stretched, and
sprite coordinates remain aligned with the original scene.

## Data files and copyright

The public source and its new Git history exclude the original archives, sounds,
and generated executables. Local data paths are ignored by Git.

> This repository contains no Sierra data files, takes no position on where
> users obtain them, and provides no copies. The data files remain
> Sierra/Dynamix intellectual property.

The screensaver requires the original `RESOURCE.MAP`
and `RESOURCE.001` from the 1992 Sierra/Dynamix release; sound will be optional.
Users copy those files into a folder of their choosing and select it with
`--data-dir`, the first-run folder prompt, or the `F10` Data Files manager. The
application verifies supported files by hash and does not provide, download, or
link to copies.

For reference, the canonical archive hashes are:

### Tested files and checksums

* `RESOURCE.001` - `md5: 8bb6c99e9129806b5089a39d24228a36`
* `RESOURCE.MAP` - `md5: 374e6d05c5e0acd88fb5af748948c899`

The repository includes
[`scrantic/Johnny-Castaway-Original-Data.sfv`](scrantic/Johnny-Castaway-Original-Data.sfv)
for verification with any standard SFV checker. Copy the two user-supplied
archives beside that file and verify these CRC-32 values:

* `RESOURCE.001` - `F11E965A`
* `RESOURCE.MAP` - `40660749`

The SFV contains filenames and checksums only; it does not contain game data.

These are the only game-data archives accepted by the application. They are
loaded from the selected external directory and rejected when either MD5 differs.
When a developer supplies them locally, the test suite verifies both hashes and
decompresses every resource; data-dependent tests skip in a clean public clone.
Narrow
RLE/LZW bounds guards turn truncated or structurally invalid data into a clear
resource error instead of an index-out-of-range failure.

The Apple Silicon macOS application bundle built from this project was also
checked byte-for-byte. Its embedded `RESOURCE.MAP` and `RESOURCE.001` are the
same files with the same hashes; it is not an alternate resource set.

### Resource types

* `.BMP` = used for sprites (4bits per pixel, color indexed (16 color max))
* `.SCR` = used for backgrounds (4bits per pixel, color indexed (16 color max))
* `.ADS` = scene level orchestration (higher level)
* `.TTM` = animation sequencing scripts (lower level)
* `.PAL` = color palette - this game only used up to 16 colors
* `.WAV` = audio - but this engine just references extracted .wav files and plays them

### Known missing original resources

`FIRE.TTM` references `FLAME.BMP` and `FLURRY.BMP`, but neither file exists in
any of the verified original resource archives used by this project. The same
files are also absent from the archive published by Castaway Viewer, and the
known public C, C++, and Go implementations contain references but no bitmap
data for either resource.

`FIRE.BMP` and `FIRE1.BMP` are present, but they contain different sprite sets
and are not valid replacements for the missing files. They are intentionally
left unchanged rather than aliased or reconstructed. The engine skips the two
missing sprite loads, allowing `FIRE.TTM` and full-story playback to continue
without those visual effects. The separate fire-making sequence in
`MJFIRE.TTM`, orchestrated by `BUILDING.ADS`, has all of its required resources.

## Project references

### Related implementations and information

* [deckarep/Johnny-Castaway-2026-Public](https://github.com/deckarep/Johnny-Castaway-2026-Public)
  — direct upstream for this Go/Raylib branch. It supplies the original port
  history, canonical archive hashes, resource-type notes, desktop/WASM goals,
  and a macOS menu-bar idle-timeout screensaver concept. Its final timing fix
  and larger fade radius are already present locally. Its tracked WAV files and
  lack of a declared repository license are not a basis for redistribution;
  the release gates above still apply.
  The [upstream pull-request index](https://github.com/deckarep/Johnny-Castaway-2026-Public/pulls)
  provides additional development history; merged PR #1 is already included in
  this branch's ancestry.
* [jno6809/jc_reborn](https://github.com/jno6809/jc_reborn) — C implementation
  on which this Go port is based.
* [tallPete/JohnnyCastaway](https://github.com/tallPete/JohnnyCastaway) — native
  Swift 6/macOS screensaver organized as resource, engine, Metal renderer, and
  screensaver packages. Its current README reports 104 engine tests and 11
  renderer tests, external user-supplied Sierra data, and GPL-3.0 licensing.
  Its fixes provide regression ideas for 10 TTM slots, multi-hour CPU behavior,
  clipping, scene-start flicker, cloud placement, settings persistence, and
  main-monitor-only audio.
* [vasartori/jhonny-castaway-screensaver](https://github.com/vasartori/jhonny-castaway-screensaver)
  — Linux/XScreenSaver approach that embeds `mpv`, selects randomized offsets,
  separates day/night ranges, and uses a December video variant.
* [11-hour Johnny Castaway recording](https://www.youtube.com/watch?v=l8D6qppreiI)
  — long-run visual, scene-order, timing, and day/night comparison point. It is
  not a source of data files for this project.
* [castaway.xesf.net](https://castaway.xesf.net/) — appearance and behavior
  comparison point for the browser presentation.
* [alexbevi/xbak](https://github.com/alexbevi/xbak) — GPL-3.0 historical Sierra
  resource/archive tooling reference; useful for comparing parsing assumptions,
  not as a source of Johnny Castaway assets.
* [bailli/Johnny](https://github.com/bailli/Johnny) — C++ implementation.
* [ScummVM DGDS engine](https://github.com/scummvm/scummvm/tree/master/engines/dgds)
  — related instruction semantics, but not a Johnny Castaway implementation.

### Interpreter references
ScummVM has some more comprehensive implementation of ADS and TTM instruction set, but it's
a superset of Johnny's instruction set. The compatible buffer-copy and ADS
condition semantics are adapted here where Johnny's embedded scripts use them,
including `DRAW_SCREEN`, AND/OR blocks, `IF_NOT_PLAYED`, asynchronous
`IF_FINISHED`, and the one `WHILE_RUNNING` continuation in `ACTIVITY.ADS`.

## Project status

See [`TODO.md`](TODO.md) for the completed work, confirmed remaining problems,
and the prioritized plan for the next development session.

## License

The engine source and modifications are distributed under GNU GPL version 3 or
later; see [`LICENSE`](LICENSE) and [`NOTICE.md`](NOTICE.md). This license does
not cover or grant rights to Sierra/Dynamix data, artwork, scripts, or sounds,
none of which are included in the public source repository.

