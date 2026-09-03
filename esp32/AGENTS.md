# ESP32 agent instructions

These instructions apply to work under `esp32/` and supplement the general
repository rules in `../AGENTS.md`. Read both files before starting ESP32 work.
Also read the active ESP32 section of `../TODO.md`, `PLAN.md`, `README.md`, the
shared settings contract and the relevant ESP components before changing code.

## Scope and references

Focus ESP32 requests under `esp32/`. Use the desktop engine at commit `343c7f5`
and current desktop decoder tests as behavior references when scene or resource
semantics must remain aligned.

Preserve the component boundaries between board access, resources, graphics,
engine behavior and later networking. Hardware-specific code must not leak into
resource decoding or scene semantics.

## Toolchain and data

Prefer the repo-local ESP-IDF v5.5.x installation when available. Use an
activated ESP-IDF PowerShell environment and separate build directories for
normal firmware, board-test firmware and QEMU firmware.

Before flashing:

- identify the actual serial port with `esptool`; never guess between stale COM
  ports
- verify ESP32-S3 identity, flash capacity and PSRAM capacity
- validate partition sizes and ensure generated `jcdata.bin` fits
- keep canonical `RESOURCE.MAP` and `RESOURCE.001` as ignored build inputs unless
  their distribution rights are explicitly resolved

## Hardware authorization

For an active ESP32 task, it is explicitly acceptable to reset, reboot or
power-cycle the connected ESP32 board and to build, flash, monitor and test it
without asking for confirmation each time. This permission includes entering
the board's download mode and repeating non-destructive firmware tests needed to
diagnose boot, display, touch or serial behavior.

This permission does not authorize rebooting the Windows host, erasing unrelated
device data, changing permanent security settings or flashing a device that has
not first been positively identified.

## Validation evidence

Keep evidence categories distinct:

- a successful configure/build proves only the toolchain and source build
- a QEMU run proves only behavior exercised by the emulated CPU, flash,
  peripherals and virtual framebuffer
- a verified flash proves bytes were written to the identified physical device
- serial logs prove only the behavior visible in those logs
- color, orientation, drift, touch and cold-boot stability require physical
  observation

Never claim physical display or touch success from a build, host fixture, flash
result or emulator alone. When hardware is unavailable, complete host and QEMU
tests plus build validation, then report the exact remaining physical gates.
