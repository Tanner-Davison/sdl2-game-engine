#!/bin/bash
# Copies animated tile PNGs into the repo and rewrites JSONs with relative paths.
# Run from the sdl-sandbox project root.

set -e
REPO="$(cd "$(dirname "$0")" && pwd)"
ANIM_DIR="$REPO/game_assets/tiles/animated_tiles"

echo "Copying PNGs into repo..."

# CoinSprite
cp /Users/tanner.davison/Desktop/CoinSprite/*.png "$ANIM_DIR/CoinSprite/"

# EvilPlants
cp /Users/tanner.davison/Desktop/EvilPlants/*.png "$ANIM_DIR/EvilPlants/"

# LavaSprite
cp /Users/tanner.davison/Desktop/LavaSprite/*.png "$ANIM_DIR/LavaSprite/"

# LuxurySprite
cp /Users/tanner.davison/Desktop/LuxurySprite/*.png "$ANIM_DIR/LuxurySprite/"

# craftpix pack
CRAFTPIX="/Users/tanner.davison/Desktop/craftpix-897809-jetpack-flat-2d-game-kit/PNG"
cp "$CRAFTPIX/Items Sprite/Heals/"*.png     "$ANIM_DIR/Heals/"
cp "$CRAFTPIX/Collision FX/blownup/"*.png   "$ANIM_DIR/blownup/"
cp "$CRAFTPIX/Collision FX/dazed/"*.png     "$ANIM_DIR/dazed/"
cp "$CRAFTPIX/Items Sprite/rockets/"*.png   "$ANIM_DIR/rockets/"
cp "$CRAFTPIX/Items Sprite/speeddash/"*.png "$ANIM_DIR/speeddash/"
cp "$CRAFTPIX/Collision FX/spoof/"*.png     "$ANIM_DIR/spoof/"

echo "Rewriting JSONs with relative paths..."

python3 - <<'PYEOF'
import json, os, glob

anim_dir = "game_assets/tiles/animated_tiles"

animations = {
    "CoinSprite":  {"fps": 8.0,  "frames": sorted(glob.glob(f"{anim_dir}/CoinSprite/*.png"))},
    "EvilPlants":  {"fps": 9.0,  "frames": sorted(glob.glob(f"{anim_dir}/EvilPlants/*.png"))},
    "LavaSprite":  {"fps": 8.0,  "frames": sorted(glob.glob(f"{anim_dir}/LavaSprite/*.png"))},
    "LuxurySprite":{"fps": 8.0,  "frames": sorted(glob.glob(f"{anim_dir}/LuxurySprite/*.png"))},
    "Heals":       {"fps": 8.0,  "frames": sorted(glob.glob(f"{anim_dir}/Heals/*.png"))},
    "blownup":     {"fps": 9.0,  "frames": sorted(glob.glob(f"{anim_dir}/blownup/*.png"))},
    "dazed":       {"fps": 10.0, "frames": sorted(glob.glob(f"{anim_dir}/dazed/*.png"))},
    "rockets":     {"fps": 8.0,  "frames": sorted(glob.glob(f"{anim_dir}/rockets/*.png"))},
    "speeddash":   {"fps": 8.0,  "frames": sorted(glob.glob(f"{anim_dir}/speeddash/*.png"))},
    "spoof":       {"fps": 12.0, "frames": sorted(glob.glob(f"{anim_dir}/spoof/*.png"))},
}

for name, data in animations.items():
    out = {"name": name, "fps": data["fps"], "frames": data["frames"]}
    path = f"{anim_dir}/{name}.json"
    with open(path, "w") as f:
        json.dump(out, f, indent=4)
    print(f"  Wrote {path} ({len(data['frames'])} frames)")

PYEOF

echo "Done! All animated tile JSONs now use relative paths."
