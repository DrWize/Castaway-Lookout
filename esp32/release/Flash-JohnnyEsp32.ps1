[CmdletBinding()]
param(
    [string]$Port,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$packageRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$firmwareRoot = Join-Path $packageRoot 'firmware'
$dataRoot = Join-Path $packageRoot 'data'
$dataImage = Join-Path $dataRoot 'jcdata.bin'
$esptoolVersion = '4.12.0'
$esptoolUri = "https://github.com/espressif/esptool/releases/download/v$esptoolVersion/esptool-v$esptoolVersion-windows-amd64.zip"
$esptoolZipSha256 = '42FDDC5E6A05716868AD77FB43ACBF53BE041F97ABED87FF850DF1DC88140889'
$expectedMapMd5 = '374E6D05C5E0ACD88FB5AF748948C899'
$expectedArchiveMd5 = '8BB6C99E9129806B5089A39D24228A36'

function Assert-File([string]$Path) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required file is missing: $Path"
    }
}

function Test-FirmwareChecksums {
    $checksumPath = Join-Path $firmwareRoot 'SHA256SUMS.txt'
    Assert-File $checksumPath
    foreach ($line in Get-Content -LiteralPath $checksumPath) {
        if ($line -notmatch '^([0-9A-Fa-f]{64})\s+\*(.+)$') {
            throw "Invalid checksum line: $line"
        }
        $path = Join-Path $firmwareRoot $Matches[2]
        Assert-File $path
        $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if ($actual -ne $Matches[1]) {
            throw "Firmware checksum failed for $($Matches[2])"
        }
    }
}

