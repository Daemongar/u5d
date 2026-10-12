#!/usr/bin/env python3
"""Re-cut an AI-upscaled tile sheet so every tile sits on an exact grid.

AI image upscalers (Gemini and the like) redraw the picture rather than scale it
exactly, so tiles drift a few pixels from where an even grid expects them and
plain resizing slices neighboring tiles into each other. This finds each tile's
true position by comparing the upscaled sheet against a reference sheet with the
same layout (the game's TILES.BMP: 32 tiles across, 16 down) and cuts it out
there, then writes a sheet with exact square tiles.

Usage:
  tools/align_tiles.py <upscaled image> <reference TILES.BMP> <output.bmp>
                       [--tile-size 32] [--search 16]

  --tile-size  size of each tile in the output (32 is what the game draws today)
  --search     how far (in upscaled pixels) to look around each tile's expected
               position

The upscaled tiles are assumed square, with the sheet 32 tiles wide (its height
may be squeezed or cropped; the search handles that). Requires Pillow and NumPy.
"""

import argparse

import numpy as np
from PIL import Image

COLUMNS, ROWS = 32, 16
COMPARE = 32  # tiles are compared at this size, in grayscale


def normalized(a):
    return (a - a.mean()) / (a.std() + 1e-3)


def match_score(src, x, y, size, ref_tile):
    crop = Image.fromarray(src[y:y + size, x:x + size].astype(np.uint8))
    a = np.asarray(crop.resize((COMPARE, COMPARE), Image.BILINEAR), dtype=np.float32)
    return float((normalized(a) * ref_tile).mean())


def find_tile(src, x0, y0, size, ref_tile, search):
    """Best (x, y) for a tile near (x0, y0): a coarse pass, then a fine one."""
    h, w = src.shape

    def clamp(x, y):
        return max(0, min(w - size, x)), max(0, min(h - size, y))

    best = (-2.0, *clamp(x0, y0))
    for step, radius in ((2, search), (1, 2)):
        cx, cy = (x0, y0) if step == 2 else (best[1], best[2])
        for dy in range(-radius, radius + 1, step):
            for dx in range(-radius, radius + 1, step):
                x, y = clamp(cx + dx, cy + dy)
                score = match_score(src, x, y, size, ref_tile)
                if score > best[0]:
                    best = (score, x, y)
    return best


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('upscaled')
    parser.add_argument('reference')
    parser.add_argument('output')
    parser.add_argument('--tile-size', type=int, default=32)
    parser.add_argument('--search', type=int, default=16)
    args = parser.parse_args()

    src_rgb = Image.open(args.upscaled).convert('RGB')
    src = np.asarray(src_rgb.convert('L'), dtype=np.float32)
    ref_img = Image.open(args.reference).convert('L')
    ref_tile_px = ref_img.width // COLUMNS
    ref = np.asarray(ref_img.resize((COLUMNS * COMPARE, ROWS * COMPARE), Image.BILINEAR), dtype=np.float32)

    size = round(src_rgb.width / COLUMNS)          # upscaled tiles are square
    row_pitch = src_rgb.height / ROWS               # may differ if the height was squeezed
    print(f"Upscaled sheet {src_rgb.width}x{src_rgb.height}: tiles about {size}x{size} "
          f"(reference tiles {ref_tile_px}x{ref_tile_px})")

    out_size = args.tile_size
    out = Image.new('RGB', (COLUMNS * out_size, ROWS * out_size))
    offsets = np.zeros((ROWS, COLUMNS), dtype=int)
    scores = []

    for t in range(COLUMNS * ROWS):
        r, c = divmod(t, COLUMNS)
        ref_tile = ref[r * COMPARE:(r + 1) * COMPARE, c * COMPARE:(c + 1) * COMPARE]
        x0, y0 = round(c * src_rgb.width / COLUMNS), round(r * row_pitch)

        if ref_tile.std() < 4:
            # Nearly blank reference tile: nothing to match, keep the expected spot.
            x, y = max(0, min(src_rgb.width - size, x0)), max(0, min(src_rgb.height - size, y0))
        else:
            score, x, y = find_tile(src, x0, y0, size, normalized(ref_tile), args.search)
            scores.append((score, t))
        offsets[r, c] = y - y0

        tile = src_rgb.crop((x, y, x + size, y + size))
        out.paste(tile.resize((out_size, out_size), Image.LANCZOS), (c * out_size, r * out_size))

    out.save(args.output)

    scores.sort()
    print("Typical vertical drift per row (pixels):", [int(np.median(row)) for row in offsets])
    print(f"Average match {np.mean([s for s, _ in scores]):.2f} (1.0 = identical shapes)")
    weak = [f"{t} ({s:.2f})" for s, t in scores[:8] if s < 0.5]
    if weak:
        print("Weakest matches, worth checking by eye:", ", ".join(weak))
    print(f"Wrote {args.output}: {out.width}x{out.height}, {out_size}x{out_size} tiles")


if __name__ == '__main__':
    main()
