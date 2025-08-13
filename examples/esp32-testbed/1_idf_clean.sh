#!/bin/bash
set -e

clear
rm -rf build

export WORKLOAD_PRESET=esp32-s3-workloads-all
export ROBOTICK_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/.."

echo -e "\033[1m🔧 Setting target to esp32s3...\033[0m"
idf.py set-target esp32s3

# ✅ Copy sdkconfig AFTER set-target
cp defaults_sdkconfig sdkconfig
