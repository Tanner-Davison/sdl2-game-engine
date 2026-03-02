from PIL import Image

import glob, os

frames = sorted(glob.glob('/home/tanner/projects/cpp/sdl-sandbox/game_assets/frost_knight_png_sequences/Idle/*.png'))[:6]
frames += sorted(glob.glob('/home/tanner/projects/cpp/sdl-sandbox/game_assets/frost_knight_png_sequences/Walking/*.png'))[:4]

all_smx, all_smy, all_sMx, all_sMy = [], [], [], []

for fpath in frames:
    img = Image.open(fpath).convert('RGBA')
    w, h = img.size
    pixels = img.load()
    threshold = 10
    mn_x, mn_y, mx_x, mx_y = w, h, 0, 0
    for y in range(h):
        for x in range(w):
            if pixels[x, y][3] > threshold:
                if x < mn_x: mn_x = x
                if x > mx_x: mx_x = x
                if y < mn_y: mn_y = y
                if y > mx_y: mx_y = y
    sx = 120 / w; sy = 160 / h
    all_smx.append(int(mn_x*sx)); all_smy.append(int(mn_y*sy))
    all_sMx.append(int(mx_x*sx)); all_sMy.append(int(mx_y*sy))
    print(f'{os.path.basename(fpath)}: scaled x={int(mn_x*sx)}-{int(mx_x*sx)}, y={int(mn_y*sy)}-{int(mx_y*sy)}')

print(f'\nWorst-case (tightest fit across all frames):')
print(f'  min of left insets  = {min(all_smx)}')
print(f'  min of top insets   = {min(all_smy)}')
print(f'  max right edge      = {max(all_sMx)}')
print(f'  max bottom edge     = {max(all_sMy)}')
final_inset_x = min(all_smx)
final_inset_top = min(all_smy)
final_inset_bottom = 160 - max(all_sMy) - 1
final_inset_right = 120 - max(all_sMx) - 1
print(f'\nFinal insets (tight across all sampled frames):')
print(f'  INSET_X      = {min(final_inset_x, final_inset_right)}  (use smaller of left/right)')
print(f'  INSET_TOP    = {final_inset_top}')
print(f'  INSET_BOTTOM = {final_inset_bottom}')

img = Image.open(frames[0]).convert('RGBA')
w, h = img.size
print(f'Native size: {w}x{h}')

pixels = img.load()
threshold = 10
min_x, min_y, max_x, max_y = w, h, 0, 0

for y in range(h):
    for x in range(w):
        if pixels[x, y][3] > threshold:
            if x < min_x: min_x = x
            if x > max_x: max_x = x
            if y < min_y: min_y = y
            if y > max_y: max_y = y

print(f'Tight bounds (native): x={min_x}-{max_x}, y={min_y}-{max_y}')
print(f'Character size (native): {max_x-min_x+1} x {max_y-min_y+1}')

sx = 120 / w
sy = 160 / h
smx = int(min_x * sx)
smy = int(min_y * sy)
sMx = int(max_x * sx)
sMy = int(max_y * sy)

print(f'Scaled to 120x160: x={smx}-{sMx}, y={smy}-{sMy}')
print(f'Character size (scaled): {sMx-smx+1} x {sMy-smy+1}')
print(f'')
print(f'INSET_TOP    = {smy}')
print(f'INSET_BOTTOM = {160 - sMy - 1}')
print(f'INSET_X_LEFT = {smx}')
print(f'INSET_X_RIGHT= {120 - sMx - 1}')
print(f'(X inset symmetric avg: {(smx + (120 - sMx - 1)) // 2})')
