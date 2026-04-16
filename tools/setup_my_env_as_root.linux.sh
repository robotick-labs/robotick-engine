#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  echo "🛑 Please run as root (current uid: $(id -u))."
  exit 1
fi

export DEBIAN_FRONTEND=noninteractive
export APT_LISTCHANGES_FRONTEND=none

echo "🔧 Installing core build dependencies..."

retry_apt_update() {
  local attempt=1
  local max_attempts=3

  while (( attempt <= max_attempts )); do
    rm -rf /var/lib/apt/lists/*

    if apt-get update -yq -o Acquire::Retries=3 -o APT::Update::Error-Mode=any; then
      return 0
    fi

    if (( attempt == max_attempts )); then
      echo "🛑 apt-get update failed after ${max_attempts} attempts."
      return 1
    fi

    echo "⚠️ apt-get update failed on attempt ${attempt}/${max_attempts}; retrying..."
    sleep 5
    ((attempt++))
  done
}

retry_apt_update

apt-get install -y --no-install-recommends -o Acquire::Retries=3 \
  ninja-build \
  catch2 \
  libsdl2-dev \
  libssl-dev \
  libcurl4-openssl-dev \
  python3.12-dev

apt-get clean
rm -rf /var/lib/apt/lists/*

echo "✅ Core setup complete."
