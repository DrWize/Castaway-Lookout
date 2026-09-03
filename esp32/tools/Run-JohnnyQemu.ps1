[CmdletBinding()]
param(
    [switch] $BoardTest,
    [switch] $Graphics
)

$ErrorActionPreference = "Stop"
$esp32Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$repoRoot = (Resolve-Path (Join-Path $esp32Root "..")).Path
$idfPath = (Resolve-Path (Join-Path $repoRoot ".tools\esp-idf-v5.5.5")).Path
$idfToolsPath = (Resolve-Path (Join-Path $repoRoot ".tools\espressif")).Path
$python = (Resolve-Path (Join-Path $idfToolsPath "python_env\idf5.5_py3.14_env\Scripts\python.exe")).Path
$qemu = (Resolve-Path (Join-Path $idfToolsPath "tools\qemu-xtensa\esp_develop_9.2.2_20260417\qemu\bin\qemu-system-xtensa.exe")).Path

$profile = if ($Graphics -and $BoardTest) {
    "graphics-board-test"
} elseif ($Graphics) {
    "graphics-scene"
} elseif ($BoardTest) {
    "headless-board-test"
} else {
    "headless-scene"
}
$buildDir = Join-Path $esp32Root "build-qemu-$profile-runner"
$sdkconfig = Join-Path $esp32Root "build-qemu-$profile-runner.sdkconfig"
$profileDefaults = if ($Graphics -and $BoardTest) {
    "sdkconfig.qemu-graphics-board-test"
} elseif ($Graphics) {
    "sdkconfig.qemu-graphics"
} elseif ($BoardTest) {
    "sdkconfig.qemu-board-test"
} else {
    "sdkconfig.qemu"
}

$env:IDF_TOOLS_PATH = $idfToolsPath
. (Join-Path $idfPath "export.ps1")

Push-Location $esp32Root
try {
    idf.py -B $buildDir -D "SDKCONFIG=$sdkconfig" -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;$profileDefaults" build
    if ($LASTEXITCODE -ne 0) { throw "ESP-IDF build failed with exit code $LASTEXITCODE" }

    $mergeArgs = @(
        "-m", "esptool", "--chip", "esp32s3", "merge_bin",
        "--flash_size", "16MB", "--fill-flash-size", "16MB",
        "-o", (Join-Path $buildDir "qemu_flash.bin"),
        "0x0", (Join-Path $buildDir "bootloader\bootloader.bin"),
        "0x8000", (Join-Path $buildDir "partition_table\partition-table.bin"),
        "0x10000", (Join-Path $buildDir "johnny_esp32.bin")
    )
    if (-not $BoardTest) {
        $mergeArgs += @("0x310000", (Join-Path $buildDir "jcdata.bin"))
    }
    & $python @mergeArgs
    if ($LASTEXITCODE -ne 0) { throw "QEMU flash merge failed with exit code $LASTEXITCODE" }

    $efuse = Join-Path $buildDir "qemu_efuse.bin"
    & $python (Join-Path $PSScriptRoot "make_qemu_efuse.py") $idfPath $efuse
    if ($LASTEXITCODE -ne 0) { throw "QEMU eFuse generation failed with exit code $LASTEXITCODE" }

    $qemuArgs = @(
        "-M", "esp32s3", "-m", "8M",
        "-drive", "file=$($buildDir.Replace('\', '/'))/qemu_flash.bin,if=mtd,format=raw",
        "-drive", "file=$($efuse.Replace('\', '/')),if=none,format=raw,id=efuse",
        "-global", "driver=nvram.esp32s3.efuse,property=drive,value=efuse",
        "-global", "driver=timer.esp32s3.timg,property=wdt_disable,value=true",
        "-global", "driver=ssi_psram,property=is_octal,value=true",
        "-nic", "user,model=open_eth"
    )
    if ($Graphics) { $qemuArgs += @("-display", "sdl") } else { $qemuArgs += "-nographic" }
    $qemuArgs += @("-serial", "mon:stdio")
    & $qemu @qemuArgs
}
finally {
    Pop-Location
}
