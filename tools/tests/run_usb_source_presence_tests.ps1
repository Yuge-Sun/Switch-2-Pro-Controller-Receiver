[CmdletBinding()]
param([string]$Compiler = "cc", [switch]$Zig)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$buildDir = Join-Path $repoRoot "work\b\tests\usb_source_presence"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$prefix = @()
if ($Zig) { $prefix = @("cc") }
foreach ($tree in @("firmware", "source/final-three-in-one/firmware")) {
    $include = Join-Path $repoRoot "$tree/esp32s3_dualsense_identity_experiment/main"
    $exe = Join-Path $buildDir "usb_source_presence_test.exe"
    & $Compiler @prefix -std=c11 -Wall -Wextra -Werror "-I$include" (Join-Path $PSScriptRoot "usb_source_presence_test.c") -o $exe
    if ($LASTEXITCODE -ne 0) { throw "USB source presence compilation failed: $tree" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "USB source presence tests failed: $tree" }
}
