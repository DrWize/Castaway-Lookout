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
| D1 | Installation-only README and platform guides | Complete | Baseline | 57-line README, separate guides, correct downloads and local links/anchors | 2026-09-05 |
| D2 | Preserve developer references and correct status claims | Complete | D1 | Technical reference and docs index added; premature main claim corrected; physical gate retained | 2026-09-05 |
| P1 | Refresh and inspect ESP32 package | Complete | D1, D2, S1 | Exact 11-file archive inventory; firmware bytes/checksums unchanged; guide/source/license included; no private data | 2026-09-05 |
| S1 | Commit intended source and integrate into main | Complete | Static package preparation | `039813d51698cd275ab39034a391b11a76ff5e59` pushed and matched live main; unrelated GIF retained | 2026-09-05 |
| R1 | Rename repository to Castaway-Lookout | Complete | S1 | GitHub reports `DrWize/Castaway-Lookout`, default main; origin updated; Windows RC3 assets retained | 2026-09-05 |
| R2 | Publish ESP32 RC4 prerelease | Complete | R1, P1 | Public RC4 has ZIP and checksum; tag points to `92be4bd`; prerelease=true | 2026-09-05 |
| V1 | Verify GitHub state and downloads | Complete | R2 | Renamed repo/default main verified; anonymous RC4 downloads byte-match local files; Windows RC3 installer hash unchanged | 2026-09-05 |
| H1 | Physical sidebar/control acceptance | Passed by user | Existing RC4 firmware | Clock/weather appearance, smooth playback, Reviewer/Off, controls and reboot persistence confirmed; weather refresh tracked separately below | 2026-09-05 |

### H1 user observations â€” 2026-09-05

- Acceptance update: the user explicitly confirms the real browser login,
  physical presentation/control/persistence checks and release-critical fidelity
  checks have passed. This is user acceptance evidence, not a new automated run.
  Local diagnostic source/tests remain uncommitted; no root cause is asserted.

- Login blocker: after reboot, the user reports "Server has encountered an
  unexpected error" with the password box still visible. Approved diagnosis:
  add stage-specific session-storage errors and sanitized serial NVS counts;
  run focused checks, build normal firmware and app-flash without erasing
  settings. Superseded by the user's successful-login confirmation above.

- Confirmed by the user: the selected weather location survives reboot.
- Reported: startup takes a long time; `STALE DATA` appears after reboot.
  Startup duration has not been timed, and a successful post-reboot weather
  refresh has not yet been confirmed.
- Source inspection: normal firmware runs extensive deterministic/resource and
  all-63-scene startup checks before starting networking. The installation guide
  currently allows about 90 seconds; this is not a new measured boot time.
- Cached forecasts are deliberately marked stale when restored. The weather
  task attempts a fetch once Wi-Fi is connected and a location is configured,
  checking eligibility every five seconds. It refreshes every 45 minutes and
  currently also waits 45 minutes after a failed attempt. Selecting a location
  triggers an earlier attempt. Success clears stale; failure retains the cache.
- Follow-up candidates: shorten end-user startup by separating boot regression
  fixtures from normal playback, and use a shorter retry after weather failures.
  These are recorded for consideration; no firmware changes are made here.
- Weather follow-up completed 2026-09-05: local firmware now shows `LAST UPDATED`
  and time, `SAVED WEATHER` when time is unavailable, or `WAITING FOR WEATHER`
  without a forecast. Controller wording matches and includes the update date.
  Five focused checks, controller syntax/state execution and normal build pass.
  COM4 N16R8 app-only flash verified and RTS rebooted without erasing NVS.
  Serial confirms the saved Solna forecast refreshed at 91.683 seconds after
  reboot. User confirmed the new wording "looks good" on 2026-09-05, closing
  physical wording acceptance and the weather follow-up. Local changes remain
  uncommitted; published RC4 assets have not been replaced.

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

## Static inspection evidence â€” 2026-09-05

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
The RC4 guide ships byte-for-byte in the published ZIP. The landing page now
links directly to the verified public assets.

## Published result â€” 2026-09-05

