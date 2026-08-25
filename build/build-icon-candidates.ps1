[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$candidateDirectory = Join-Path $projectRoot 'assets\icons\candidates'
$ffmpeg = (Get-Command ffmpeg -ErrorAction Stop).Source
$rendererCandidates = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Microsoft\Edge\Application\msedge.exe'),
    (Join-Path $env:ProgramFiles 'Google\Chrome\Application\chrome.exe')
)
$renderer = $rendererCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (!$renderer) {
    throw 'Microsoft Edge or Google Chrome is required to render the SVG masters.'
}
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$candidates = @(
    @{ Name = 'castaway-lookout'; Label = 'Castaway Lookout' },
    @{ Name = 'palm-island'; Label = 'Palm Island' },
    @{ Name = 'jc-island-emblem'; Label = 'JC Island Emblem' }
)

$temporaryDirectory = Join-Path $candidateDirectory '.render-temp'
$resolvedCandidateDirectory = [System.IO.Path]::GetFullPath($candidateDirectory).TrimEnd([System.IO.Path]::DirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
$resolvedTemporaryDirectory = [System.IO.Path]::GetFullPath($temporaryDirectory)
if (!$resolvedTemporaryDirectory.StartsWith($resolvedCandidateDirectory, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Temporary directory is outside the candidate directory: $resolvedTemporaryDirectory"
}
if (Test-Path -LiteralPath $temporaryDirectory) {
    Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null

try {
    foreach ($candidate in $candidates) {
        $svg = Join-Path $candidateDirectory ($candidate.Name + '.svg')
        $preview = Join-Path $candidateDirectory ($candidate.Name + '.png')
        $icon = Join-Path $candidateDirectory ($candidate.Name + '.ico')
        if (!(Test-Path -LiteralPath $svg -PathType Leaf)) {
            throw "Missing SVG master: $svg"
        }

        $profile = Join-Path $temporaryDirectory ('browser-' + $candidate.Name)
        $svgUri = ([System.Uri]$svg).AbsoluteUri
        & $renderer '--headless=new' '--disable-gpu' '--disable-sync' '--no-first-run' '--hide-scrollbars' '--default-background-color=00000000' '--window-size=512,512' "--user-data-dir=$profile" "--screenshot=$preview" $svgUri | Out-Null
        if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $preview -PathType Leaf)) {
            throw "Failed to render $svg"
        }

        $filterOutputs = @()
        foreach ($size in $sizes) {
            $sizePreview = Join-Path $temporaryDirectory ($candidate.Name + "-$size.png")
            & $ffmpeg -y -hide_banner -loglevel error -i $preview -vf "scale=$size`:$size`:flags=lanczos,format=rgba" -frames:v 1 $sizePreview
            if ($LASTEXITCODE -ne 0) {
                throw "Failed to render $svg at ${size}x${size}"
            }
            $filterOutputs += $sizePreview
        }

        & $ffmpeg -y -hide_banner -loglevel error -i $filterOutputs[0] -i $filterOutputs[1] -i $filterOutputs[2] -i $filterOutputs[3] -i $filterOutputs[4] -i $filterOutputs[5] -i $filterOutputs[6] -map 0:v -map 1:v -map 2:v -map 3:v -map 4:v -map 5:v -map 6:v -c:v png $icon
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to create multi-resolution icon for $($candidate.Name)"
        }
    }

    Add-Type -AssemblyName System.Drawing
    $sheet = New-Object System.Drawing.Bitmap 1200, 620, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($sheet)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(245, 248, 250))
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $titleFont = New-Object System.Drawing.Font 'Segoe UI', 22, ([System.Drawing.FontStyle]::Bold)
        $labelFont = New-Object System.Drawing.Font 'Segoe UI', 17, ([System.Drawing.FontStyle]::Bold)
        $sizeFont = New-Object System.Drawing.Font 'Segoe UI', 10
        $ink = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(8, 48, 79))
        $card = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
        try {
            $graphics.DrawString('Johnny Castaway icon candidates', $titleFont, $ink, 28, 20)
            for ($index = 0; $index -lt $candidates.Count; $index++) {
                $candidate = $candidates[$index]
                $x = 25 + ($index * 390)
                $graphics.FillRectangle($card, $x, 75, 370, 515)
                $image = [System.Drawing.Image]::FromFile((Join-Path $candidateDirectory ($candidate.Name + '.png')))
                try {
                    $graphics.DrawImage($image, $x + 57, 100, 256, 256)
                    $graphics.DrawString($candidate.Label, $labelFont, $ink, $x + 24, 375)
                    $smallX = $x + 32
                    foreach ($size in @(64, 32, 16)) {
                        $small = [System.Drawing.Image]::FromFile((Join-Path $temporaryDirectory ($candidate.Name + "-$size.png")))
                        try {
                            $graphics.DrawImageUnscaled($small, $smallX, 445)
                        } finally {
                            $small.Dispose()
                        }
                        $graphics.DrawString("${size}px", $sizeFont, $ink, $smallX, 520)
                        $smallX += 112
                    }
                } finally {
                    $image.Dispose()
                }
            }
        } finally {
            $titleFont.Dispose()
            $labelFont.Dispose()
            $sizeFont.Dispose()
            $ink.Dispose()
            $card.Dispose()
        }
        $sheet.Save((Join-Path $candidateDirectory 'candidates-preview.png'), [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $sheet.Dispose()
    }

    $windres = 'C:\msys64\mingw64\bin\windres.exe'
    if (!(Test-Path -LiteralPath $windres -PathType Leaf)) {
        throw "windres is required to validate the ICO files: $windres"
    }
    $env:PATH = (Split-Path -Parent $windres) + [System.IO.Path]::PathSeparator + $env:PATH
    $resourceScript = Join-Path $projectRoot 'build\windows\icon-candidates-smoke.rc'
    $resourceOutput = Join-Path $temporaryDirectory 'icon-candidates-smoke.o'
    Push-Location (Split-Path -Parent $resourceScript)
    try {
        & $windres -i (Split-Path -Leaf $resourceScript) -O coff -o $resourceOutput
    } finally {
        Pop-Location
    }
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $resourceOutput -PathType Leaf)) {
        throw 'windres rejected one or more candidate ICO files.'
    }

    $innoCompiler = Join-Path $projectRoot '.tools\InnoSetup-6.7.3\ISCC.exe'
    if (!(Test-Path -LiteralPath $innoCompiler -PathType Leaf)) {
        throw "Run build\build-installer.ps1 once to prepare the Inno Setup compiler: $innoCompiler"
    }
    $innoScript = Join-Path $projectRoot 'build\windows\icon-candidate-smoke.iss'
    foreach ($candidate in $candidates) {
        $icon = (Join-Path $candidateDirectory ($candidate.Name + '.ico')).Replace('\', '/')
        & $innoCompiler "/DCandidateIcon=$icon" "/DCandidateName=$($candidate.Name)" $innoScript | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Inno Setup rejected $icon"
        }
    }
} finally {
    if (Test-Path -LiteralPath $temporaryDirectory) {
        Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
    }
}

Get-ChildItem -LiteralPath $candidateDirectory -File | Sort-Object Name | Select-Object Name, Length
