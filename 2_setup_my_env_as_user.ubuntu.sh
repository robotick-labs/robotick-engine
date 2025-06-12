#!/usr/bin/env bash
set -e
echo "🐍 Setting up Python environment..."

python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip

if [ -f setup.py ]; then
  echo "✅ Installing Robotick from root setup.py..."
  pip install -e .
elif [ -f robotick-engine/setup.py ]; then
  echo "✅ Installing Robotick from subdir..."
  cd robotick-engine && pip install -e .
else
  echo "❌ setup.py not found!" && exit 1
fi

echo "✅ Python setup complete."