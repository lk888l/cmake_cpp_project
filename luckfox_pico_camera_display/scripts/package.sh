#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$SOURCE_DIR/build/rv1103-release"}
STAGE_DIR=${STAGE_DIR:-"$SOURCE_DIR/build/package/camera-display"}
ARCHIVE=${ARCHIVE:-"$SOURCE_DIR/build/camera-display-rv1103.tar.gz"}

case "$STAGE_DIR" in
    "$SOURCE_DIR"/build/*) ;;
    *)
        echo "refusing staging directory outside $SOURCE_DIR/build: $STAGE_DIR" >&2
        exit 2
        ;;
esac
case "$ARCHIVE" in
    "$SOURCE_DIR"/build/*) ;;
    *)
        echo "refusing archive path outside $SOURCE_DIR/build: $ARCHIVE" >&2
        exit 2
        ;;
esac

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/bin" "$STAGE_DIR/licenses"
install -m 0755 "$BUILD_DIR/src/luckfox_pico_camera_display" "$STAGE_DIR/bin/"
if [ -n "${LUCKFOX_SDK_ROOT:-}" ]; then
    STRIP=$(find "$LUCKFOX_SDK_ROOT/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf" \
        -type f -name arm-rockchip830-linux-uclibcgnueabihf-strip | head -1)
    if [ -n "$STRIP" ]; then
        "$STRIP" --strip-unneeded \
            "$STAGE_DIR/bin/luckfox_pico_camera_display"
    fi
fi
install -m 0644 "$SOURCE_DIR/config/camera-display.ini" "$STAGE_DIR/"
install -m 0755 "$SOURCE_DIR/deploy/camera-displayctl" "$STAGE_DIR/"
install -m 0644 "$SOURCE_DIR/THIRD_PARTY_NOTICES_cn.md" "$STAGE_DIR/licenses/"
install -m 0644 "$SOURCE_DIR/THIRD_PARTY_NOTICES_en.md" "$STAGE_DIR/licenses/"
tar -C "$(dirname "$STAGE_DIR")" -czf "$ARCHIVE" "$(basename "$STAGE_DIR")"
echo "$ARCHIVE"
