#!/usr/bin/env bash
set -e
echo "🐍 Setting up Python environment (Docker-friendly, system-wide)..."

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SETUP_FILE="$SCRIPT_DIR/../setup.py"

if [ ! -f "$SETUP_FILE" ]; then
  echo "❌ setup.py not found in $SETUP_FILE!" && exit 1
fi

echo "🧹 Removing any existing .egg-info metadata..."
find "$SCRIPT_DIR/.." -type d -name '*.egg-info' -exec rm -rf {} +

echo "📦 Installing Robotick (editable) to system site-packages..."
pip3 install --break-system-packages -e "$SCRIPT_DIR/.."

echo "🔍 Verifying 'robotick' module import using: $(which python3)"
if ! python3 -c "import robotick" 2>/dev/null; then
  echo "❌ 'robotick' module not importable after install!"
  exit 1
fi

echo "✅ Python setup complete (system install, no sudo)."
