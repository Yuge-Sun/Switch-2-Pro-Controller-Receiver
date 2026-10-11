[CmdletBinding()]
param(
    [string]$Port = "COM3",
    [string]$LogPath = (Join-Path $PWD ("reconnect-{0}.log" -f (Get-Date -Format "yyyyMMdd-HHmmss")))
)

$ErrorActionPreference = "Stop"
$serial = [System.IO.Ports.SerialPort]::new($Port, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$serial.Handshake = [System.IO.Ports.Handshake]::None
$serial.DtrEnable = $false
$serial.RtsEnable = $false
$serial.WriteTimeout = 1000
$serial.ReadBufferSize = 65536
$serial.NewLine = "`n"
$writer = $null
$fullPath = [IO.Path]::GetFullPath($LogPath)
$markers = @{
    '1' = 'About to enter controller pairing mode'
    '2' = 'Controller connected; buttons and sticks verified'
    '3' = 'About to turn controller off'
    '4' = 'About to wake controller normally WITHOUT pairing mode'
    '5' = 'Normal wake has failed after waiting'
    '6' = 'About to retry using controller pairing mode'
    'B' = 'About to reset receiver using EN/RESET'
}

function Write-Marker([string]$Message) {
    $line = "`r`n[HOST {0}] {1}`r`n" -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff zzz'), $Message
    [Console]::Write($line)
    $writer.Write($line)
    $writer.Flush()
}

try {
    # CreateNew prevents accidentally overwriting an earlier capture.
    $stream = [IO.File]::Open($fullPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
    $writer = [IO.StreamWriter]::new($stream, [Text.UTF8Encoding]::new($false))
    $writer.AutoFlush = $true
    $serial.Open()
    Write-Marker "Capture started: port=$Port baud=115200 DTR=false RTS=false"
    Write-Host "Saving to: $fullPath"
    Write-Host 'Keys (no Enter needed): 1=pair, 2=working, 3=off, 4=normal wake, 5=failed, 6=pairing retry'
    Write-Host 'B=receiver reset marker, S=read-only diagnostic status, Q=stop and save'
    Write-Host 'Use a regular PowerShell terminal, not PowerShell ISE. Leave both USB cables connected.'

    while ($true) {
        $chunk = $serial.ReadExisting()
        if ($chunk.Length -gt 0) {
            $writer.Write($chunk)
            [Console]::Write($chunk)
        }
        if ([Console]::KeyAvailable) {
            $key = [Console]::ReadKey($true).KeyChar.ToString().ToUpperInvariant()
            if ($key -eq 'Q') { break }
            if ($markers.ContainsKey($key)) { Write-Marker $markers[$key] }
            if ($key -eq 'S') {
                Write-Marker 'Requesting read-only status diag'
                $serial.WriteLine('status diag')
            }
        }
        Start-Sleep -Milliseconds 20
    }
    $tail = $serial.ReadExisting()
    if ($tail.Length -gt 0) { $writer.Write($tail) }
    Write-Marker 'Capture finished'
}
finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
    if ($null -ne $writer) { $writer.Dispose() }
    Write-Host "Log file: $fullPath"
}
