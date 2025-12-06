#!/bin/bash
set -e

echo -e "\033[1m🔨 Building project...\033[0m" && \
idf.py build
