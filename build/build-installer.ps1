[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$innoVersion = '6.7.3'
$innoInstallerName = "innosetup-$innoVersion.exe"
$innoInstallerUri = "https://github.com/jrsoftware/issrc/releases/download/is-6_7_3/$innoInstallerName"
$innoInstallerSha256 = '9C73C3BAE7ED48D44112A0F48E66742C00090BDB5BEF71D9D3C056C66E97B732'
$toolCache = Join-Path $projectRoot '.tools'
$innoRoot = Join-Path $toolCache "InnoSetup-$innoVersion"
$compiler = Join-Path $innoRoot 'ISCC.exe'
$installerScript = Join-Path $projectRoot 'installer\JohnnyCastaway.iss'
$settingsArtifact = Join-Path $projectRoot 'installer\JohnnyCastaway.ini'
$iconArtifact = Join-Path $projectRoot 'assets\icons\candidates\castaway-lookout.ico'
$appArtifact = Join-Path $PSScriptRoot 'JohnnyCastaway.exe'
$screensaverArtifact = Join-Path $PSScriptRoot 'JohnnyCastaway.scr'

foreach ($artifact in @($appArtifact, $screensaverArtifact, $installerScript, $settingsArtifact, $iconArtifact)) {
    if (!(Test-Path -LiteralPath $artifact -PathType Leaf)) {
        throw "Required installer input is missing: $artifact"
    }
}

if (!(Test-Path -LiteralPath $compiler -PathType Leaf)) {
    New-Item -ItemType Directory -Path $toolCache, $innoRoot -Force | Out-Null
    $bootstrap = Join-Path $toolCache $innoInstallerName
    if (!(Test-Path -LiteralPath $bootstrap -PathType Leaf)) {
        Write-Host "Downloading official Inno Setup $innoVersion compiler..."
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $innoInstallerUri -OutFile $bootstrap -UseBasicParsing
    }

    $hash = (Get-FileHash -LiteralPath $bootstrap -Algorithm SHA256).Hash
    if ($hash -ne $innoInstallerSha256) {
        throw "Inno Setup bootstrap SHA-256 is $hash; expected $innoInstallerSha256"
    }
    $signature = Get-AuthenticodeSignature -LiteralPath $bootstrap
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Pyrsys B\.V\.') {
        throw "Inno Setup bootstrap signature is not the expected valid Pyrsys B.V. signature: $($signature.Status) / $($signature.SignerCertificate.Subject)"
    }

    Write-Host 'Preparing the project-local portable Inno Setup compiler...'
    $arguments = @(
        '/PORTABLE=1',
        '/VERYSILENT',
        '/SUPPRESSMSGBOXES',
        '/NORESTART',
        '/CURRENTUSER',
        "/DIR=$innoRoot"
    )
    $process = Start-Process -FilePath $bootstrap -ArgumentList $arguments -Wait -PassThru -WindowStyle Hidden
    if ($process.ExitCode -ne 0 -or !(Test-Path -LiteralPath $compiler -PathType Leaf)) {
        throw "Portable Inno Setup preparation failed with exit code $($process.ExitCode)"
    }
}

Write-Host 'Compiling JohnnyCastaway-Windows-11-x64-Setup.exe...'
& $compiler $installerScript
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup compiler failed with exit code $LASTEXITCODE"
}

$output = Join-Path $PSScriptRoot 'installer\JohnnyCastaway-Windows-11-x64-Setup.exe'
if (!(Test-Path -LiteralPath $output -PathType Leaf)) {
    throw "Installer compiler did not create $output"
}

$item = Get-Item -LiteralPath $output
$sha256 = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
Write-Host "Created: $($item.FullName)"
Write-Host "Size:    $($item.Length) bytes"
Write-Host "SHA-256: $sha256"
