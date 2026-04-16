#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="robotick-ubuntu24.04-native-linux"
LOCAL_TAG="${IMAGE_NAME}:local"
DOCKERFILE="${SCRIPT_DIR}/robotick-ubuntu24.04-native-linux.Dockerfile"

"${SCRIPT_DIR}/build-robotick-ubuntu24.04-build-base.sh"

echo "Building ${LOCAL_TAG} from ${DOCKERFILE}"
docker build \
  -t "${LOCAL_TAG}" \
  -f "${DOCKERFILE}" \
  "${SCRIPT_DIR}"

echo "Built ${LOCAL_TAG}"
