#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="robotick-ubuntu24.04-build-base"
LOCAL_TAG="${IMAGE_NAME}:local"
REMOTE_NAME="ghcr.io/robotick-labs/${IMAGE_NAME}"
SMOKE_TEST='
  cmake --version
  ninja --version
  ccache --version
  python3 --version
  git --version
'

IMAGE_NAME="${IMAGE_NAME}" \
LOCAL_TAG="${LOCAL_TAG}" \
REMOTE_NAME="${REMOTE_NAME}" \
SMOKE_TEST="${SMOKE_TEST}" \
exec "${SCRIPT_DIR}/check-and-push-image.sh" "$@"
