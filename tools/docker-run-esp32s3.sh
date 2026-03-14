#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel 2>/dev/null || true)"

if [[ -z "$REPO_ROOT" ]]; then
    echo "🛑 Unable to determine git repo root from $SCRIPT_DIR"
    exit 1
fi

IMAGE_NAME="robotick-engine-esp32s3"
DOCKERFILE="$SCRIPT_DIR/docker/esp32s3/Dockerfile"
CONTAINER_NAME="robotick-engine-dev-esp32s3"

echo "🐳 Building image: $IMAGE_NAME"
docker build \
  -t "$IMAGE_NAME" \
  -f "$DOCKERFILE" \
  "$(dirname "$DOCKERFILE")"

if docker ps -a --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
  echo "🧼 Removing existing container '${CONTAINER_NAME}'..."
  docker rm -f "$CONTAINER_NAME"
fi

docker run -it \
  --user root \
  --privileged \
  -v /dev:/dev \
  -v "$REPO_ROOT:$REPO_ROOT" \
  -w "$REPO_ROOT/robotick/robotick-engine" \
  --name "$CONTAINER_NAME" \
  "$IMAGE_NAME" \
  bash -c "
    set -e
    echo '🔗 Running symlink setup...'
    bash \"$SCRIPT_DIR/make_esp32_symlinks.sh\"
    echo '🚀 Ready. Launching shell.'
    exec bash
  "
