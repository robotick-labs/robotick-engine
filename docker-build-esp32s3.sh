#!/bin/bash
set -e

docker build -t robotick-dev:esp32s3 -f robotick-engine/docker/Docker.esp32s3.dev .