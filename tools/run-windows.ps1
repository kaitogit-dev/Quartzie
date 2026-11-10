param(
    [ValidateSet("none","dsound")]
    [string]$AudioBackend = "none",
    [ValidateSet("iso","disk")]
    [string]$BootMode = "iso"
)
$ErrorActionPreference = "Stop"
$qemu = Join-Path $env:ProgramFiles "qemu"
$executable = Join-Path $qemu "qemu-system-x86_64.exe"
if (!(Test-Path $executable)) { throw "QEMU not found: $executable" }
$bundle = Split-Path -Parent $PSScriptRoot
if (!(Test-Path (Join-Path $bundle "Quartzie-0.45.0-x64.iso"))) { $bundle = $PSScriptRoot }
$iso = Join-Path $bundle "Quartzie-0.45.0-x64.iso"
if (!(Test-Path $iso)) { $iso = Join-Path $env:USERPROFILE "Downloads\Quartzie-0.45.0-x64.iso" }
$systemDisk = Join-Path $bundle "Quartzie-0.45.0-x64.img"
if (!(Test-Path $systemDisk)) { $systemDisk = Join-Path $env:USERPROFILE "Downloads\Quartzie-0.45.0-x64.img" }
if ($BootMode -eq "iso" -and !(Test-Path $iso)) { throw "ISO not found: $iso" }
if ($BootMode -eq "disk" -and !(Test-Path $systemDisk)) { throw "System image not found: $systemDisk" }
$firmware = Join-Path $qemu "share\edk2-x86_64-code.fd"
$variables = Join-Path $qemu "share\edk2-i386-vars.fd"
if (!(Test-Path $firmware) -or !(Test-Path $variables)) { throw "UEFI firmware not found in QEMU share directory" }
$userdisk = Join-Path $bundle "Quartzie-0.45.0-userdata.img"
if (!(Test-Path $userdisk)) { $userdisk = Join-Path $env:USERPROFILE "Downloads\Quartzie-0.45.0-userdata.img" }
if (!(Test-Path $userdisk)) { throw "Persistent data disk not found: $userdisk" }
$variablesCopy = Join-Path $env:TEMP "quartzie-0.45.0-vars.fd"
Copy-Item $variables $variablesCopy -Force
$displays = (& $executable -display help 2>&1 | Out-String)
$display = if ($displays -match "(?m)^\s*gtk\s*$") { "gtk,zoom-to-fit=on,show-cursor=off" } else { "sdl,show-cursor=off" }
$bootArgs = if ($BootMode -eq "disk") {
    @("-drive","if=none,id=quartzie-system,format=raw,file=$systemDisk","-device","usb-storage,drive=quartzie-system,bootindex=1")
} else {
    @("-cdrom",$iso,"-boot","order=d")
}
& $executable -machine "q35,pcspk-audiodev=quartzie" -audiodev "$AudioBackend,id=quartzie" -m 512 -vga std -accel whpx -accel tcg -display $display -drive "if=pflash,format=raw,unit=0,readonly=on,file=$firmware" -drive "if=pflash,format=raw,unit=1,file=$variablesCopy" -usb -device usb-tablet @bootArgs -drive "if=none,id=quartzie-user,format=raw,file=$userdisk" -device usb-storage,drive=quartzie-user -nic user,model=e1000
