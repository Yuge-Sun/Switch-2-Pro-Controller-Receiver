#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
mbed="${IDF_PATH:?Set IDF_PATH to ESP-IDF 5.4.2}/components/mbedtls/mbedtls"
mkdir -p "$repo/work/b/tests/pairing"
for tree in firmware source/final-three-in-one/firmware; do
  ble="$repo/$tree/esp32s3_switch2_bridge/main/ble"
  cc -std=c11 -Wall -Wextra -Werror '-DMBEDTLS_CONFIG_FILE="pairing_mbedtls_config.h"' \
    -I"$repo/tools/tests" -I"$ble" -I"$mbed/include" \
    "$repo/tools/tests/switch2_pairing_test.c" "$ble/switch2_pairing.c" \
    "$mbed/library/aes.c" "$mbed/library/platform_util.c" "$mbed/library/platform.c" \
    -o "$repo/work/b/tests/pairing/pairing_test"
  "$repo/work/b/tests/pairing/pairing_test"
done
