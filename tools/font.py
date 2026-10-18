import pathlib
import sys
from PIL import Image, ImageDraw, ImageFont


def build(path, regular, medium):
    sizes = (15, 18, 28, 42)
    nibble = []
    glyphs = []
    for style, size in enumerate(sizes):
        font = ImageFont.truetype(medium if style > 1 else regular, size)
        for char in range(32, 127):
            symbol = chr(char)
            advance = max(1, round(font.getlength(symbol)))
            img = Image.new('L', (size * 3, size * 3), 0)
            draw = ImageDraw.Draw(img)
            baseline = size * 2
            draw.text((size, baseline), symbol, 255, font=font, anchor='ls')
            bbox = img.getbbox()
            off = len(nibble)
            if bbox:
                crop = img.crop(bbox)
                width, height = crop.size
                data = [max(0, min(15, (pixel + 8) // 17)) for pixel in crop.getdata()]
                nibble.extend(data)
                left = bbox[0] - size
                top = bbox[1] - baseline
            else:
                width = height = 0
                left = top = 0
            glyphs.append((off, width, height, advance, left, top))
    packed = [nibble[i] << 4 | (nibble[i + 1] if i + 1 < len(nibble) else 0) for i in range(0, len(nibble), 2)]
    lines = ['#include "font.h"', 'static const uint8_t pixels[] = {']
    for i in range(0, len(packed), 32):
        lines.append(','.join(str(x) for x in packed[i:i+32]) + ',')
    lines += ['};', 'static const QGlyph glyphs[380] = {']
    for glyph in glyphs:
        lines.append('{' + ','.join(str(x) for x in glyph) + '},')
    lines += ['};',
        'const uint8_t *q_font_data(void) { return pixels; }',
        'const QGlyph *q_font_glyph(int style, int code) {',
        '    if (style < 0 || style > 3) style = 1;',
        '    if (code < 32 || code > 126) code = 63;',
        '    return &glyphs[style * 95 + code - 32];',
        '}']
    pathlib.Path(path).write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    build(*sys.argv[1:4])
