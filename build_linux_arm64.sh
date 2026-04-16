#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE="robotick-debian12-cross-linux-arm64:local"
DOCKERFILE="${ROOT_DIR}/tools/docker/robotick-debian12-cross-linux-arm64.Dockerfile"
CONFIG_PRESET="robotick-engine-linux-arm64"

if ! command -v docker >/dev/null 2>&1; then
  echo "[build_linux_arm64] docker command not found." >&2
  exit 1
fi

echo "[build_linux_arm64] Building Docker image ${IMAGE}..."
docker build -t "${IMAGE}" -f "${DOCKERFILE}" "${ROOT_DIR}/tools/docker"

echo "[build_linux_arm64] Configuring and building preset '${CONFIG_PRESET}'..."
docker run --rm --init \
  --user root \
  -v "${ROOT_DIR}:/workspace" \
  -w /workspace \
  "${IMAGE}" \
  bash -lc "
    set -Eeuo pipefail
    mkdir -p .cmake
    cmake --preset ${CONFIG_PRESET}
    cmake --build --preset ${CONFIG_PRESET} -j\$(nproc)
  "

echo "[build_linux_arm64] ✅ Linux ARM64 cross-build complete."
