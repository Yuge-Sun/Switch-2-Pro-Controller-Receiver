[CmdletBinding()]
param([string]$Compiler = 'cc', [switch]$Zig, [string]$IdfPath)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$IdfPath) { $IdfPath = Join-Path $repo '.toolchain/esp-idf-v5.4.2' }
$mbed = Join-Path $IdfPath 'components/mbedtls/mbedtls'
$output = Join-Path $repo 'work/b/tests/pairing'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$prefix = @()
if ($Zig) { $prefix = @('cc') }
foreach ($tree in @('firmware', 'source/final-three-in-one/firmware')) {
    $ble = Join-Path $repo "$tree/esp32s3_switch2_bridge/main/ble"
    $exe = Join-Path $output 'pairing_test.exe'
    & $Compiler @prefix -std=c11 -Wall -Wextra -Werror '-DMBEDTLS_CONFIG_FILE="pairing_mbedtls_config.h"' `
        "-I$PSScriptRoot" "-I$ble" "-I$mbed/include" `
        (Join-Path $PSScriptRoot 'switch2_pairing_test.c') (Join-Path $ble 'switch2_pairing.c') `
        "$mbed/library/aes.c" "$mbed/library/platform_util.c" "$mbed/library/platform.c" -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Pairing tests failed to compile: $tree" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "Pairing tests failed: $tree" }
}
