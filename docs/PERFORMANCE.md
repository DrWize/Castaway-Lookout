# Display performance matrix

This document records physical-output measurements for Castaway Lookout display and
display-filter modes. Results are hardware-specific and are not substitutes for testing on
lower-powered GPUs. It covers the Windows renderer only; ESP32 panel performance
and acceptance are tracked in `../esp32/ALL_SCENES_FIDELITY.md`.

## 2026-07-16 baseline

Hardware and software:

- Windows x64 release-candidate artifact built from commit `a1f395e`
- NVIDIA GeForce RTX 4080, driver 610.74
- OpenGL 3.3 / GLSL 3.30 through Raylib 5.5
- 3840x1080 physical output
- centered 4:3 image, bilinear scaling, 30 FPS application target
- original data loaded from the user's ignored local data directory

Each mode was launched explicitly with `--crt MODE`, allowed to settle, and
recorded through the F8 performance instrumentation after 90 render samples.
CPU submission time measures the application's render submission work; the
current HUD's GPU-cost label is qualitative and is not a measured GPU duration.

| Display mode | FPS | CPU submit | 30 FPS budget | CPU impact | Expected GPU cost |
| --- | ---: | ---: | ---: | --- | --- |
| Off | 30 | 0.19 ms | 1% | Very low | Minimal |
| Lightweight | 30 | 0.19 ms | 1% | Very low | Very low |
| Fast / Balanced | 30 | 0.15 ms | <1% | Very low | Low |
| Lottes | 30 | 0.18 ms | 1% | Very low | High |

All modes met the target frame rate at this resolution. These numbers show no
CPU submission bottleneck on the tested machine, but they do not establish safe
defaults for integrated or older GPUs.

HDR Pop was added after this baseline and has no physical-output result yet. It
must be measured with Windows HDR both enabled and disabled; because Raylib's
current swapchain is SDR, treat it as an HDR-style enhancement rather than an
HDR10 signal.

## Required measurements

The release-quality matrix remains incomplete until the same modes are tested
on physical outputs at:

- 1920x1080
- 3840x2160
- 5120x1440
- 7680x2160

For each run, record GPU and driver, monitor/output mode, scaling and fit mode,
average FPS, CPU submission time, visible shader defects, fallback behavior,
and whether only the centered 4:3 viewport incurs shader cost. Test at least one
lower-powered or integrated GPU before selecting automatic quality defaults.

## Current recommendation

Keep the existing explicit user choice and conservative fallback behavior. Do
not select a new automatic CRT default from the RTX 4080 baseline alone.
