#!/bin/bash
set -e

SCRIPT_DIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"
docker build --no-cache -t robotick-dev:esp32s3 -f "$SCRIPT_DIR/docker/Docker.esp32s3.dev" .
