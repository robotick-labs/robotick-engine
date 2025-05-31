#!/bin/bash

set -e

if [ -z "$1" ]; then
  echo "Usage: ./reset-feature.sh <feature-name>"
  exit 1
fi

FEATURE_NAME="feature/$1"

echo "👉 Switching to dev..."
git checkout dev
git pull origin dev

if git show-ref --verify --quiet refs/heads/"$FEATURE_NAME"; then
  echo "🧹 Deleting local branch '$FEATURE_NAME'..."
  git branch -D "$FEATURE_NAME"
fi

echo "🌱 Creating fresh branch '$FEATURE_NAME' from dev..."
git checkout -b "$FEATURE_NAME"

echo "🚀 Force pushing to origin (overwrites remote branch if it exists)..."
git push -u origin "$FEATURE_NAME" --force

echo "✅ Done! Branch '$FEATURE_NAME' is now reset to match dev."
