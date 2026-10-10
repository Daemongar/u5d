#!/usr/bin/env python3
"""Build a 32x32 full-color tile atlas (TILES.BMP) from the original TILES.16.

The original tiles are 512 16x16 images in the 16-color EGA palette. This
script:

  1. decodes TILES.16 (the game's LZW format),
  2. doubles each tile with Scale2x, which rounds off diagonal staircases
     instead of plain pixel doubling,
  3. recolors it with a softer, richer palette and light top/bottom shading,
  4. on terrain tiles only, blends EGA checkerboard dithering into true
     intermediate colors and adds a little grain.

People, monsters and objects keep crisp edges. The output is a 24-bit BMP,
32 tiles across, which the SDL builds load from the game data directory.

Usage: tools/upscale_tiles.py <path/to/TILES.16> <output TILES.BMP>
Requires Pillow and NumPy.
"""

import struct
import sys

import numpy as np
from PIL import Image

TILE = 16
SCALE = 2
COLUMNS = 32

# Softer, slightly warmer stand-ins for the 16 EGA colors.
PALETTE = np.array([
    (14, 12, 18),     # 0  black
    (28, 56, 140),    # 1  blue
    (48, 138, 60),    # 2  green
    (42, 148, 158),   # 3  cyan
    (158, 40, 34),    # 4  red
    (138, 52, 138),   # 5  magenta
    (150, 98, 48),    # 6  brown
    (172, 168, 160),  # 7  light gray
    (88, 86, 94),     # 8  dark gray
    (90, 122, 228),   # 9  light blue
    (112, 206, 92),   # 10 light green
    (122, 218, 228),  # 11 light cyan
    (232, 98, 82),    # 12 light red
    (218, 112, 218),  # 13 light magenta
    (244, 218, 102),  # 14 yellow
    (244, 240, 230),  # 15 white
], dtype=np.float32)

# Terrain tiles are drawn as colored specks on black. In full color, black
# becomes a ground color for that terrain (see LOOK2.DAT descriptions).
GROUND = {
    0x01: (12, 26, 70),     # deep water
    0x02: (18, 40, 92),     # water
    0x03: (34, 70, 112),    # shoals
    0x04: (50, 62, 36),     # swamp
    0x05: (36, 76, 32),     # grass
    0x06: (52, 78, 34),     # brush
    0x07: (152, 126, 80),   # parched desert
    0x08: (52, 78, 34),     # brush
    0x09: (26, 60, 28),     # trees
    0x0a: (22, 56, 30),     # tropical forest
    0x0b: (68, 74, 44),     # foothills
    0x0c: (64, 62, 60),     # mountains
    0x0d: (72, 72, 78),     # high peaks
    0x0e: (68, 74, 44),     # foothills
    0x0f: (68, 74, 44),     # foothills
    0x1c: (152, 126, 80),   # parched desert
    0x1e: (152, 126, 80),   # parched desert
    0x1f: (152, 126, 80),   # parched desert
    0x2c: (92, 66, 42),     # plowed patch
    0x2d: (86, 70, 40),     # bountiful crops
}

# Locations, roads and the like sit on black too; give them ground so they
# don't show as black squares on the map. Only background black (connected
# to the tile edge) is replaced, so doorways and windows stay dark.
GRASS, ROCK, WATER, SAND = (36, 76, 32), (64, 62, 60), (18, 40, 92), (152, 126, 80)
BACKDROP = {t: GRASS for t in range(0x10, 0x1c)}
BACKDROP.update({0x16: ROCK, 0x17: ROCK, 0x18: ROCK})     # cave, mine, dungeon
BACKDROP.update({t: GRASS for t in range(0x20, 0x27)})    # roads
BACKDROP.update({0x1d: WATER, 0x2e: GRASS, 0x2f: SAND})   # bridge, fruit tree, cactus

# Tiles that get dither blending and grain.
TERRAIN = set(GROUND)


def lzw_decompress(data):
    """Port of src/common/lzw.c: 32-bit length, then 9-12 bit LSB-first codes."""
    out_len = struct.unpack_from('<I', data, 0)[0]
    pos, bit_buf, bits = 4, 0, 0
    code_size, next_code, prev = 9, 258, -1
    prefix = [0] * 4096
    suffix = [0] * 4096
    out = bytearray()

    def expand(code):
        stack = []
        while code >= 256:
            stack.append(suffix[code])
            code = prefix[code]
        stack.append(code)
        return stack[::-1]

    while len(out) < out_len:
        while bits < code_size:
            if pos >= len(data):
                break
            bit_buf |= data[pos] << bits
            pos += 1
            bits += 8
        if bits < code_size:
            break
        code = bit_buf & ((1 << code_size) - 1)
        bit_buf >>= code_size
        bits -= code_size

        if code == 256:
            code_size, next_code, prev = 9, 258, -1
            continue
        if code == 257:
            break
        if code < next_code:
            seq = expand(code)
        elif code == next_code and prev >= 0:
            seq = expand(prev)
            seq.append(seq[0])
        else:
            raise ValueError(f"invalid LZW code {code}")
        out.extend(seq)

        if prev >= 0 and next_code < 4096:
            prefix[next_code] = prev
            suffix[next_code] = seq[0]
            next_code += 1
            if code_size < 12 and next_code == (1 << code_size):
                code_size += 1
        prev = code

    return bytes(out[:out_len])


