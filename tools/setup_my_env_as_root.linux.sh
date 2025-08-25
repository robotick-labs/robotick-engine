#!/usr/bin/env bash
set -e

echo "🔧 Installing core build dependencies..."

apt-get update && apt-get install -y \
  libsdl2-dev \
  libssl-dev \
  libcurl4-openssl-dev \
  && apt-get clean

echo "✅ Core setup complete."
