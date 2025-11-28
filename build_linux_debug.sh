#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

CONFIG_PRESET="robotick-engine-tests-linux-debug"

if [ ! -d ".cmake" ]; then
  mkdir -p .cmake >/dev/null 2>&1 || true
fi

echo "[build_linux_debug] Configuring CMake preset '${CONFIG_PRESET}'..."
cmake --preset "${CONFIG_PRESET}"

echo "[build_linux_debug] Building preset '${CONFIG_PRESET}'..."
cmake --build --preset "${CONFIG_PRESET}" -j"$(nproc)"

echo "[build_linux_debug] Running unit tests..."
ctest --test-dir "build/${CONFIG_PRESET}/cpp/tests" --output-on-failure --timeout 120 --fail-if-no-tests -j"$(nproc)"

echo "[build_linux_debug] ✅ Linux debug build + tests complete."
