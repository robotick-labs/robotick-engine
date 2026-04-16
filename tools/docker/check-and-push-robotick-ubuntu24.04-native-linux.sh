#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="robotick-ubuntu24.04-native-linux"
LOCAL_TAG="${IMAGE_NAME}:local"
REMOTE_NAME="ghcr.io/robotick-labs/${IMAGE_NAME}"
SMOKE_TEST='
  cmake --version
  ninja --version
  pkg-config --modversion sdl2
  pkg-config --modversion opencv4
  python3 --version
  test -f /usr/include/yaml-cpp/yaml.h
'

IMAGE_NAME="${IMAGE_NAME}" \
LOCAL_TAG="${LOCAL_TAG}" \
REMOTE_NAME="${REMOTE_NAME}" \
SMOKE_TEST="${SMOKE_TEST}" \
exec "${SCRIPT_DIR}/check-and-push-image.sh" "$@"
