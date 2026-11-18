[CmdletBinding()]param([string]$Python='python',[string]$Mkisofs='',[string]$PspDev='D:/pspdev',[string]$Msys2='D:/msys64')
$ErrorActionPreference='Stop'
$env:PSP_TEMPLATE_DEV=$PspDev
$env:PSP_TEMPLATE_MSYS=$Msys2
. "$PSScriptRoot/psp-env.ps1"
function Run([string]$exe,[string[]]$arguments){ & $exe @arguments; if($LASTEXITCODE -ne 0){throw "Failed: $exe ($LASTEXITCODE)"} }
Push-Location $PSScriptRoot
try {
    if (!$Mkisofs){$cmd=Get-Command mkisofs -ErrorAction SilentlyContinue; $Mkisofs=if($cmd){$cmd.Source}else{'C:/Program Files (x86)/VMware/VMware Workstation/mkisofs.exe'}}
    if (!(Test-Path -LiteralPath $Mkisofs)){throw 'Specify -Mkisofs <absolute path to mkisofs.exe>'}
    $out=Join-Path $PSScriptRoot 'build/current';$stage=Join-Path $out 'disc';$obj=Join-Path $out 'objects'
    if(Test-Path -LiteralPath $out){$actual=(Resolve-Path -LiteralPath $out).Path; if($actual -ne (Join-Path $PSScriptRoot 'build/current' | ForEach-Object {[IO.Path]::GetFullPath($_)})){throw $actual}; Remove-Item -LiteralPath $actual -Recurse -Force}
    New-Item -ItemType Directory -Force $obj,"$stage/PSP_GAME/SYSDIR",dist | Out-Null
    Copy-Item runtime/atri-ui/*.c,runtime/atri-ui/*.h $obj
    Run $Python @('-B','tools/prepare.py',$stage,$obj,"$out/preview")
    Copy-Item runtime/*.c,runtime/*.h,runtime/Makefile $obj
    Run "$Msys2/usr/bin/make.exe" @('-C',$obj,'all')
    $cfg=Get-Content project/game.json -Encoding utf8 -Raw | ConvertFrom-Json
    Run "$PspDev/bin/mksfoex.exe" @('-s','CATEGORY=UG','-s',"DISC_ID=$($cfg.disc_id)",'-s','DISC_VERSION=1.00','-s','PSP_SYSTEM_VER=5.50','-d','BOOTABLE=1','-d','REGION=32768','-d','MEMSIZE=0',$cfg.title,"$stage/PSP_GAME/PARAM.SFO")
    foreach($name in @('BOOT.BIN','EBOOT.BIN')){Copy-Item -LiteralPath "$obj/reader.prx" -Destination "$stage/PSP_GAME/SYSDIR/$name"}
    $iso="$out/new.iso";Run $Mkisofs @('-iso-level','2','-omit-version-number','-A','PSP GAME','-sysid','PSP GAME','-V',$cfg.disc_id,'-o',$iso,$stage)
    Run $Python @('-B','tools/psp_iso.py',$iso);Run $Python @('-B','tools/verify.py',$iso,$stage)
    Move-Item -LiteralPath $iso -Destination "dist/$($cfg.disc_id).iso" -Force
    Get-Item -LiteralPath "dist/$($cfg.disc_id).iso" | Select-Object FullName,Length
}finally{Pop-Location}
