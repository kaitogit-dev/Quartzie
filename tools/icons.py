import base64
import pathlib
import sys
import zipfile
from io import BytesIO
from PIL import Image

NAMES = (
    "1bar_volume", "1bar_wifi", "2bar_volume", "2bar_wifi",
    "3bar_wifi", "cmd", "compressed", "file_explorer", "folder",
    "image", "media_player", "muted_volume", "no_wifi",
    "settings", "text", "trashbin", "trashbin_full",
    "volume", "wifi", "wifi_error",
)


def build(archive, output):
    lines = ['#include "icons.h"', 'static const uint32_t atlas[QI_COUNT][Q_ICON_SIZE*Q_ICON_SIZE] = {']
    with zipfile.ZipFile(archive) as source:
        found = set(source.namelist())
        if found != {name + '.ico' for name in NAMES}:
            raise ValueError('Icon archive contents do not match the expected set')
        for name in NAMES:
            raw = source.read(name + '.ico')
            icon = Image.open(BytesIO(raw))
            icon = icon.ico.getimage((64, 64)).convert('RGBA')
            icon = icon.resize((48, 48), Image.Resampling.LANCZOS)
            lines.append('{')
            pixels = list(icon.getdata())
            for start in range(0, len(pixels), 16):
                section = pixels[start:start+16]
                lines.append(','.join(
                    '0x%08x' % ((a << 24) | (r << 16) | (g << 8) | b)
                    for r, g, b, a in section
                ) + ',')
            lines.append('},')
    icon = Image.open(BytesIO(base64.b64decode(
        pathlib.Path("assets/start-icon.png.base64").read_text().strip()
    ))).convert("RGBA")
    if icon.size != (48, 48):
        raise ValueError("Start logo atlas image must be 48 x 48")
    lines.append("{")
    pixels = list(icon.getdata())
    for start in range(0, len(pixels), 16):
        section = pixels[start:start + 16]
        lines.append(",".join(
            "0x%08x" % ((a << 24) | (r << 16) | (g << 8) | b)
            for r, g, b, a in section
        ) + ",")
    lines.append("},")
    lines.extend((
        '};',
        'const uint32_t *q_icon_pixels(unsigned int id){',
        '    return id<QI_COUNT ? atlas[id] : 0;',
        '}',
    ))
    pathlib.Path(output).write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    build(*sys.argv[1:3])
