$ErrorActionPreference = "Stop"
$qemu = Join-Path $env:ProgramFiles "qemu"
$executable = Join-Path $qemu "qemu-system-x86_64.exe"
$iso = Join-Path $PSScriptRoot "Quartzie-0.38.6-x64.iso"
if (!(Test-Path $iso)) { $iso = Join-Path $env:USERPROFILE "Downloads\Quartzie-0.38.6-x64.iso" }
if (!(Test-Path $executable)) { throw "QEMU not found: $executable" }
if (!(Test-Path $iso)) { throw "ISO not found: $iso" }
$firmware = Join-Path $qemu "share\edk2-x86_64-code.fd"
$variables = Join-Path $qemu "share\edk2-i386-vars.fd"
if (!(Test-Path $firmware) -or !(Test-Path $variables)) { throw "UEFI firmware not found in QEMU share directory" }
$variablesCopy = Join-Path $env:TEMP "quartzie-0.38.6-vars.fd"
Copy-Item $variables $variablesCopy -Force
$displays = (& $executable -display help 2>&1 | Out-String)
$display = if ($displays -match "(?m)^\s*gtk\s*$") { "gtk,zoom-to-fit=on,show-cursor=on" } else { "sdl,show-cursor=on" }
$port = Get-Random -Minimum 40000 -Maximum 55000
$qmp = "tcp:127.0.0.1:$port,server=on,wait=off"
$arguments = @(
    "-machine", "q35", "-m", "512", "-vga", "std",
    "-accel", "whpx", "-accel", "tcg",
    "-display", $display,
    "-qmp", $qmp,
    "-fw_cfg", "name=opt/quartzie/hostcursor,string=1",
    "-drive", "`"if=pflash,format=raw,unit=0,readonly=on,file=$firmware`"",
    "-drive", "`"if=pflash,format=raw,unit=1,file=$variablesCopy`"",
    "-cdrom", "`"$iso`"", "-boot", "order=d",
    "-usb", "-device", "usb-tablet", "-nic", "none"
)

function Read-Qmp($reader) {
    for ($i = 0; $i -lt 64; $i++) {
        $line = $reader.ReadLine()
        if ($null -eq $line) { throw "QEMU disconnected from the monitor." }
        $result = ConvertFrom-Json -InputObject $line
        if ($result.PSObject.Properties.Name -contains "error") {
            throw "QEMU monitor error: $line"
        }
        if ($result.PSObject.Properties.Name -contains "return") { return $result }
    }
    throw "No monitor response from QEMU."
}

$process = Start-Process -FilePath $executable -ArgumentList $arguments -PassThru
$client = $null
try {
    for ($attempt = 0; $attempt -lt 60; $attempt++) {
        if ($process.HasExited) { throw "QEMU exited before the mouse was configured." }
        $trial = New-Object System.Net.Sockets.TcpClient
        try {
            $trial.Connect("127.0.0.1", $port)
            $client = $trial
            break
        } catch {
            $trial.Dispose()
            Start-Sleep -Milliseconds 100
        }
    }
    if ($null -eq $client) { throw "Could not connect to the QEMU mouse controller." }
    $client.ReceiveTimeout = 5000
    $client.SendTimeout = 5000
    $reader = New-Object System.IO.StreamReader($client.GetStream())
    $writer = New-Object System.IO.StreamWriter($client.GetStream())
    $writer.AutoFlush = $true
    $null = $reader.ReadLine()
    $writer.WriteLine('{"execute":"qmp_capabilities"}')
    $null = Read-Qmp $reader
    $writer.WriteLine('{"execute":"query-mice"}')
    $mice = (Read-Qmp $reader).return
    $tablet = $mice | Where-Object { $_.absolute } | Select-Object -First 1
    if ($null -eq $tablet) { throw "QEMU did not expose an absolute USB tablet." }
    $selection = @{ execute = "human-monitor-command"; arguments = @{ "command-line" = "mouse_set $($tablet.index)" } } | ConvertTo-Json -Compress -Depth 5
    $writer.WriteLine($selection)
    $null = Read-Qmp $reader
    $writer.WriteLine('{"execute":"query-mice"}')
    $current = (Read-Qmp $reader).return | Where-Object { $_.current } | Select-Object -First 1
    if ($null -eq $current -or !$current.absolute) { throw "QEMU could not activate absolute tablet input." }
    Write-Host "Quartzie pointer: USB tablet selected. Desktop coordinates are absolute."
    $client.Dispose()
    $client = $null
    $process.WaitForExit()
} finally {
    if ($null -ne $client) { $client.Dispose() }
}
