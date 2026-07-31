#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SDK_ROOT=${LUCKFOX_SDK_ROOT:-"$HOME/src/luckfox-pico"}
MEDIA_ROOT=${LUCKFOX_MEDIA_ROOT:-"$SDK_ROOT/output/out/media_out"}
BUILD_DIR=${BUILD_DIR:-"$SOURCE_DIR/build/rv1103-release"}

test -d "$SDK_ROOT/.git"
test "$(git -c safe.directory="$SDK_ROOT" -C "$SDK_ROOT" rev-parse HEAD)" = \
    "824b817f889c2cbff1d48fcdb18ab494a68f69d1"

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$SOURCE_DIR/cmake/luckfox-rv1103.cmake" \
    -DLUCKFOX_SDK_ROOT="$SDK_ROOT" \
    -DLUCKFOX_MEDIA_ROOT="$MEDIA_ROOT" \
    -DCAMERA_DISPLAY_BUILD_APP=ON \
    -DCAMERA_DISPLAY_BUILD_TESTS=OFF
cmake --build "$BUILD_DIR" --parallel
