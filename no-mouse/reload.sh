#!/bin/bash

# Script to rebuild and reload the no-mouse plugin
# Usage: ./reload.sh

set -e  # Exit on error

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_PATH="$SCRIPT_DIR/no-mouse.so"

echo "🔨 Building no-mouse plugin..."
cd "$SCRIPT_DIR"
make clean
make

echo ""
echo "🔄 Unloading old plugin (if loaded)..."
# Try to unload, but don't fail if it's not loaded
hyprctl plugin unload "$PLUGIN_PATH" 2>/dev/null || echo "  Plugin was not loaded, skipping unload"

echo ""
echo "✅ Loading new plugin..."
hyprctl plugin load "$PLUGIN_PATH"

echo ""
echo "🎉 Plugin reloaded successfully!"
echo ""
echo "Try it with: hyprctl dispatch no-mouse toggle"
