[CmdletBinding()]
param([string]$IdfPath, [switch]$Regular)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'idf_environment.ps1')
$localPythonEnv = Get-Y700LocalPythonEnvironment
if ($localPythonEnv) {
    $env:PATH = "$(Join-Path $localPythonEnv 'Scripts');$env:PATH"
}
Import-Y700IdfEnvironment -IdfPath $IdfPath
$shortRoot = Get-Y700ShortRepoRoot
$project = Join-Path $shortRoot 'source/final-three-in-one/firmware/esp32s3_dualsense_identity_experiment'
$build = Join-Path $shortRoot 'work/b/pairing-diagnostic'
$output = Join-Path $shortRoot 'work/firmware/pairing-diagnostic-v2'
$stem = 'ps5-pairing-diagnostic'
$diagnostics = 'ON'
if ($Regular) {
    $output = Join-Path $shortRoot 'work/firmware/idle-10min-1sec'
    $stem = 'ps5-idle-10min-1sec'
    $diagnostics = 'OFF'
}
$config = Join-Path $build 'sdkconfig'
New-Item -ItemType Directory -Force -Path $build, $output | Out-Null
$argsIdf = @('-C', $project, '-B', $build, '-D', "SDKCONFIG=$config",
    '-D', 'IDF_TARGET=esp32s3',
    '-D', 'V5_5_DS5_PROFILE=hid_audio_uac1_4ch_dualsense',
    '-D', "PRO2_PAIRING_DIAGNOSTICS=$diagnostics")
& idf.py @argsIdf build
if ($LASTEXITCODE -ne 0) { throw 'Diagnostic firmware build failed.' }

$python = Get-Y700IdfPython
Push-Location $build
try {
    & $python -m esptool --chip esp32s3 merge_bin -o (Join-Path $output "$stem.bin") '@flash_args'
    if ($LASTEXITCODE -ne 0) { throw 'Diagnostic firmware merge failed.' }
    Copy-Item -LiteralPath 'esp32s3_dualsense_identity_experiment.bin' -Destination (Join-Path $output "$stem-app.bin")
    Copy-Item -LiteralPath 'flasher_args.json' -Destination $output
} finally {
    Pop-Location
}
$hashes = Get-ChildItem -LiteralPath $output -Filter '*.bin' | ForEach-Object {
    '{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $_.Name
}
$hashes | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding ascii
Write-Host "Firmware (diagnostics=$diagnostics): $output"