function Get-EspTool {
    $cacheRoot = Join-Path $env:LOCALAPPDATA "JohnnyCastawayEsp32\esptool-v$esptoolVersion"
    $archive = Join-Path $cacheRoot "esptool-v$esptoolVersion-windows-amd64.zip"
    $executable = Join-Path $cacheRoot 'esptool-windows-amd64\esptool.exe'
    if (Test-Path -LiteralPath $executable -PathType Leaf) {
        return $executable
    }
    New-Item -ItemType Directory -Path $cacheRoot -Force | Out-Null
    Write-Host "Downloading official Espressif esptool $esptoolVersion..."
    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -UseBasicParsing -Uri $esptoolUri -OutFile $archive
    $actual = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    if ($actual -ne $esptoolZipSha256) {
        throw "esptool download checksum is $actual; expected $esptoolZipSha256"
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $cacheRoot -Force
    Assert-File $executable
    return $executable
}

function New-JcDataImage {
    $mapPath = Join-Path $dataRoot 'RESOURCE.MAP'
    $archivePath = Join-Path $dataRoot 'RESOURCE.001'
    Assert-File $mapPath
    Assert-File $archivePath
    $mapMd5 = (Get-FileHash -LiteralPath $mapPath -Algorithm MD5).Hash
    $archiveMd5 = (Get-FileHash -LiteralPath $archivePath -Algorithm MD5).Hash
    if ($mapMd5 -ne $expectedMapMd5 -or $archiveMd5 -ne $expectedArchiveMd5) {
        throw "Unsupported or mixed Johnny data. Read FLASHING.md and use the matching original RESOURCE.MAP and RESOURCE.001 files."
    }
    if (-not ('JohnnyCastaway.Release.Crc32' -as [type])) {
        Add-Type -TypeDefinition @'
namespace JohnnyCastaway.Release {
    public static class Crc32 {
        public static uint Compute(byte[] first, byte[] second) {
            uint value = 0xffffffffu;
            foreach (byte item in first) value = Update(value, item);
            foreach (byte item in second) value = Update(value, item);
            return value ^ 0xffffffffu;
        }
        private static uint Update(uint value, byte item) {
            value ^= item;
            for (int bit = 0; bit < 8; bit++)
                value = (value >> 1) ^ ((value & 1) != 0 ? 0xedb88320u : 0u);
            return value;
        }
    }
}
'@
    }
    [byte[]]$mapBytes = [IO.File]::ReadAllBytes($mapPath)
    [byte[]]$archiveBytes = [IO.File]::ReadAllBytes($archivePath)
    [uint32]$totalLength = 20 + $mapBytes.Length + $archiveBytes.Length
    [uint32]$crc = [JohnnyCastaway.Release.Crc32]::Compute($mapBytes, $archiveBytes)
    $stream = [IO.File]::Open($dataImage, [IO.FileMode]::Create, [IO.FileAccess]::Write)
    try {
        $writer = [IO.BinaryWriter]::new($stream)
        $writer.Write([byte[]][char[]]'JCDT')
        $writer.Write([byte]1)
        $writer.Write([byte[]](0, 0, 0))
        $writer.Write([uint32]$mapBytes.Length)
        $writer.Write($totalLength)
        $writer.Write($crc)
        $writer.Write($mapBytes)
        $writer.Write($archiveBytes)
        $writer.Flush()
    } finally {
        $stream.Dispose()
    }
    Write-Host "Verified original game data and created private jcdata.bin ($totalLength bytes)."
}

function Get-CandidatePorts {
    $ports = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    if ($Port) {
        return @($Port.ToUpperInvariant())
    }
    try {
        foreach ($serial in Get-CimInstance Win32_SerialPort -ErrorAction Stop) {
            if ($serial.DeviceID -match '^COM\d+$') { [void]$ports.Add($serial.DeviceID) }
        }
    } catch {}
    try {
        foreach ($device in Get-CimInstance Win32_PnPEntity -ErrorAction Stop) {
            if ($device.Name -match '\((COM\d+)\)') { [void]$ports.Add($Matches[1]) }
        }
    } catch {}
    try {
        $serialMap = Get-ItemProperty 'HKLM:\HARDWARE\DEVICEMAP\SERIALCOMM' -ErrorAction Stop
        foreach ($property in $serialMap.PSObject.Properties) {
            if ($property.Value -match '^COM\d+$') { [void]$ports.Add($property.Value) }
        }
    } catch {}
    return @($ports | Sort-Object { [int]($_ -replace '^COM', '') })
}

function Find-JohnnyBoard([string]$EspTool) {
    $candidates = Get-CandidatePorts
    if ($candidates.Count -eq 0) { throw 'No Windows COM ports were found.' }
    foreach ($candidate in $candidates) {
        Write-Host "Checking $candidate..."
        $output = & $EspTool --chip esp32s3 --port $candidate flash_id 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -and
            $output -match 'Chip is ESP32-S3' -and
            $output -match 'Embedded PSRAM 8MB' -and
            $output -match 'Detected flash size: 16MB') {
            Write-Host $output
            return $candidate
        }
    }
    throw 'No supported ESP32-S3-Touch-LCD-7 was identified. Follow the BOOT/RESET recovery steps in FLASHING.md.'
}

try {
    Write-Host 'Johnny Castaway ESP32-S3-Touch-LCD-7 flasher' -ForegroundColor Cyan
    Test-FirmwareChecksums
    New-JcDataImage
    $esptool = Get-EspTool
    $selectedPort = Find-JohnnyBoard $esptool
    if (!$Force) {
        [void](Read-Host "Ready to flash the verified board on $selectedPort. Press Enter to continue or Ctrl+C to cancel")
    }
    $bootloader = Join-Path $firmwareRoot 'bootloader.bin'
    $partitionTable = Join-Path $firmwareRoot 'partition-table.bin'
    $application = Join-Path $firmwareRoot 'johnny_esp32.bin'
    & $esptool --chip esp32s3 --port $selectedPort --baud 460800 `
        --before default_reset --after hard_reset write_flash `
        --flash_mode dio --flash_freq 80m --flash_size 16MB `
        0x0 $bootloader 0x8000 $partitionTable 0x10000 $application 0x310000 $dataImage
    if ($LASTEXITCODE -ne 0) { throw "esptool failed with exit code $LASTEXITCODE" }
    Write-Host 'Flash complete. Every write was verified and the board was hard-reset.' -ForegroundColor Green
    Write-Host 'The startup checks can take about 90 seconds. Continue with FLASHING.md.'
} catch {
    Write-Error $_
    exit 1
}
