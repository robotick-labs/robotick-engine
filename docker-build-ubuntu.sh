#!/bin/bash
set -e

docker build -t robotick-dev:ubuntu-x64 -f robotick-engine/docker/Docker.ubuntu-x64.dev .