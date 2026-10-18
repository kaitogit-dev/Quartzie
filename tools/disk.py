import pathlib
import struct
import sys
import zlib

SECTOR = 512
SECTORS = 32768
MAGIC = b'CINMIXFS'
DIR_MAGIC = b'CMXDIR01'

def generate(path):
    destination = pathlib.Path(path)
    if destination.exists():
        raise SystemExit(f'Refusing to overwrite existing disk: {destination}')
    with destination.open('xb') as disk:
        disk.truncate(SECTOR * SECTORS)
        head = bytearray(SECTOR)
        head[:8] = MAGIC
        struct.pack_into('<IIIII', head, 8, 1, SECTORS, 16, 64, 16)
        struct.pack_into('<I', head, 28, zlib.crc32(head[:28]))
        disk.seek(0)
        disk.write(head)
        directory = bytearray(SECTOR)
        directory[:8] = DIR_MAGIC
        struct.pack_into('<I', directory, 8, 1)
        struct.pack_into('<I', directory, 12, zlib.crc32(directory[16:]))
        disk.write(directory)
        disk.write(directory)
    print(f'Created dedicated Quartzie persistent disk: {destination} ({SECTOR * SECTORS} bytes)')

if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: python3 tools/disk.py <new-image.img>')
    generate(sys.argv[1])
