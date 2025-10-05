#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  echo "🛑 Please run as root (current uid: $(id -u))."
  exit 1
fi

export DEBIAN_FRONTEND=noninteractive
export APT_LISTCHANGES_FRONTEND=none

echo "🔧 Installing core build dependencies..."

apt-get update -yq && apt-get install -y --no-install-recommends \
  ninja-build \
  catch2 \
  libsdl2-dev \
  libssl-dev \
  libcurl4-openssl-dev \
  python3.12-dev

apt-get clean
rm -rf /var/lib/apt/lists/*

echo "✅ Core setup complete."
