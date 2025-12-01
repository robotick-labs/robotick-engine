#!/bin/bash
set -e

# Find directory of this script
SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Start ssh-agent if needed
if [ -z "$SSH_AUTH_SOCK" ]; then
    echo "🛑 SSH_AUTH_SOCK not set. Starting agent..."
    eval "$(ssh-agent -s)"
    ssh-add ~/.ssh/id_ed25519
fi

# Validate the socket
if [ ! -S "$SSH_AUTH_SOCK" ]; then
    echo "🛑 SSH agent socket not found at: $SSH_AUTH_SOCK"
    exit 1
fi

# Clean up old container if needed
if docker ps -a --format '{{.Names}}' | grep -q "^robotick-dev-esp32s3$"; then
    echo "🧼 Removing existing container 'robotick-dev-esp32s3'..."
    docker rm -f robotick-dev-esp32s3
fi

# 🚀 Run official Espressif image
docker run -it \
  --user root \
  --privileged \
  -v /dev:/dev \
  -v "$(pwd)":/workspace \
  -v "$HOME/.robotick-vscode-server":/root/.vscode-server \
  -v "$SSH_AUTH_SOCK:/ssh-agent" \
  -e SSH_AUTH_SOCK=/ssh-agent \
  -w /workspace \
  --name robotick-dev-esp32s3 \
  espressif/idf:release-v5.4 \
  bash -c "
    set -e
    echo '📦 Installing ninja...'
    apt-get update && apt-get install -y ninja-build
    echo '🔗 Running symlink setup...'
    bash \"$SCRIPT_DIR/make_esp32_symlinks.sh\"
    echo '🚀 Ready. Launching shell.'
    exec bash
  "
