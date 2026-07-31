#!/usr/bin/env bash
set -euo pipefail

SDK_ROOT=${LUCKFOX_SDK_ROOT:-"$HOME/src/luckfox-pico"}
PINNED_COMMIT=824b817f889c2cbff1d48fcdb18ab494a68f69d1
BOARD_CONFIG=project/cfg/BoardConfig_IPC/BoardConfig-SD_CARD-Buildroot-RV1103_Luckfox_Pico-IPC.mk

test -d "$SDK_ROOT/.git"
test "$(git -c safe.directory="$SDK_ROOT" -C "$SDK_ROOT" rev-parse HEAD)" = \
    "$PINNED_COMMIT"
test -f "$SDK_ROOT/$BOARD_CONFIG"

ln -sfn "$BOARD_CONFIG" "$SDK_ROOT/.BoardConfig.mk"
(
    cd "$SDK_ROOT"
    ./build.sh media
)

test -f "$SDK_ROOT/output/out/media_out/include/rk_mpi_vi.h"
find "$SDK_ROOT/output/out/media_out" -name 'librockit.so*' -print -quit |
    grep -q .
find "$SDK_ROOT/output/out/media_out" -name 'librkaiq.so*' -print -quit |
    grep -q .
find "$SDK_ROOT/output/out/media_out" -name 'librga.so*' -print -quit |
    grep -q .
echo "SDK media output is ready: $SDK_ROOT/output/out/media_out"
