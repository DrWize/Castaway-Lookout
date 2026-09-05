# Castaway Lookout release readiness

Started: 2026-09-05. Owner: repository maintainer. This is the tracking document
for the installation documentation, project rename, main-branch integration and
ESP32 RC4 prerelease. Runtime UI redesign and new firmware development are not
part of this pass.

## Baseline verified on 2026-09-05

- GitHub default branch: `main`, at `343c7f560944bec57a2c908518dd758e9c42246a`.
- Active source branch: `feature/esp32-first-scene`, at
  `48c1490ac35be4c50ad708e2bb176c6288840060`, with pending RC4 source/docs and
  untracked release helpers. The claim that RC4 was already on main was wrong.
- Windows RC3 is published. No RC4 tag or release exists at this baseline.
- Local RC4 ZIP exists. The normal firmware SHA-256 matches the prior recorded
  candidate: `82cc2dcb528532d5f5eeec866d1676c588f181afa102e6e57ce30caa88c28267`.
- Supported targets: Windows 11 x64 and Waveshare ESP32-S3-Touch-LCD-7,
  800x480, N16R8. The ESP32 installation helper runs on Windows x64.
  There is no macOS version of this project.
- The flasher needs no Git, separately installed Python or ESP-IDF. It checks
  chip/memory identity; the user must identify the physical display model.
- GitHub reports admin access to the current repository; the proposed
  `DrWize/Castaway-Lookout` name returns 404 before rename.

## Ordered work

| ID | Task | Status | Depends on | Evidence / remaining work | Completed |
| --- | --- | --- | --- | --- | --- |
| D1 | Installation-only README and platform guides | In progress | Baseline | Separate Windows RC3 and ESP32 RC4 paths; finish static review | — |
| D2 | Preserve developer references and correct status claims | Pending | D1 | Retain attribution, technical evidence and open physical gates | — |
| P1 | Refresh and inspect ESP32 package | Pending | D1, D2 | Reuse existing firmware; include working packaged documentation and checksums; exclude private data | — |
| S1 | Commit intended source and integrate into main | Complete | Static package preparation | `039813d51698cd275ab39034a391b11a76ff5e59` pushed and matched live main; unrelated GIF retained | 2026-09-05 |
| R1 | Rename repository to Castaway-Lookout | Complete | S1 | GitHub reports `DrWize/Castaway-Lookout`, default main; origin updated; Windows RC3 assets retained | 2026-09-05 |
| R2 | Publish ESP32 RC4 prerelease | Pending | R1 | ZIP, checksum, source revision, release notes; keep Windows RC3 download | — |
| V1 | Verify GitHub state and downloads | Pending | R2 | Confirm main, repository name, release assets and downloaded hashes | — |
| H1 | Physical sidebar/control acceptance | Open, outside this pass | Existing RC4 firmware | Clock layout/colour, city selection, smoothness, Reviewer/Off, persistence, weather staleness | — |

## Documentation and packaging decisions

- Public project name: **Castaway Lookout**. Keep executable names, installer
  identity, settings paths and runtime UI unchanged.
- The root README is the installation entry point. Detailed Windows setup and
  ESP32 flashing each have one guide. Developer/reference material stays linked
  from reference documentation, not mixed into installation steps.
- Use exact platform release links. Until RC4 is published, label its download
  as pending; never describe a local ZIP as publicly downloadable.
- ESP32 RC4 remains a prerelease. The existing Windows RC3 assets stay intact.
- Windows installer data setup can download a verified archive or use an
  existing archive; ESP32 users supply the two original resource files.
- Repack existing firmware without rebuilding. Record artifact hashes and
  source provenance; do not claim the embedded revision is a new build.
- Preserve unrelated working-tree content, especially `build/Johnny-telescope.gif`.

## Review policy and acceptance

No tests, firmware builds, flashing or runtime UI changes are requested for this
pass. Review documentation links, source/package inventories, hashes, release
metadata and repository status statically. Existing GitHub CI may run on push;
leave its configuration unchanged.

Earlier tests, builds, flash/serial checks and the closed 63-scene review are
historical evidence recorded in the ESP32 ledgers. They are not fresh validation
of this publication, and do not close H1. Mark publishing tasks complete only
after the corresponding live GitHub evidence is available.

Success: a new user can select the correct download and finish setup without
developer documentation. GitHub main contains the intended source and docs;
the renamed repository serves the RC4 ZIP and checksum.

## Static inspection evidence — 2026-09-05

- Firmware descriptor: project `johnny_esp32`, version `2026.1.0-rc.4`, built
  `Sep 4 2026 22:32:42`; binary length 1,208,256 bytes. It matches the baseline
  SHA-256 above. Runtime source inputs predate this binary; no runtime source
  was edited in this publication pass.
- Current configuration disables QEMU, board-test and REVIEW-only profiles.
  Existing application and private data image fit their declared partitions.
- Local Markdown targets/anchors resolve. The official Waveshare board and FAQ
  references confirm UART1 routing; the Windows CH343 driver URL returns 200.
- Packaging script syntax was inspected with PowerShell's parser. This is
  static inspection, not a package-flasher execution or firmware test.

Packaging provenance requires a committed source revision, so the final ZIP is
assembled after S1; its static preparation preceded the source integration.
The release guide is prepared for the RC4 asset; the landing page remains
explicitly pending until publication is verified.
