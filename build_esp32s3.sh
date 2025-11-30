#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE="${ESP32_IDF_IMAGE:-espressif/idf:release-v5.4}"
DOCKER_RUN_OPTS=(--rm --user root -v "${ROOT_DIR}:/workspace" -w /workspace)

if ! command -v docker >/dev/null 2>&1; then
  echo "[build_esp32s3] ❌ docker command not found. Please install Docker and try again." >&2
  exit 1
fi

if ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
  echo "[build_esp32s3] Pulling Docker image ${IMAGE}..."
  docker pull "${IMAGE}"
fi

echo "[build_esp32s3] Starting ESP32-S3 build using ${IMAGE}..."
docker run "${DOCKER_RUN_OPTS[@]}" "${IMAGE}" bash -c '
set -Eeuo pipefail
export TERM=xterm-256color
set -x
cd tools/esp32-compile-check
if ! command -v ninja >/dev/null 2>&1; then
  DEBIAN_FRONTEND=noninteractive apt-get update -yq
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ninja-build ccache
  apt-get clean
  rm -rf /var/lib/apt/lists/*
fi
bash /workspace/tools/make_esp32_symlinks.sh
./1_idf_clean.sh
./2_idf_build.sh
'

echo "[build_esp32s3] ✅ ESP32-S3 build finished successfully."
