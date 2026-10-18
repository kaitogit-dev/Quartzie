#!/usr/bin/env python3
"""Turn the single source wallpaper PNG into 256x144 RGB565 pixels for BIOS rendering.

Supports ordinary non-interlaced 8-bit RGB/RGBA PNGs; uses Python stdlib only.
The PNG remains the editable project asset, while the boot image embeds an
uncompressed downsampled resource for predictable, low-overhead drawing.
"""
import pathlib
import struct
import sys
import zlib

WIDTH, HEIGHT = 256, 144
SIGNATURE = b'\x89PNG\r\n\x1a\n'


def decode_rgb(path):
    raw = pathlib.Path(path).read_bytes()
    if not raw.startswith(SIGNATURE):
        raise ValueError('Wallpaper must be PNG')
    offset = len(SIGNATURE)
    chunks = bytearray()
    dimension = None
    while offset + 12 <= len(raw):
        size = struct.unpack_from('>I', raw, offset)[0]
        tag = raw[offset + 4:offset + 8]
        block = raw[offset + 8:offset + 8 + size]
        crc = struct.unpack_from('>I', raw, offset + 8 + size)[0]
        if zlib.crc32(tag + block) != crc:
            raise ValueError('Wallpaper PNG CRC error')
        offset += size + 12
        if tag == b'IHDR':
            w, h, depth, mode, compression, filtering, interlace = struct.unpack('>IIBBBBB', block)
            if depth != 8 or mode not in (2, 6) or compression or filtering or interlace:
                raise ValueError('Wallpaper requires noninterlaced RGB/RGBA 8-bit PNG')
            if w < WIDTH or h < HEIGHT or w > 4096 or h > 4096:
                raise ValueError('Wallpaper dimensions outside supported range')
            dimension = (w, h, 3 if mode == 2 else 4)
        elif tag == b'IDAT':
            chunks.extend(block)
        elif tag == b'IEND':
            break
    if dimension is None:
        raise ValueError('Missing PNG IHDR')
    w, h, bpp = dimension
    data = zlib.decompress(chunks)
    pitch = w * bpp
    if len(data) != (pitch + 1) * h:
        raise ValueError('Invalid PNG scanline length')
    rows = []
    previous = bytearray(pitch)
    offset = 0
    for _ in range(h):
        mode = data[offset]
        line = bytearray(data[offset + 1:offset + 1 + pitch])
        offset += pitch + 1
        if mode not in (0, 1, 2, 3, 4):
            raise ValueError('Invalid PNG filter')
        for col in range(pitch):
            left = line[col - bpp] if col >= bpp else 0
            above = previous[col]
            upper_left = previous[col - bpp] if col >= bpp else 0
            if mode == 1:
                line[col] = (line[col] + left) & 255
            elif mode == 2:
                line[col] = (line[col] + above) & 255
            elif mode == 3:
                line[col] = (line[col] + (left + above) // 2) & 255
            elif mode == 4:
                p = left + above - upper_left
                a, b, c = abs(p - left), abs(p - above), abs(p - upper_left)
                pred = left if a <= b and a <= c else above if b <= c else upper_left
                line[col] = (line[col] + pred) & 255
        rows.append(line)
        previous = line
    return w, h, bpp, rows


def convert(source, dest):
    w, h, channels, rows = decode_rgb(source)
    output = bytearray(WIDTH * HEIGHT * 2)
    for y in range(HEIGHT):
        sy = (y * (h - 1) + HEIGHT // 2) // HEIGHT
        for x in range(WIDTH):
            sx = (x * (w - 1) + WIDTH // 2) // WIDTH
            start = sx * channels
            r, g, b = rows[sy][start:start + 3]
            rgb565 = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)
            struct.pack_into('<H', output, 2 * (y * WIDTH + x), rgb565)
    pathlib.Path(dest).write_bytes(output)
    print(f'Converted {source} into {len(output):,} bytes of RGB565 wallpaper pixels')


if __name__ == '__main__':
    convert(sys.argv[1], sys.argv[2])
