#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE="${ROBOTICK_NATIVE_LINUX_IMAGE:-ghcr.io/robotick-labs/robotick-ubuntu24.04-native-linux:latest}"
CONFIG_PRESET="robotick-engine-tests-linux-release"

if ! command -v docker >/dev/null 2>&1; then
  echo "[build_linux_release] docker command not found." >&2
  exit 1
fi

if ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
  echo "[build_linux_release] Pulling Docker image ${IMAGE}..."
  docker pull "${IMAGE}"
fi

echo "[build_linux_release] Configuring and building preset '${CONFIG_PRESET}' inside ${IMAGE}..."
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

echo "[build_linux_release] ✅ Linux release build complete."
