#!/bin/bash
set -e

docker build --no-cache -t robotick-dev:esp32s3 -f robotick-engine/docker/Docker.esp32s3.dev .