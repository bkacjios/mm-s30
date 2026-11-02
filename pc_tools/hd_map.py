#!/usr/bin/env python3
"""
World map art for the map overhaul (src/code/s30_map_overhaul.c), from the MMN64HD texture pack
(Nerrel et al., https://github.com/Nerrel/MMHD/): its Map/*.dds files, which replace the overhaul's
overlay thumbnails, cut down for the S30 and written as raw textures for S30Gfx_TextureOverride.

    pc_tools/hd_map.py path/to/mm_MMN64HD.rtz data/hdmap

Each texture is a quarter of the pack's size (the world map 6912x4096 -> 1728x1024), then stretched to
the nearest power of 2 on each side for mipmaps on GLES2 (texture coordinates are normalised, so the
stretch doesn't show). Opaque ones are RGB565, the rest RGBA8888. (The world map at 4096 wide, for the
overhaul's 8x zoom, made the game lag on the device.)

File: "S30T", u16 width, u16 height, u8 format (0 RGBA8888, 1 RGB565), 3 bytes padding, then the
pixels, rows top first, little-endian.
"""
import io
import math
import os
import struct
import sys
import zipfile

import numpy as np
from PIL import Image

SCALE = 4  # the pack's size / this
MAX_SIDE = 2048


def pot(n):
    return min(MAX_SIDE, max(1, 2 ** round(math.log2(max(n, 1)))))


def convert(data):
    im = Image.open(io.BytesIO(data))
    im.load()
    im = im.convert("RGBA")
    w = pot(im.width / SCALE)
    h = pot(im.height / SCALE)
    im = im.resize((w, h), Image.Resampling.LANCZOS)
    opaque = im.getchannel("A").getextrema()[0] == 255
    if opaque:
        rgb = np.asarray(im, dtype=np.uint16)
        px = ((rgb[..., 0] >> 3) << 11 | (rgb[..., 1] >> 2) << 5 | rgb[..., 2] >> 3).astype("<u2").tobytes()
    else:
        px = im.tobytes()
    return struct.pack("<4sHHB3x", b"S30T", w, h, 1 if opaque else 0) + bytes(px), (w, h, opaque)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    rtz, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    with zipfile.ZipFile(rtz) as z:
        for name in z.namelist():
            # The named Map/ files are the overhaul's; Map/ZELDA... are other HD map textures
            if not name.startswith("Map/") or not name.endswith(".dds") or "#" in name:
                continue
            base = name[len("Map/") : -len(".dds")]
            blob, (w, h, opaque) = convert(z.read(name))
            with open(os.path.join(out, base + ".tex"), "wb") as f:
                f.write(blob)
            print("%-16s %4dx%-4d %s" % (base, w, h, "RGB565" if opaque else "RGBA8888"))


if __name__ == "__main__":
    main()
