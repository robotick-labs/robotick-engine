#!/bin/bash
set -e

# Define the base paths
TARGET_DIR="platforms/esp32/components/robotick-engine"
SOURCE_DIR="../../../../cpp"

# Ensure target dir exists
mkdir -p "$TARGET_DIR"

# Symlink each subfolder
for folder in lib include src cmake CMakeWorkloads.json; do
    ln -sf "${SOURCE_DIR}/${folder}" "${TARGET_DIR}/${folder}"
done

echo "Symlinks created in $TARGET_DIR"
