#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE="${ROBOTICK_NATIVE_LINUX_IMAGE:-ghcr.io/robotick-labs/robotick-ubuntu24.04-native-linux:latest}"
CONFIG_PRESET="robotick-engine-tests-linux-debug"

if ! command -v docker >/dev/null 2>&1; then
  echo "[build_linux_debug] docker command not found." >&2
  exit 1
fi

if [[ "${IMAGE}" == *":latest" ]]; then
  echo "[build_linux_debug] Refreshing Docker image ${IMAGE}..."
  docker pull "${IMAGE}"
elif ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
  echo "[build_linux_debug] Pulling Docker image ${IMAGE}..."
  docker pull "${IMAGE}"
fi

echo "[build_linux_debug] Configuring, building, and testing preset '${CONFIG_PRESET}' inside ${IMAGE}..."

if [[ -e "${ROOT_DIR}/build/${CONFIG_PRESET}" ]]; then
  echo "[build_linux_debug] Repairing ownership of existing build/${CONFIG_PRESET}..."
  docker run --rm --init \
    -v "${ROOT_DIR}:/workspace" \
    -w /workspace \
    "${IMAGE}" \
    bash -lc "
      set -Eeuo pipefail
      chown -R $(id -u):$(id -g) build/${CONFIG_PRESET}
    "
fi

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
    ctest --test-dir build/${CONFIG_PRESET}/cpp/tests --output-on-failure --timeout 5 --fail-if-no-tests -j\$(nproc)
  "

echo "[build_linux_debug] ✅ Linux debug build + tests complete."
