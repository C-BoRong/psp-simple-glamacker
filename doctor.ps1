[CmdletBinding()]param([string]$Python='python',[string]$PspDev='D:/pspdev',[string]$Msys2='D:/msys64',[string]$Mkisofs='')
$missing = [Collections.Generic.List[string]]::new()
function Check($label,$ok,$detail,$required=$true){
    $status=if($ok){'OK'}elseif($required){'MISSING'}else{'OPTIONAL'}
    if (!$ok -and $required){$missing.Add($label)}
    Write-Output "[$status] $label : $detail"
}
Check Windows ($env:OS -eq 'Windows_NT') 'Windows-native toolchain'
Check Git ([bool](Get-Command git -ErrorAction SilentlyContinue)) 'Required for project history'
foreach ($path in @("$PspDev/bin/psp-gcc.exe","$PspDev/bin/psp-prxgen.exe","$PspDev/bin/mksfoex.exe","$PspDev/psp/sdk/lib/build.mak","$Msys2/usr/bin/make.exe")) { Check Tool (Test-Path -LiteralPath $path -PathType Leaf) $path }
if(Test-Path "$PspDev/bin/psp-gcc.exe"){$target=& "$PspDev/bin/psp-gcc.exe" -dumpmachine;Check Compiler ($LASTEXITCODE -eq 0 -and $target -like '*psp*') $target}
if(!$Mkisofs){$m=Get-Command mkisofs -ErrorAction SilentlyContinue;$Mkisofs=if($m){$m.Source}else{'C:/Program Files (x86)/VMware/VMware Workstation/mkisofs.exe'}}
Check Mkisofs (Test-Path -LiteralPath $Mkisofs -PathType Leaf) $Mkisofs
$py=Get-Command $Python -ErrorAction SilentlyContinue
if($py){$v=& $py.Source -B -c 'import sys,PIL,fontTools; print(sys.version.split()[0],PIL.__version__,fontTools.__version__);sys.exit(sys.version_info<(3,10))';Check 'Python >=3.10 / Pillow / fontTools' ($LASTEXITCODE -eq 0) "$v"}else{Check Python $false $Python}
Check Font (Test-Path -LiteralPath "$PSScriptRoot/fonts/sourcehansans-bold.otf" -PathType Leaf) 'Source Han Sans Bold'
Check FFmpeg ([bool](Get-Command ffmpeg -ErrorAction SilentlyContinue)) 'Optional for game BGM' $false
Write-Output ('Project drive free: {0:N1} GiB' -f ((Get-Item -LiteralPath $PSScriptRoot).PSDrive.Free/1GB))
if($missing.Count){exit 1};exit 0
