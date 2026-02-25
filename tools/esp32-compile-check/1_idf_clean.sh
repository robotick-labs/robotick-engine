#!/bin/bash
set -e

rm -rf build

echo -e "\033[1m🔧 Setting target to esp32s3...\033[0m"
idf.py set-target esp32s3

# ✅ Copy sdkconfig AFTER set-target
cp defaults_sdkconfig sdkconfig
