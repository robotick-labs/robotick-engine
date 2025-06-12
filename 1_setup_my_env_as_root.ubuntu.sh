#!/usr/bin/env bash
set -e
echo "🔧 Installing system dependencies as root..."

sudo apt-get update && sudo apt-get install -y \
  build-essential \
  cmake \
  clang \
  clang-tidy \
  clang-format \
  curl \
  g++ \
  make \
  git \
  python3 \
  python3-pip \
  python3-venv \
  ninja-build \
  gdb \
  libssl-dev \
  libcurl4-openssl-dev \
  && sudo apt-get clean

echo "📦 Installing Paho MQTT C library..."
git clone --depth=1 https://github.com/eclipse/paho.mqtt.c.git /tmp/paho.mqtt.c
cd /tmp/paho.mqtt.c
cmake -Bbuild -S. \
  -DPAHO_BUILD_STATIC=TRUE \
  -DPAHO_BUILD_SHARED=TRUE \
  -DPAHO_WITH_SSL=TRUE \
  -DPAHO_BUILD_CMAKE_EXPORTS=ON \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --target install -- -j$(nproc)
sudo cmake --install build
sudo ldconfig
rm -rf /tmp/paho.mqtt.c

# ← HERE: move out of deleted directory
cd /tmp

echo "📦 Installing Paho MQTT C++ library..."
git clone --depth=1 https://github.com/eclipse/paho.mqtt.cpp.git /tmp/paho.mqtt.cpp
cd /tmp/paho.mqtt.cpp
cmake -Bbuild -S. \
  -DPAHO_BUILD_STATIC=TRUE \
  -DPAHO_BUILD_SHARED=TRUE \
  -DPAHO_BUILD_CMAKE_EXPORTS=ON \
  -DCMAKE_PREFIX_PATH=/usr \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --target install -- -j$(nproc)
sudo cmake --install build
sudo ldconfig
rm -rf /tmp/paho.mqtt.cpp

echo "✅ Root-level setup complete."
