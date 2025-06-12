#!/bin/bash
set -e

SCRIPT_DIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"
docker build -t robotick-dev:ubuntu-x64 -f "$SCRIPT_DIR/docker/Docker.ubuntu-x64.dev" .
