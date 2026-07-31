#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SDK_ROOT=${LUCKFOX_SDK_ROOT:-"$HOME/src/luckfox-pico"}
IMAGE=${CAMERA_DISPLAY_BUILD_IMAGE:-luckfox-camera-display:ubuntu22}

docker build -t "$IMAGE" -f "$SOURCE_DIR/containers/ubuntu22.Dockerfile" "$SOURCE_DIR"
docker run --rm \
    -v "$SOURCE_DIR:/work/source" \
    -v "$SDK_ROOT:/opt/luckfox-sdk" \
    "$IMAGE" \
    bash -lc '
      export LUCKFOX_SDK_ROOT=/opt/luckfox-sdk
      export LUCKFOX_MEDIA_ROOT=/opt/luckfox-sdk/output/out/media_out
      /work/source/scripts/build-sdk-media.sh
      BUILD_DIR=/work/source/build/rv1103-container \
        /work/source/scripts/build-app.sh
      /work/source/scripts/verify-binary.sh \
        /work/source/build/rv1103-container/src/luckfox_pico_camera_display
      BUILD_DIR=/work/source/build/rv1103-container \
        /work/source/scripts/package.sh
    '
