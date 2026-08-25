[CmdletBinding()]
param(
    [ValidateSet('Interactive', 'Download', 'Archive')]
    [string]$Mode = 'Interactive',
    [string]$ArchivePath = '',
    [string]$TargetDirectory = '',
    [switch]$InstallSound,
    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$resourceArchiveUri = 'https://archive.org/download/johnny-castaway-screensaver/scrantic-run.zip'
$resourceArchiveLength = 1248045
$resourceArchiveCrc32 = '5AF11A9B'
$resourceArchiveSha256 = '111C384AA44FC810C0453F524D8C02DEE58EA3358EE788316B2FC1F2059AFC56'
$soundCommit = 'be6afefd43a3334acc66fc9d777c162c8bfb9558'
$soundArchiveUri = "https://codeload.github.com/nivs1978/Johnny-Castaway-Open-Source/zip/$soundCommit"
$soundArchiveSha256 = '37CBE04E7BE37A3E729A87D44B18BC94FB96596C2AD2339F91C14BDDC1B5C822'
$manifestPath = Join-Path $PSScriptRoot 'sound-manifest.json'

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -TypeDefinition @'
using System;

namespace JohnnyCastawaySetup
{
    public static class Crc32
    {
        public static uint Compute(byte[] data)
        {
            uint crc = 0xffffffff;
            foreach (byte value in data)
            {
                crc ^= value;
                for (int bit = 0; bit < 8; bit++)
                    crc = (crc & 1) != 0 ? (crc >> 1) ^ 0xedb88320 : crc >> 1;
            }
            return ~crc;
        }
    }
}
'@

function Get-HashHex {
    param(
        [Parameter(Mandatory)]
        [string]$LiteralPath,
        [Parameter(Mandatory)]
        [ValidateSet('MD5', 'SHA256')]
        [string]$Algorithm
    )

    return (Get-FileHash -LiteralPath $LiteralPath -Algorithm $Algorithm).Hash.ToUpperInvariant()
}

function Assert-Equal {
    param(
        [Parameter(Mandatory)]
        $Actual,
        [Parameter(Mandatory)]
        $Expected,
        [Parameter(Mandatory)]
        [string]$Description
    )

    if ($Actual -ne $Expected) {
        throw "$Description is $Actual; expected $Expected"
    }
}

function Get-Crc32Hex {
    param([Parameter(Mandatory)][byte[]]$Bytes)

    return ([JohnnyCastawaySetup.Crc32]::Compute($Bytes)).ToString('X8')
}

function Get-ZipEntryBytes {
    param(
        [Parameter(Mandatory)]
        [System.IO.Compression.ZipArchive]$Archive,
        [Parameter(Mandatory)]
        [string]$EntryName,
        [switch]$MatchSuffix
    )

    $matches = @($Archive.Entries | Where-Object {
        $normalized = $_.FullName.Replace('\', '/')
        if ($MatchSuffix) {
            $normalized.EndsWith($EntryName, [StringComparison]::OrdinalIgnoreCase)
        } else {
            $normalized.Equals($EntryName, [StringComparison]::Ordinal)
        }
    })
    if ($matches.Count -ne 1) {
        throw "ZIP entry '$EntryName' matched $($matches.Count) entries; expected exactly one"
    }
    $entry = $matches[0]
    if ($entry.FullName.Contains('../') -or $entry.FullName.Contains('..\')) {
        throw "Unsafe ZIP entry path: $($entry.FullName)"
    }
    $stream = $entry.Open()
    try {
        $memory = New-Object System.IO.MemoryStream
        try {
            $stream.CopyTo($memory)
            return $memory.ToArray()
        } finally {
            $memory.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

function Test-WaveBytes {
    param(
        [Parameter(Mandatory)]
        [byte[]]$Bytes,
        [Parameter(Mandatory)]
        [string]$Name
    )

    if ($Bytes.Length -lt 12) {
        throw "$Name is too short to be a WAV file"
    }
    $riff = [Text.Encoding]::ASCII.GetString($Bytes, 0, 4)
    $wave = [Text.Encoding]::ASCII.GetString($Bytes, 8, 4)
    if ($riff -ne 'RIFF' -or $wave -ne 'WAVE') {
        throw "$Name does not have a valid RIFF/WAVE header"
    }
}

function Invoke-Download {
    param(
        [Parameter(Mandatory)]
        [string]$Uri,
        [Parameter(Mandatory)]
        [string]$Destination
    )

    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $Uri -OutFile $Destination -UseBasicParsing
}

function Select-InteractiveOptions {
    Add-Type -AssemblyName System.Windows.Forms
    $notice = @'
Johnny Castaway needs original Sierra/Dynamix data that is not included with the modern application.

Yes: download the preserved scrantic-run.zip from Internet Archive.
No: select an existing scrantic-run.zip.
Cancel: leave setup unchanged.

Internet Archive and JCOS are independent third-party sources. Their availability does not transfer ownership of the original Sierra/Dynamix content.
'@
    $choice = [System.Windows.Forms.MessageBox]::Show(
        $notice,
        'Johnny Castaway data setup',
        [System.Windows.Forms.MessageBoxButtons]::YesNoCancel,
        [System.Windows.Forms.MessageBoxIcon]::Information)
    if ($choice -eq [System.Windows.Forms.DialogResult]::Cancel) {
        throw 'Data setup was canceled.'
    }
    if ($choice -eq [System.Windows.Forms.DialogResult]::Yes) {
        $script:Mode = 'Download'
    } else {
        $dialog = New-Object System.Windows.Forms.OpenFileDialog
        $dialog.Title = 'Select scrantic-run.zip'
        $dialog.Filter = 'ZIP archives (*.zip)|*.zip|All files (*.*)|*.*'
        if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
            throw 'Data setup was canceled.'
        }
        $script:Mode = 'Archive'
        $script:ArchivePath = $dialog.FileName
    }

    $soundChoice = [System.Windows.Forms.MessageBox]::Show(
        'Install the 23 optional Johnny Castaway sound effects now? You can add them later by running Data and Sound Setup from the Start menu.',
        'Johnny Castaway sound effects',
        [System.Windows.Forms.MessageBoxButtons]::YesNo,
        [System.Windows.Forms.MessageBoxIcon]::Question)
    $script:InstallSound = $soundChoice -eq [System.Windows.Forms.DialogResult]::Yes
}

function Install-JohnnyData {
    if ($Mode -eq 'Interactive') {
        Select-InteractiveOptions
    }
    if (!$TargetDirectory) {
        $localAppData = [Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)
        $script:TargetDirectory = Join-Path $localAppData 'JohnnyCastaway\scrantic'
    }

    $targetParent = Split-Path -Parent $TargetDirectory
    New-Item -ItemType Directory -Path $targetParent -Force | Out-Null
    $operationId = [Guid]::NewGuid().ToString('N')
    $workDirectory = Join-Path $targetParent ".setup-$operationId"
    $stageDirectory = Join-Path $targetParent ".scrantic-stage-$operationId"
    $backupDirectory = Join-Path $targetParent ".scrantic-backup-$operationId"
    New-Item -ItemType Directory -Path $workDirectory, $stageDirectory -Force | Out-Null

    try {
        if (Test-Path -LiteralPath $TargetDirectory -PathType Container) {
            Get-ChildItem -LiteralPath $TargetDirectory -Force | ForEach-Object {
                Copy-Item -LiteralPath $_.FullName -Destination $stageDirectory -Recurse -Force
            }
        }

        $resourceZip = Join-Path $workDirectory 'scrantic-run.zip'
        if ($Mode -eq 'Download') {
            Write-Host 'Downloading verified Johnny Castaway data from Internet Archive...'
            Invoke-Download -Uri $resourceArchiveUri -Destination $resourceZip
        } else {
            if (!(Test-Path -LiteralPath $ArchivePath -PathType Leaf)) {
                throw "Selected archive was not found: $ArchivePath"
            }
            Copy-Item -LiteralPath $ArchivePath -Destination $resourceZip -Force
        }

        $resourceZipBytes = [IO.File]::ReadAllBytes($resourceZip)
        Assert-Equal $resourceZipBytes.Length $resourceArchiveLength 'scrantic-run.zip size'
        Assert-Equal (Get-Crc32Hex $resourceZipBytes) $resourceArchiveCrc32 'scrantic-run.zip CRC32'
        Assert-Equal (Get-HashHex $resourceZip 'SHA256') $resourceArchiveSha256 'scrantic-run.zip SHA-256'

        $resourceManifest = @(
            [pscustomobject]@{ Name = 'RESOURCE.MAP'; Size = 1453; Crc32 = 'F2528ED1'; Sha256 = '976F15EA100A84244B0B5B11A5255F936DED6D744C4ADBBD03C66DA78A804D18' },
            [pscustomobject]@{ Name = 'RESOURCE.001'; Size = 1175278; Crc32 = '58F392A9'; Sha256 = 'EEA4138EA958AC101EAA58B5A5A5608CDE4F0D65467C0216BCB8B425D76F37B5' }
        )
        $archive = [IO.Compression.ZipFile]::OpenRead($resourceZip)
        try {
            foreach ($file in $resourceManifest) {
                $bytes = Get-ZipEntryBytes -Archive $archive -EntryName $file.Name
                Assert-Equal $bytes.Length $file.Size "$($file.Name) size"
                Assert-Equal (Get-Crc32Hex $bytes) $file.Crc32 "$($file.Name) CRC32"
                $destination = Join-Path $stageDirectory $file.Name
                [IO.File]::WriteAllBytes($destination, $bytes)
                Assert-Equal (Get-HashHex $destination 'SHA256') $file.Sha256 "$($file.Name) SHA-256"
            }
        } finally {
            $archive.Dispose()
        }

        if ($InstallSound) {
            if (!(Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
                throw "Sound manifest is missing: $manifestPath"
            }
            $parsedManifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            $soundManifest = @()
            foreach ($entry in $parsedManifest) {
                $soundManifest += $entry
            }
            Assert-Equal $soundManifest.Count 23 'sound manifest entry count'
            $soundZip = Join-Path $workDirectory 'jcos-sounds.zip'
            Write-Host 'Downloading the verified optional sound effects from JCOS...'
            Invoke-Download -Uri $soundArchiveUri -Destination $soundZip
            Assert-Equal (Get-HashHex $soundZip 'SHA256') $soundArchiveSha256 'JCOS source archive SHA-256'

            $soundArchive = [IO.Compression.ZipFile]::OpenRead($soundZip)
            try {
                foreach ($sound in $soundManifest) {
                    $suffix = "/JCOS/Resources/$($sound.name)"
                    $bytes = Get-ZipEntryBytes -Archive $soundArchive -EntryName $suffix -MatchSuffix
                    Assert-Equal $bytes.Length ([int]$sound.size) "$($sound.name) size"
                    Test-WaveBytes -Bytes $bytes -Name $sound.name
                    $destination = Join-Path $stageDirectory $sound.name
                    [IO.File]::WriteAllBytes($destination, $bytes)
                    Assert-Equal (Get-HashHex $destination 'MD5') $sound.md5.ToUpperInvariant() "$($sound.name) MD5"
                    Assert-Equal (Get-HashHex $destination 'SHA256') $sound.sha256.ToUpperInvariant() "$($sound.name) SHA-256"
                }
            } finally {
                $soundArchive.Dispose()
            }
        }

        $movedExisting = $false
        try {
            if (Test-Path -LiteralPath $TargetDirectory) {
                Move-Item -LiteralPath $TargetDirectory -Destination $backupDirectory
                $movedExisting = $true
            }
            Move-Item -LiteralPath $stageDirectory -Destination $TargetDirectory
            if ($movedExisting) {
                Remove-Item -LiteralPath $backupDirectory -Recurse -Force
            }
        } catch {
            if (!(Test-Path -LiteralPath $TargetDirectory) -and $movedExisting -and (Test-Path -LiteralPath $backupDirectory)) {
                Move-Item -LiteralPath $backupDirectory -Destination $TargetDirectory
            }
            throw
        }

        $soundStatus = if ($InstallSound) { 'with 23 verified sound effects' } else { 'without optional sound effects' }
        Write-Host "Johnny Castaway data installed $soundStatus in $TargetDirectory"
        if (!$Quiet) {
            Add-Type -AssemblyName System.Windows.Forms
            $newLine = [Environment]::NewLine
            [System.Windows.Forms.MessageBox]::Show(
                "Johnny Castaway data was installed $soundStatus.$newLine$newLine" +
                'Sounds can be added later from Data and Sound Setup in the Start menu.',
                'Johnny Castaway setup complete',
                [System.Windows.Forms.MessageBoxButtons]::OK,
                [System.Windows.Forms.MessageBoxIcon]::Information) | Out-Null
        }
    } finally {
        if (Test-Path -LiteralPath $workDirectory) {
            Remove-Item -LiteralPath $workDirectory -Recurse -Force
        }
        if (Test-Path -LiteralPath $stageDirectory) {
            Remove-Item -LiteralPath $stageDirectory -Recurse -Force
        }
    }
}

try {
    Install-JohnnyData
    exit 0
} catch {
    Write-Error $_
    if (!$Quiet) {
        Add-Type -AssemblyName System.Windows.Forms
        [System.Windows.Forms.MessageBox]::Show(
            $_.Exception.Message,
            'Johnny Castaway data setup failed',
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Error) | Out-Null
    }
    exit 1
}
