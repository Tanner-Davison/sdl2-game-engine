#!/bin/bash
# Find all occurrences of old palette member names in LevelEditorScene.cpp
# (outside the #if 0 block which is dead code)

FILE="/Users/tanner.davison/projects/cpp/sdl-sandbox/src/LevelEditorScene.cpp"

echo "=== mPaletteItems ==="
grep -n 'mPaletteItems' "$FILE" | grep -v '#if 0' | grep -v 'REMOVED_'

echo ""
echo "=== mBgItems ==="
grep -n 'mBgItems' "$FILE" | grep -v '#if 0' | grep -v 'REMOVED_'

echo ""
echo "=== mSelectedTile ==="
grep -n 'mSelectedTile' "$FILE" | grep -v '#if 0' | grep -v 'REMOVED_'

echo ""
echo "=== mSelectedBg ==="
grep -n 'mSelectedBg' "$FILE" | grep -v '#if 0' | grep -v 'REMOVED_'

echo ""
echo "=== mPaletteScroll ==="
grep -n 'mPaletteScroll' "$FILE" | grep -v '#if 0' | grep -v 'REMOVED_'

echo ""
echo "=== mBgPaletteScroll ==="
grep -n 'mBgPaletteScroll' "$FILE" | grep -v '#if 0' | grep -v 'REMOVED_'

echo ""
echo "=== mTileCurrentDir ==="
grep -n 'mTileCurrentDir' "$FILE" | grep -v '#if 0' | grep -v 'REMOVED_'

echo ""
echo "=== mPaletteCollapsed ==="
grep -n 'mPaletteCollapsed' "$FILE" | grep -v '#if 0' | grep -v 'REMOVED_'

echo ""
echo "=== mActiveTab ==="
grep -n 'mActiveTab' "$FILE" | grep -v '#if 0' | grep -v 'REMOVED_'

echo ""
echo "=== PaletteTab ==="
grep -n 'PaletteTab' "$FILE" | grep -v '#if 0' | grep -v 'REMOVED_'

echo ""
echo "=== mPalCellLabels ==="
grep -n 'mPalCellLabels' "$FILE" | grep -v '#if 0' | grep -v 'REMOVED_'

echo ""
echo "=== mLastClickTime ==="
grep -n 'mLastClickTime' "$FILE"

echo ""
echo "=== mLastClickIndex ==="
grep -n 'mLastClickIndex' "$FILE"

echo ""
echo "=== mFolderIcon ==="
grep -n 'mFolderIcon' "$FILE"
