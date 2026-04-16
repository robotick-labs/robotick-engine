#!/usr/bin/env bash
set -euo pipefail

if [[ $# -gt 1 ]]; then
    echo "Usage: $0 [push-tag]"
    exit 1
fi

: "${IMAGE_NAME:?IMAGE_NAME is required}"
: "${LOCAL_TAG:?LOCAL_TAG is required}"
: "${REMOTE_NAME:?REMOTE_NAME is required}"
: "${SMOKE_TEST:?SMOKE_TEST is required}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)"

BRANCH_NAME="$(git -C "${REPO_ROOT}" rev-parse --abbrev-ref HEAD)"
COMMIT_SHA="$(git -C "${REPO_ROOT}" rev-parse --short HEAD)"
DEFAULT_TAG_BASE="$(
    printf '%s' "${BRANCH_NAME}" \
        | tr '[:upper:]' '[:lower:]' \
        | sed -E 's#[^a-z0-9_.-]+#-#g; s#^[^a-z0-9_]+##; s#-+#-#g; s#-+$##'
)"
DEFAULT_TAG_BASE="${DEFAULT_TAG_BASE:-branch}"
MAX_BASE_LEN=$((128 - 1 - ${#COMMIT_SHA}))
DEFAULT_TAG_BASE="${DEFAULT_TAG_BASE:0:${MAX_BASE_LEN}}"
PUSH_TAG="${1:-${DEFAULT_TAG_BASE}-${COMMIT_SHA}}"
if ! [[ "${PUSH_TAG}" =~ ^[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}$ ]]; then
    echo "Invalid push tag: ${PUSH_TAG}" >&2
    exit 1
fi
REMOTE_TAG="${REMOTE_NAME}:${PUSH_TAG}"

if ! docker image inspect "${LOCAL_TAG}" >/dev/null 2>&1; then
    echo "Missing local image ${LOCAL_TAG}. Build it first."
    exit 1
fi

echo "Smoke-checking ${LOCAL_TAG}"
docker run --rm "${LOCAL_TAG}" bash -lc "${SMOKE_TEST}"

echo "Logging into ghcr.io"
GH_USER="$(gh api user -q .login)"
gh auth token | docker login ghcr.io -u "${GH_USER}" --password-stdin

echo "Tagging ${LOCAL_TAG} as ${REMOTE_TAG}"
docker tag "${LOCAL_TAG}" "${REMOTE_TAG}"

echo "Pushing ${REMOTE_TAG}"
docker push "${REMOTE_TAG}"

echo "Pushed ${REMOTE_TAG}"
