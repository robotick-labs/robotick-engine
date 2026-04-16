#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE="${ROBOTICK_LINUX_ARM32_IMAGE:-ghcr.io/robotick-labs/robotick-debian12-cross-linux-arm32:latest}"
CONFIG_PRESET="robotick-engine-linux-arm32"

if ! command -v docker >/dev/null 2>&1; then
  echo "[build_linux_arm32] docker command not found." >&2
  exit 1
fi

if ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
  echo "[build_linux_arm32] Pulling Docker image ${IMAGE}..."
  docker pull "${IMAGE}"
fi

echo "[build_linux_arm32] Configuring and building preset '${CONFIG_PRESET}'..."
docker run --rm --init \
  --user root \
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

echo "[build_linux_arm32] ✅ Linux ARM32 cross-build complete."
