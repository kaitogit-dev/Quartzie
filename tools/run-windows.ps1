$ErrorActionPreference = "Stop"
$qemu = Join-Path $env:ProgramFiles "qemu"
$executable = Join-Path $qemu "qemu-system-x86_64.exe"
$iso = Join-Path $PSScriptRoot "Quartzie-0.38.2-x64.iso"
if (!(Test-Path $iso)) { $iso = Join-Path $env:USERPROFILE "Downloads\Quartzie-0.38.2-x64.iso" }
if (!(Test-Path $executable)) { throw "QEMU not found: $executable" }
if (!(Test-Path $iso)) { throw "ISO not found: $iso" }
$firmware = Join-Path $qemu "share\edk2-x86_64-code.fd"
$variables = Join-Path $qemu "share\edk2-i386-vars.fd"
if (!(Test-Path $firmware) -or !(Test-Path $variables)) { throw "UEFI firmware not found in QEMU share directory" }
$variablesCopy = Join-Path $env:TEMP "quartzie-0.38.2-vars.fd"
Copy-Item $variables $variablesCopy -Force
$displays = (& $executable -display help 2>&1 | Out-String)
$display = if ($displays -match "(?m)^\s*gtk\s*$") { "gtk,zoom-to-fit=on" } else { "sdl,show-cursor=off" }
& $executable -machine q35 -m 512 -vga std -accel whpx -accel tcg -display $display -drive "if=pflash,format=raw,unit=0,readonly=on,file=$firmware" -drive "if=pflash,format=raw,unit=1,file=$variablesCopy" -cdrom $iso -boot order=d -usb -device usb-tablet -nic none