- Repository: [DrWize/Castaway-Lookout](https://github.com/DrWize/Castaway-Lookout),
  default branch `main`. Local checkout remains in its existing JohnnyCx64
  directory; origin uses the renamed URL. The GitHub description now identifies
  both Windows and ESP32. Runtime branding and installer identity are unchanged.
- [ESP32 RC4 prerelease](https://github.com/DrWize/Castaway-Lookout/releases/tag/v2026.1.0-rc.4)
  is public. Its tag and packaged SOURCE.md identify
  `92be4bde6b63f7b08969d493242d747812ec2be4`; this source is on main.
  Subsequent publication-status documentation does not alter firmware source.
- ZIP: `JohnnyCastaway-ESP32-S3-Touch-LCD-7-2026.1.0-rc.4.zip`, 852,260 bytes.
  SHA-256: `101d1f5e41cb0896a1d6539edf61acd281bfcd33611e4fd0595d96e6930c126b`.
  Both ZIP and `.zip.sha256` were downloaded anonymously and matched local bytes.
- Archive inventory: flasher BAT/PS1, FLASHING.md, SOURCE.md, LICENSE, NOTICE.md,
  data-folder placeholder, three firmware binaries and firmware/SHA256SUMS.txt.
  No original resource archives, `jcdata.bin`, credentials or build caches ship.
- Windows RC3 installer was downloaded through the renamed repository URL and
  retains SHA-256
  `7cf840099fbb87e3c8b77b4de13fe8ff9be23944feb19e1d2f940879756f4d4d`.
- No tests, firmware builds or flashing were performed. Existing GitHub CI was
  left unchanged. H1 remains open; publication is not physical acceptance.

## Accepted firmware republished — 2026-09-05

- User authorized updating main and publishing the accepted follow-up.
- Firmware source commit: `38a478c6478dde55b5177c28494aad345a9975db`, pushed
  and verified on main. Includes weather wording, login storage diagnostics,
  focused login tests and dated user acceptance records.
- RC4 ZIP and checksum replaced with the accepted firmware package; release
  notes now describe the changes, validation and completed physical acceptance.
- Firmware: 1,209,616 bytes, SHA-256
  `eca23cb337b03bb09f358d55d86d1f4c416d0d7723d876ae3a994ff0d9c1796d`.
- ZIP: 853,015 bytes, SHA-256
  `c38b6773012554746b78fc89d2c628f2696787264676c62c3ad61e81f5dc5826`.
- Anonymous ZIP/checksum downloads byte-match local assets. The 11-file archive
  contains the accepted app bytes and exact source revision in SOURCE.md;
  original resources, private data and credentials are excluded.
- Existing RC4 tag `92be4bd` is retained. Release notes explicitly distinguish
  the updated package source from GitHub's automatic original-tag source archive.
  RC4 remains a prerelease; Windows artifacts are unchanged.
- Prior build, focused checks, verified flash/reboot, fresh-weather serial
  evidence and user visual acceptance apply to these exact firmware bytes.

## ESP32 stable promotion — 2026-09-05

User authorized graduating the accepted RC4 to stable. Target: `v2026.1.0`,
ESP32 firmware version `2026.1.0`, latest release with prerelease=false.
Only version metadata, packaging defaults and documentation change; runtime
behavior retains the user's RC4 acceptance. Build, identified-device flash,
serial verification, package/source provenance and public download verification
are complete. Windows RC3 and its separate acceptance work remain unchanged.

Stable promotion closed 2026-09-05:

- Source/tag: `4f3db04d65801c8834c6c4dfd608cbde70ff6abb` on main; annotated
  `v2026.1.0` points to this exact package source revision. GitHub source CI passed.
- Normal stable build passed; only 74 descriptor/checksum bytes differ from
  the accepted RC4 app. Runtime bytes outside those regions are identical.
- App: 1,209,616 bytes, SHA-256
  `41c9b4ce4a3472efaebce88d66dcb606ed46a53e4bb9313dc83e1acce3a2ad7d`.
- COM4 positively identified as ESP32-S3 rev 0.2, 16 MB flash/8 MB PSRAM;
  app-only flash verified and RTS rebooted with NVS preserved. Serial reported
  version 2026.1.0, web ready at 86.863 seconds and fresh weather at 91.407 seconds.
- Stable ZIP: 852,998 bytes, SHA-256
  `63f5549ab04dffc84a74b396e1d1e2cec7c71cc5de1211d8ffd2f477d3dab9be`.
  Eleven-file inventory, guide, source revision and firmware checksums verified.
- Public ZIP/checksum downloads byte-match local assets. GitHub latest reports
  `v2026.1.0`, draft=false and prerelease=false. Installation links use stable.
- Prior RC releases and Windows assets remain available unchanged. Physical
  acceptance is inherited from unchanged RC4 behavior, not claimed as a new
  independent visual test of the metadata-only stable build.

## Windows stable promotion — started 2026-09-05

User requested the same stable promotion for Windows. Prepare 2026.1.0
application, screensaver and installer; keep installation identity/settings and
runtime behavior unchanged. Update version metadata and links, build from an
isolated clean source checkout, run Windows regression/build and vet checks,
verify native PE metadata and installer packaging, then publish Windows assets
alongside ESP32 in the existing stable release with explicit source provenance.
The existing ESP32 stable tag stays fixed; a Windows-specific source tag will
identify the Windows build. User clean-account/machine acceptance confirmation
has been requested; publication awaits that evidence or explicit gate waiver.
