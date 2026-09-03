# Repository Guidelines

## Project Structure & Module Organization

The repository root contains the Go 1.24 desktop engine. Runtime code is grouped by responsibility in files such as `graphics.go`, `story.go`, `resource.go`, and `sound.go`; tests sit beside the code as `*_test.go`. Windows build and QA scripts live in `build/`, installer sources in `installer/`, user documentation in `docs/`, and artwork in `assets/`. The ESP32 port is an independent ESP-IDF project under `esp32/`; read `esp32/AGENTS.md` before changing it. Original Sierra/Dynamix files belong in ignored `scrantic/` directories and must never be committed.

## Build, Test, and Development Commands

- `build\test.bat` configures Go, CGO, and MSYS2, then runs `go test ./...` for Windows amd64.
- `build\build.bat` runs tests and produces `build\JohnnyCastaway.exe` plus `build\JohnnyCastaway.scr`.
- `powershell -ExecutionPolicy Bypass -File build\stability_sweep.ps1` exercises data-dependent scene playback; it requires valid external game resources.
- `gofmt -w <files>` formats changed Go files before review.

CI repeats the Windows test and build flow in `.github/workflows/windows-x64.yml`. Set `MSYS2_ROOT` if MSYS2 is not installed at `C:\msys64`.

## Coding Style & Naming Conventions

Follow standard Go formatting: tabs via `gofmt`, short package-level filenames, `PascalCase` for exported identifiers, and `camelCase` for internal identifiers. Keep platform-specific behavior in files such as `platform_windows.go`; avoid spreading Windows or ESP32 details through shared engine logic. Prefer focused changes and comments that explain non-obvious resource formats or rendering behavior.

## Testing Guidelines

Use Go's `testing` package. Name tests `TestXxx`, place them in the corresponding `*_test.go`, and favor table-driven cases for parsers, paths, and state transitions. Run `build\test.bat` for every Go change and the stability sweep when playback, timing, resources, or rendering changes. There is no fixed coverage threshold; new behavior should have regression coverage. Clean public clones may skip tests requiring copyrighted data.

## Commit & Pull Request Guidelines

Recent commits use concise, imperative subjects, for example `Improve TTM event playback and scene rendering`. Keep each commit scoped to one logical change. Pull requests should explain behavior and motivation, list validation performed, link relevant issues, and include screenshots or captures for visible rendering/UI changes. Note any data-dependent, QEMU-only, or physical-hardware checks that remain unverified. Never commit generated binaries, private resources, credentials, or unrelated worktree changes.
