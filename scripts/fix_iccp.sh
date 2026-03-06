#!/usr/bin/env bash
# fix_iccp.sh — strips the known-incorrect sRGB iCCP chunk from all PNG files
# under game_assets/. Run once from the project root:
#
#   chmod +x scripts/fix_iccp.sh
#   ./scripts/fix_iccp.sh
#
# macOS:  brew install pngcrush   (preferred)
#         brew install imagemagick (fallback)
# WSL/Linux: sudo apt install pngcrush
#            sudo apt install imagemagick

set -euo pipefail

ASSET_DIR="${1:-game_assets}"

if ! [ -d "$ASSET_DIR" ]; then
    echo "ERROR: asset directory '$ASSET_DIR' not found."
    echo "Run this script from the project root, e.g.:"
    echo "  cd ~/projects/cpp/sdl-sandbox && ./scripts/fix_iccp.sh"
    exit 1
fi

TOTAL=$(find "$ASSET_DIR" -iname "*.png" | wc -l | tr -d ' ')
echo "Found $TOTAL PNG files under $ASSET_DIR/"

if command -v pngcrush &>/dev/null; then
    echo "Using pngcrush to strip iCCP chunks..."
    FIXED=0
    FAILED=0
    while IFS= read -r -d '' f; do
        if pngcrush -ow -rem iCCP "$f" &>/dev/null; then
            FIXED=$((FIXED + 1))
        else
            echo "  WARN: pngcrush failed on $f"
            FAILED=$((FAILED + 1))
        fi
    done < <(find "$ASSET_DIR" -iname "*.png" -print0)
    echo "Done. Fixed: $FIXED  Failed: $FAILED"

elif command -v mogrify &>/dev/null; then
    echo "pngcrush not found, using ImageMagick mogrify -strip..."
    FIXED=0
    FAILED=0
    while IFS= read -r -d '' f; do
        if mogrify -strip "$f" &>/dev/null; then
            FIXED=$((FIXED + 1))
        else
            echo "  WARN: mogrify failed on $f"
            FAILED=$((FAILED + 1))
        fi
    done < <(find "$ASSET_DIR" -iname "*.png" -print0)
    echo "Done. Fixed: $FIXED  Failed: $FAILED"

else
    echo "ERROR: Neither pngcrush nor mogrify (ImageMagick) found."
    echo ""
    if [[ "$(uname)" == "Darwin" ]]; then
        echo "You're on macOS. Install with Homebrew:"
        echo "  brew install pngcrush"
        echo "  (or: brew install imagemagick)"
    else
        echo "Install with apt:"
        echo "  sudo apt install pngcrush"
        echo "  (or: sudo apt install imagemagick)"
    fi
    exit 1
fi
