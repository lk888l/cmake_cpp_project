#!/usr/bin/env bash
set -euo pipefail

BINARY=${1:?usage: verify-binary.sh path/to/luckfox_pico_camera_display}
SDK_ROOT=${LUCKFOX_SDK_ROOT:-"$HOME/src/luckfox-pico"}
TOOLCHAIN_ROOT="$SDK_ROOT/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf"
READELF=$(find "$TOOLCHAIN_ROOT" -type f \
    -name arm-rockchip830-linux-uclibcgnueabihf-readelf | head -1)

test -n "$READELF"
file "$BINARY"
HEADER=$("$READELF" -h "$BINARY")
PROGRAM=$("$READELF" -l "$BINARY")
DYNAMIC=$("$READELF" -d "$BINARY")

grep -q 'Class:.*ELF32' <<<"$HEADER"
grep -q 'Data:.*little endian' <<<"$HEADER"
grep -q 'Machine:.*ARM' <<<"$HEADER"
grep -q 'hard-float ABI' <<<"$HEADER"
grep -q '/lib/ld-uClibc.so.0' <<<"$PROGRAM"
for LIBRARY in librockit.so librkaiq.so librga.so; do
    grep -q "Shared library:.*\\[$LIBRARY\\]" <<<"$DYNAMIC"
done
if grep -Eq 'ld-linux-x86-64|/mnt/[a-z]/|C:|/opt/luckfox-sdk|/work/' \
    <<<"$PROGRAM$DYNAMIC"; then
    echo "unexpected host interpreter or path in binary" >&2
    exit 1
fi
echo "RV1103 uClibc ABI/dependency verification passed"