def decode_tiles(raw):
    """512 tiles, 4 bits per pixel, high nibble is the left pixel."""
    data = np.frombuffer(raw, dtype=np.uint8).reshape(-1, TILE, TILE // 2)
    tiles = np.empty((data.shape[0], TILE, TILE), dtype=np.uint8)
    tiles[:, :, 0::2] = data >> 4
    tiles[:, :, 1::2] = data & 0x0f
    return tiles


def scale2x(img):
    """Scale2x/EPX on an index image, clamping at the tile edges."""
    p = np.pad(img, 1, mode='edge')
    c = p[1:-1, 1:-1]
    a, d = p[:-2, 1:-1], p[2:, 1:-1]   # up, down
    l, r = p[1:-1, :-2], p[1:-1, 2:]   # left, right

    e0 = np.where((l == a) & (l != d) & (a != r), a, c)
    e1 = np.where((a == r) & (a != l) & (r != d), r, c)
    e2 = np.where((d == l) & (d != r) & (l != a), l, c)
    e3 = np.where((r == d) & (r != a) & (d != l), d, c)

    out = np.empty((img.shape[0] * 2, img.shape[1] * 2), dtype=img.dtype)
    out[0::2, 0::2] = e0
    out[0::2, 1::2] = e1
    out[1::2, 0::2] = e2
    out[1::2, 1::2] = e3
    return out


def box_blur(rgb):
    p = np.pad(rgb, ((1, 1), (1, 1), (0, 0)), mode='edge')
    acc = np.zeros_like(rgb)
    for dy in range(3):
        for dx in range(3):
            acc += p[dy:dy + rgb.shape[0], dx:dx + rgb.shape[1]]
    return acc / 9.0


def edge_connected(mask):
    """Pixels of mask reachable from the tile border (4-connected)."""
    h, w = mask.shape
    seen = np.zeros_like(mask, dtype=bool)
    stack = [(y, x) for y in range(h) for x in (0, w - 1) if mask[y, x]]
    stack += [(y, x) for x in range(w) for y in (0, h - 1) if mask[y, x]]
    while stack:
        y, x = stack.pop()
        if seen[y, x]:
            continue
        seen[y, x] = True
        for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
            if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not seen[ny, nx]:
                stack.append((ny, nx))
    return seen


def render_tile(index, idx16, rng):
    idx32 = scale2x(idx16)
    rgb = PALETTE[idx32].copy()
    if index in GROUND:
        rgb[idx32 == 0] = GROUND[index]
    elif index in BACKDROP:
        rgb[edge_connected(idx32 == 0)] = BACKDROP[index]

    # Shading: lighten the top edge of each shape, darken the bottom edge.
    up = np.pad(idx32, ((1, 0), (0, 0)), mode='edge')[:-1]
    down = np.pad(idx32, ((0, 1), (0, 0)), mode='edge')[1:]
    shade = np.ones(idx32.shape, dtype=np.float32)
    shade[up != idx32] *= 1.10
    shade[down != idx32] *= 0.86
    rgb *= shade[:, :, None]

    if index in TERRAIN:
        # Blend checkerboard dithering into intermediate colors: weight by how
        # many of a source pixel's neighbors differ from it.
        p = np.pad(idx16, 1, mode='edge')
        c = p[1:-1, 1:-1]
        busy = ((p[:-2, 1:-1] != c).astype(np.float32) + (p[2:, 1:-1] != c) +
                (p[1:-1, :-2] != c) + (p[1:-1, 2:] != c)) / 4.0
        weight = np.kron(busy, np.ones((SCALE, SCALE)))[:, :, None] * 0.75
        rgb = rgb * (1.0 - weight) + box_blur(rgb) * weight

        # A little grain so repeated terrain doesn't look flat.
        grain = rng.normal(1.0, 0.035, idx32.shape).astype(np.float32)
        rgb *= grain[:, :, None]

    return np.clip(rgb, 0, 255).astype(np.uint8)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)

    with open(sys.argv[1], 'rb') as f:
        tiles = decode_tiles(lzw_decompress(f.read()))

    size = TILE * SCALE
    rows = (len(tiles) + COLUMNS - 1) // COLUMNS
    atlas = np.zeros((rows * size, COLUMNS * size, 3), dtype=np.uint8)
    rng = np.random.default_rng(1988)

    for i, tile in enumerate(tiles):
        r, c = divmod(i, COLUMNS)
        atlas[r * size:(r + 1) * size, c * size:(c + 1) * size] = render_tile(i, tile, rng)

    Image.fromarray(atlas, 'RGB').save(sys.argv[2], format='BMP')
    print(f"Wrote {len(tiles)} tiles ({atlas.shape[1]}x{atlas.shape[0]}) to {sys.argv[2]}")


if __name__ == '__main__':
    main()
