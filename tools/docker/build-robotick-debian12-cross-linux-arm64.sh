#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="robotick-debian12-cross-linux-arm64"
LOCAL_TAG="${IMAGE_NAME}:local"
DOCKERFILE="${SCRIPT_DIR}/robotick-debian12-cross-linux-arm64.Dockerfile"

echo "Building ${LOCAL_TAG} from ${DOCKERFILE}"
docker build \
  -t "${LOCAL_TAG}" \
  -f "${DOCKERFILE}" \
  "${SCRIPT_DIR}"

echo "Built ${LOCAL_TAG}"
