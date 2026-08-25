# Johnny Castaway 2026 Roadmap

Updated: 2026-07-17

## Objective

Ship a trustworthy source-only, native x64 release for Windows 11, then improve
visibility, performance tuning, and visual fidelity. The supported Windows
build has one architecture, one toolchain, one CI job, and two deliverables:
`JohnnyCastaway.exe` and `JohnnyCastaway.scr`.

## Phase 0 — Native Windows 11 x64 baseline (P0)

- [x] Remove the legacy 32-bit screensaver build, resources, workflow, artifact
  names, and compatibility assumptions.
- [x] Build both the desktop application and screensaver with `GOARCH=amd64`
  and the MSYS2 MinGW64 toolchain.
- [x] Give both deliverables amd64 Windows manifests and PE32+ resources.
- [x] Consolidate regression testing, builds, architecture verification, and
  artifact upload in one Windows 11 x64 workflow.
- [ ] Confirm the native artifacts and all screensaver modes on a clean Windows
  11 account or machine.

Exit criteria:

- No legacy 32-bit build code or generated deliverables remain.
- Both deliverables report `GOOS=windows`, `GOARCH=amd64`, and PE32+ format.
- `/c`, `/s`, `/p HWND`, `/p:HWND`, and preview host-close behavior pass.

## Phase 1 — Release candidate verification (P0)

- [x] Run the full Go regression suite and `go vet` with the supported Windows
  CGO toolchain.
- [x] Confirm preview mode creates a real child window inside the Windows Screen
  Saver preview host and exits cleanly.
- [x] Run the all-TTM stability sweep and check for crashes, resource leaks,
  missing-resource failures, and content-switch regressions.
- [x] Perform a final source/data-boundary scan and verify the documented archive
  hashes and clean-clone build instructions.
- [ ] Require the Windows 11 x64 GitHub Actions workflow to pass and retain its
  paired EXE and SCR artifact for release testing.

Exit criteria:

- Local tests, stability sweep, screensaver QA, and x64 CI are green.
- A clean clone can build with user-supplied data and no undocumented files.

## Phase 2 — Publish the release (P0)

- [ ] Merge the verified native x64 branch into `main`.
- [ ] Publish `v2026.1.0-rc.3` with the paired native x64 EXE and SCR, per-user
  installer, required data policy, controls, and unsigned-binary notes.
- [ ] Test the downloadable artifacts on a clean Windows 11 account or machine.
- [ ] Promote the candidate to `v2026.1.0` if that smoke test passes.

Exit criteria:

- The public repository has a reproducible, source-only stable release.
- Installation, data setup, screensaver configuration, and known limitations
  are clear to a new user.

## Phase 3 — DrWize/Home portfolio index (P1)

- [x] Inventory all DrWize projects and give each one a consistent portfolio
  entry with status, purpose, technology, repository link, and an image where
  appropriate.
- [x] Keep source, releases, issues, detailed documentation, and development
  history in each project's canonical repository.
- [ ] Update the Johnny Castaway entry after stable with a concise Windows 11
  x64 summary, CI/release status, and source/data-policy note.

## Phase 4 — Real-hardware performance matrix (P1)

- [ ] Run F9 benchmarks at 1920x1080, 3840x2160, 5120x1440, and 7680x2160.
- [ ] Record Off, Lightweight, Fast, HDR Pop, and Lottes results with GPU, monitor,
  scaling, fit/stretch, average FPS, and CPU submission time.
- [ ] Verify that only the centered 4:3 viewport incurs shader cost on 32:9
  displays.
- [x] Fall back to Lightweight when a CRT shader cannot compile and exclude
  unsupported modes from the live cycle and benchmark.
- [ ] Establish conservative performance thresholds and defaults from physical
  measurements while retaining a user override.

Progress: the four pre-HDR production CRT paths held 30 FPS at a physical 3840x1080
output on an RTX 4080, with 0.15–0.19 ms CPU submission time. The remaining
targets and lower-powered hardware are documented in `docs/PERFORMANCE.md`.

## Phase 5 — Optional CRT modes (P2)

- [x] Evaluate CRT-Easymode; do not import it without an authoritative license
  version.
- [x] Evaluate GPL-2.0-or-later CRT-Geom; defer a port until the physical matrix
  can measure its intended configuration.
- [x] Keep the established CRT modes until another CRT option has a clear license,
  purpose, measured cost, persisted settings, and safe fallback. Details are in
  `docs/ROADMAP_EVALUATIONS.md`.

## Phase 6 — Fidelity and missing artwork decision (P2)

- [ ] Compare representative scenes, long-run order, timing, day/night changes,
  tides, and holiday behavior with documented observation references.
- [x] Enforce clip zones, correct cloud placement and movement, stabilize walk
  frame order and island positioning, and keep the palm tree fixed in place.
- [x] Keep unavailable `FLAME.BMP` and `FLURRY.BMP` effects as documented
  omissions for stable. Any future replacement must have documented authorship
  and licensing and remain optional for original-data comparisons.

## Phase 7 — Other platforms (deferred)

- [x] Evaluate macOS, Linux/XScreenSaver, and WebAssembly and remove incomplete
  platform shims and CI that implied support.
- [ ] Reopen another platform only after the Windows stable release and only
  with an owned runtime, integration, packaging, and QA plan.

## Recommended immediate sequence

1. Commit the native Windows 11 x64 migration.
2. Push the branch and require the Windows 11 x64 CI workflow to pass.
3. Merge and publish `v2026.1.0-rc.3` with its installer, EXE, and SCR.
4. Test RC3 on a genuinely separate Windows 11 account or machine.
5. Promote the verified candidate to stable, then update the portfolio.
