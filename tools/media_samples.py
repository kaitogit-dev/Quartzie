import math
import struct

def chunk(name, payload):
    return name + struct.pack("<I", len(payload)) + payload + (b"\x00" if len(payload) & 1 else b"")

def samples():
    width, height = 160, 90
    raw = bytearray()
    for y in range(height - 1, -1, -1):
        for x in range(width):
            dx = x - 90
            dy = y - 48
            sun = dx * dx + dy * dy < 22 * 22
            raw.extend((72 + y * 2 if not sun else 106, 70 + x // 3 if not sun else 212, 64 + x if not sun else 255))
    bmp = bytearray(54 + len(raw))
    bmp[:2] = b"BM"
    struct.pack_into("<I", bmp, 2, len(bmp))
    struct.pack_into("<I", bmp, 10, 54)
    struct.pack_into("<IiiHHII", bmp, 14, 40, width, height, 1, 24, 0, len(raw))
    bmp[54:] = raw
    data = bytes(int(128 + 90 * math.sin(2 * math.pi * 440 * i / 8000)) for i in range(8000))
    wav = b"WAVE" + chunk(b"fmt ", struct.pack("<HHIIHH", 1, 1, 8000, 8000, 1, 8)) + chunk(b"data", data)
    wav = b"RIFF" + struct.pack("<I", len(wav)) + wav
    vw, vh, count, fps = 96, 64, 16, 8
    avih = bytearray(56)
    struct.pack_into("<I", avih, 0, 1000000 // fps)
    struct.pack_into("<I", avih, 16, count)
    struct.pack_into("<I", avih, 24, 1)
    struct.pack_into("<II", avih, 32, vw, vh)
    bitmap = struct.pack("<IiiHHIIiiII", 40, vw, vh, 1, 24, 0, vw * vh * 3, 0, 0, 0, 0)
    frames = bytearray()
    for frame in range(count):
        pixels = bytearray()
        for y in range(vh - 1, -1, -1):
            for x in range(vw):
                on = abs(x - (frame * 6 + 8) % vw) < 10 and abs(y - 32) < 10
                pixels.extend((240 if on else x * 2, 200 if on else y * 3, 90 if on else 35 + frame * 8))
        frames.extend(chunk(b"00db", pixels))
    hdrl = chunk(b"avih", avih) + chunk(b"LIST", b"strl" + chunk(b"strf", bitmap))
    avi = b"AVI " + chunk(b"LIST", b"hdrl" + hdrl) + chunk(b"LIST", b"movi" + frames)
    avi = b"RIFF" + struct.pack("<I", len(avi)) + avi
    return (("PHOTO   BMP", bytes(bmp)), ("SOUND   WAV", wav), ("CLIP    AVI", avi))
