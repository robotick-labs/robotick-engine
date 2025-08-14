#!/bin/bash
set -e

clear
rm -rf build

echo -e "\033[1m🔧 Setting target to esp32s3...\033[0m"
idf.py -DROBOTICK_WORKLOADS_USE_ALL=ON set-target esp32s3

# ✅ Copy sdkconfig AFTER set-target
cp defaults_sdkconfig sdkconfig
