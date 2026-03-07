#!/usr/bin/env python3
import sys

filepath = "/Users/tanner.davison/projects/cpp/sdl-sandbox/src/LevelEditorScene.cpp"

with open(filepath, 'r') as f:
    lines = f.readlines()

# Find the markers
start_idx = None
end_idx = None
for i, line in enumerate(lines):
    if '// DEADBLOCK_A' in line:
        start_idx = i
    if '// DEAD_CODE_END' in line:
        end_idx = i

if start_idx is None or end_idx is None:
    print(f"ERROR: markers not found. start={start_idx}, end={end_idx}")
    sys.exit(1)

print(f"Removing lines {start_idx+1} through {end_idx+1} ({end_idx - start_idx + 1} lines)")

# Remove the block
new_lines = lines[:start_idx] + lines[end_idx+1:]

with open(filepath, 'w') as f:
    f.writelines(new_lines)

print("Done.")
