#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="robotick-idf5.4-esp32"
LOCAL_TAG="${IMAGE_NAME}:local"
REMOTE_NAME="ghcr.io/robotick-labs/${IMAGE_NAME}"
SMOKE_TEST='
  idf.py --version
  ninja --version
  ccache --version
  git --version
  bash --version | head -n 1
'

IMAGE_NAME="${IMAGE_NAME}" \
LOCAL_TAG="${LOCAL_TAG}" \
REMOTE_NAME="${REMOTE_NAME}" \
SMOKE_TEST="${SMOKE_TEST}" \
exec "${SCRIPT_DIR}/check-and-push-image.sh" "$@"
