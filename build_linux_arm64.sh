#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE="${ROBOTICK_LINUX_ARM64_IMAGE:-ghcr.io/robotick-labs/robotick-debian12-cross-linux-arm64:latest}"
CONFIG_PRESET="robotick-engine-linux-arm64"

if ! command -v docker >/dev/null 2>&1; then
  echo "[build_linux_arm64] docker command not found." >&2
  exit 1
fi

if [[ "${IMAGE}" == *":latest" ]]; then
  echo "[build_linux_arm64] Refreshing Docker image ${IMAGE}..."
  docker pull "${IMAGE}"
elif ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
  echo "[build_linux_arm64] Pulling Docker image ${IMAGE}..."
  docker pull "${IMAGE}"
fi

echo "[build_linux_arm64] Configuring and building preset '${CONFIG_PRESET}'..."
docker run --rm --init \
  --user "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -v "${ROOT_DIR}:/workspace" \
  -w /workspace \
  "${IMAGE}" \
  bash -lc "
    set -Eeuo pipefail
    mkdir -p .cmake
    rm -rf build/${CONFIG_PRESET}
    cmake --preset ${CONFIG_PRESET}
    cmake --build --preset ${CONFIG_PRESET} -j\$(nproc)
  "

echo "[build_linux_arm64] ✅ Linux ARM64 cross-build complete."
