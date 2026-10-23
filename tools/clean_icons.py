import io
import pathlib
import sys
import zipfile
from PIL import Image

SIZES = ((16, 16), (24, 24), (32, 32), (48, 48), (64, 64))


def clean(image):
    image = image.convert("RGBA")
    pixels = bytearray(image.tobytes())
    for offset in range(0, len(pixels), 4):
        if pixels[offset + 3] < 16:
            pixels[offset:offset + 4] = b"\0\0\0\0"
    return Image.frombytes("RGBA", image.size, bytes(pixels))


def clean_archive(path, check=False):
    contents = {}
    with zipfile.ZipFile(path) as archive:
        for filename in archive.namelist():
            if not filename.endswith(".ico"):
                raise ValueError(filename)
            with Image.open(io.BytesIO(archive.read(filename))) as image:
                master = clean(image.ico.getimage((64, 64)))
            frames = [clean(master.resize(size, Image.Resampling.LANCZOS)) for size in SIZES[:-1]]
            output = io.BytesIO()
            master.save(output, format="ICO", sizes=SIZES, append_images=frames, compress_level=9)
            contents[filename] = output.getvalue()
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w") as archive:
        for name, binary in sorted(contents.items()):
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            archive.writestr(info, binary, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    data = output.getvalue()
    if path.read_bytes() != data:
        if check:
            raise ValueError("Icon archive needs cleanup")
        path.write_bytes(data)


if __name__ == "__main__":
    clean_archive(pathlib.Path("assets/icons.zip"), "--check" in sys.argv[1:])
