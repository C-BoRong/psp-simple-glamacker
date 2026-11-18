[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$PspDevRoot = if ($env:PSP_TEMPLATE_DEV) { $env:PSP_TEMPLATE_DEV } else { "D:/pspdev" }
$Msys2Root = if ($env:PSP_TEMPLATE_MSYS) { $env:PSP_TEMPLATE_MSYS } else { "D:/msys64" }

foreach ($required in @(
    (Join-Path $PspDevRoot "bin\psp-gcc.exe"),
    (Join-Path $Msys2Root "usr\bin\make.exe")
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "PSP toolchain is incomplete; missing: $required"
    }
}

# Keep the Windows-native toolchain first so it is not shadowed by another SDK.
$env:PSPDEV = $PspDevRoot
$env:PSPSDK = "$PspDevRoot/psp/sdk"
$env:MSYS2_ROOT = $Msys2Root
$env:MSYSTEM = "MSYS"
$env:CHERE_INVOKING = "1"
$env:Path = @(
    (Join-Path $PspDevRoot "bin"),
    (Join-Path $Msys2Root "usr\bin"),
    $env:Path
) -join ";"

Write-Host "PSP environment ready"
Write-Host "  PSPDEV=$env:PSPDEV"
Write-Host "  PSPSDK=$env:PSPSDK"
Write-Host "  MSYS2=$env:MSYS2_ROOT"
Write-Host "  Compiler=$(& psp-gcc -dumpversion 2>$null)"

