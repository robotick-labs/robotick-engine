#!/bin/bash
set -e

# Start ssh-agent if needed
if [ -z "$SSH_AUTH_SOCK" ]; then
    echo "🛑 SSH_AUTH_SOCK not set. Starting agent..."
    eval "$(ssh-agent -s)"
    ssh-add ~/.ssh/id_ed25519
fi

# Verify socket is valid
if [ ! -S "$SSH_AUTH_SOCK" ]; then
    echo "🛑 SSH agent socket not found at: $SSH_AUTH_SOCK"
    exit 1
fi

# Remove existing container if it exists
if docker ps -a --format '{{.Names}}' | grep -q "^robotick-dev$"; then
    echo "🧼 Removing existing container 'robotick-dev'..."
    docker rm -f robotick-dev
fi

# Run container with SSH agent forwarding
docker run -it \
  -v "$(pwd)":/workspace \
  -v "$HOME/.robotick-vscode-server":/root/.vscode-server \
  -v "$SSH_AUTH_SOCK:/ssh-agent" \
  -e SSH_AUTH_SOCK=/ssh-agent \
  --name robotick-dev \
  robotick-ubuntu
