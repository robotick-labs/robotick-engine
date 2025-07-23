#!/bin/bash
set -e

SCRIPT_DIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"
docker build -t robotick-dev:linux -f "$SCRIPT_DIR/docker/Docker.linux.dev" .
