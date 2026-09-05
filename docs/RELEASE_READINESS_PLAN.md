# Castaway Lookout stable publication record

Updated: 2026-09-05. Windows and ESP32 **2026.1.0 stable** are published from
main. The [release notes](releases/2026.1.0.md) contain the verified downloads,
exact platform source revisions, sizes and SHA-256 values.

## Completed publication and acceptance

| Work | Evidence | Completed |
| --- | --- | --- |
| Installation documentation and repository rename | Platform setup guides; DrWize/Castaway-Lookout with main as default branch | 2026-09-05 |
| Source integration | Documentation/ESP32 snapshot 039813d integrated into main; later accepted weather/login source 38a478c | 2026-09-05 |
| Windows stable package | Source f6d2921, tag v2026.1.0-windows; clean-checkout regression, EXE/SCR, amd64, vet and installer checks passed; CI passed on 158c350 | 2026-09-05 |
| Windows installer acceptance | User checked and accepted installation behavior; stable changes are metadata and repository links | 2026-09-05 |
| ESP32 stable package | Source 4f3db04, tag v2026.1.0; normal build, source CI and 11-file package inventory passed | 2026-09-05 |
| ESP32 physical acceptance | User confirmed login, layout, controls, switching, persistence, smoothness, fidelity and weather wording | 2026-09-05 |
| ESP32 device/serial verification | Identified ESP32-S3 rev 0.2, 16 MB flash/8 MB PSRAM; verified app-only flash and RTS reboot preserving NVS; version 2026.1.0; web at 86.863 s, fresh weather at 91.407 s | 2026-09-05 |
| Public release verification | Latest stable release serves both platforms; all seven assets downloaded and byte-verified | 2026-09-05 |

## Evidence boundaries and historical references

The 63-scene physical review remains closed in the ESP32 ledger. User acceptance
of unchanged runtime is distinct from automated tests, flash and serial evidence.
The stable ESP32 binary changed only 74 bytes in the descriptor/checksum relative
to the accepted build at 38a478c. That earlier app hash is
`eca23cb337b03bb09f358d55d86d1f4c416d0d7723d876ae3a994ff0d9c1796d`;
it is historical evidence, not the stable app hash. Its weather refresh was
observed at 91.683 seconds after reboot. Earlier September 4 validation remains
in the technical ledgers and Git history.

Successful browser login was confirmed by the user. Login diagnostics identify
storage open/write/commit failures without disclosing secrets; no underlying
storage root cause is claimed fixed. The selected weather location survives
reboot. LAST UPDATED, SAVED WEATHER and WAITING FOR WEATHER were visually accepted.

## Remaining follow-up work

- Complete the full separate-account Windows screensaver checklist in TODO:
  configuration, full-screen and preview modes, idle activation, normal exit,
  settings and data selection, sound setup and uninstall. Installer acceptance
  does not mean each scenario was independently retested.
- Preserve interpreter/lifecycle work in the ESP32 fidelity plan and the Windows
  physical CRT performance matrix. Stable publication does not close these items.
- Consider faster end-user startup and shorter retries after weather failures.
  Current weather attempts start after connection, poll eligibility every five
  seconds and use a 45-minute interval after either success or failure.

## Documentation and source policy

The root README is the installation entry point; developer details stay in
reference documents. Windows executable names, installer identity, shared INI
and settings paths are unchanged. Numeric Windows file/installer version is
2026.1.0.4, while the product version is 2026.1.0.

Keep the two source tags distinct: GitHub's automatic v2026.1.0 archive identifies
ESP32; Windows uses v2026.1.0-windows. Published binaries, packaged historical
documents and older GitHub releases/tags are retained. Original resource files,
private jcdata.bin and credentials remain excluded. Documentation cleanup does
not rebuild firmware, flash devices or replace release assets.
