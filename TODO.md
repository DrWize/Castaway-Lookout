# Johnny Castaway 2026 — Release and fidelity worklist

Updated: 2026-08-25

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
- [ ] Publish the installer only after the user has tested and approved it.

## Ordered work queue

Complete these items from top to bottom. Later work should not delay the release
unless it exposes a stability, data-safety, or fidelity regression.

1. [ ] **Finish the remaining release-critical fidelity comparison.**
   Compare long-running scene order, transitions, timing, cloud speed and
   wrapping, waves, tides, holidays, and day/night behavior. The 11-hour video
   samples at the beginning and around 1, 5, 10, and 11 hours confirm the
   640x480 composition and horizontal cloud drift, but all sampled sections are
   daytime. Find a separate trustworthy original night reference before making
   further night-palette or transition changes. If no reliable night reference
   is available, keep the limitation documented and proceed.

2. [ ] **Open a pull request and require the Windows 11 x64 CI job to pass.**
   Review the final PR diff and artifact contents before merging into `main`.
   Do not include generated executables or original game data.

3. [ ] **Create `v2026.1.0-rc.3` with the Windows installer.**
   Include the paired native binaries, per-user installer, shared INI,
   verified data setup, and Castaway Lookout icon.

4. [ ] **Test RC3 on a separate Windows account or machine.**
   Verify clean first launch with the default `scrantic` convention, explicit
   `--data-dir`, saved Data Files selection, native x64 startup, `/c`, `/s`, and
   Windows Screen Saver Settings preview/install behavior. Also verify missing
   data errors, saved settings, unsigned-binary warnings, and normal input exit.

5. [ ] **Promote the verified candidate to `v2026.1.0` stable.**
   Publish final SHA-256 values and release notes, retain the source-only/data
   policy, and confirm both downloadable artifacts were built from the stable
   tag.

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
