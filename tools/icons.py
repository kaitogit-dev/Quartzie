import base64
import pathlib
import sys
import zipfile
from io import BytesIO
from PIL import Image
from clean_icons import clean

NAMES = (
    "1bar_volume", "1bar_wifi", "2bar_volume", "2bar_wifi",
    "3bar_wifi", "cmd", "compressed", "file_explorer", "folder",
    "image", "media_player", "muted_volume", "no_wifi",
    "settings", "text", "trashbin", "trashbin_full",
    "volume", "wifi", "wifi_error",
)

SIZES = (64, 25, 27, 28, 40, 43, 58)


def build(archive, output):
    sources = []
    with zipfile.ZipFile(archive) as icons:
        if set(icons.namelist()) != {name + ".ico" for name in NAMES}:
            raise ValueError("Icon archive contents do not match the expected set")
        for name in NAMES:
            original = Image.open(BytesIO(icons.read(name + ".ico")))
            sources.append(clean(original.ico.getimage((64, 64))))
    start = Image.open(BytesIO(base64.b64decode(
        pathlib.Path("assets/start-icon.png.base64").read_text().strip()
    ))).convert("RGBA")
    sources.append(start.resize((64, 64), Image.Resampling.LANCZOS))
    for name in ("about-icon", "calculator-icon", "notes-icon", "store-icon"):
        raw = base64.b64decode(pathlib.Path(f"assets/{name}.png.base64").read_text().strip())
        custom = Image.open(BytesIO(raw)).convert("RGBA")
        if custom.size != (64, 64):
            raise ValueError("Custom app icon must be 64x64")
        sources.append(custom)
    lines = ['#include "icons.h"']
    for size in SIZES:
        lines.append(f"static const uint32_t atlas_{size}[QI_COUNT][{size}*{size}] = {{")
        for icon in sources:
            resized = clean(icon if size == 64 else icon.resize((size, size), Image.Resampling.LANCZOS))
            pixels = list(resized.getdata())
            lines.append("{")
            for offset in range(0, len(pixels), 16):
                sample = pixels[offset:offset+16]
                lines.append(",".join(
                    "0x%08x" % ((a << 24) | (r << 16) | (g << 8) | b)
                    for r, g, b, a in sample
                ) + ",")
            lines.append("},")
        lines.append("};")
    lines.extend((
        "const uint32_t *q_icon_pixels(unsigned int id){",
        "    return id<QI_COUNT ? atlas_64[id] : 0;",
        "}",
        "const uint32_t *q_icon_pixels_sized(unsigned int id,int size){",
        "    if(id>=QI_COUNT)return 0;",
        "    if(size==25)return atlas_25[id];",
        "    if(size==27)return atlas_27[id];",
        "    if(size==28)return atlas_28[id];",
        "    if(size==40)return atlas_40[id];",
        "    if(size==43)return atlas_43[id];",
        "    if(size==58)return atlas_58[id];",
        "    return atlas_64[id];",
        "}",
    ))
    pathlib.Path(output).write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    build(*sys.argv[1:3])
