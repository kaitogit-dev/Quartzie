import math
import pathlib
import struct
import sys

SECTORS = 8192
FAT_SECTORS = 32
ROOT_SECTORS = 32
DATA_START = 1 + 2 * FAT_SECTORS + ROOT_SECTORS


def entry(name, attributes, cluster, length=0):
    item = bytearray(32)
    item[:11] = name.encode('ascii')
    item[11] = attributes
    struct.pack_into('<H', item, 26, cluster)
    struct.pack_into('<I', item, 28, length)
    return item


def create(path):
    binary = pathlib.Path(path).read_bytes()
    count = math.ceil(len(binary) / 512)
    if count > 8000:
        raise ValueError('EFI executable exceeds compact image capacity')
    image = bytearray(SECTORS * 512)
    sector = memoryview(image)[:512]
    sector[:3] = b'\xeb\x3c\x90'
    sector[3:11] = b'QUARTZIE'
    struct.pack_into('<HBHBHHBHHHII', sector, 11, 512, 1, 1, 2, 512, SECTORS, 0xf8, FAT_SECTORS, 63, 255, 0, 0)
    sector[36:39] = b'\x80\x00\x29'
    struct.pack_into('<I', sector, 39, 0x12435678)
    sector[43:54] = b'QUARTZIE   '
    sector[54:62] = b'FAT16   '
    sector[510:512] = b'\x55\xaa'
    fat = bytearray(FAT_SECTORS * 512)
    for n, value in ((0, 0xfff8), (1, 0xffff), (2, 0xffff), (3, 0xffff)):
        struct.pack_into('<H', fat, n * 2, value)
    for i in range(count):
        struct.pack_into('<H', fat, (i + 4) * 2, i + 5 if i + 1 < count else 0xffff)
    image[512:(1 + FAT_SECTORS) * 512] = fat
    image[(1 + FAT_SECTORS) * 512:(1 + 2 * FAT_SECTORS) * 512] = fat

    def cluster(n):
        return (DATA_START + n - 2) * 512

    root = (1 + 2 * FAT_SECTORS) * 512
    image[root:root + 32] = entry('EFI        ', 0x10, 2)
    image[cluster(2):cluster(2) + 32] = entry('.          ', 0x10, 2)
    image[cluster(2) + 32:cluster(2) + 64] = entry('..         ', 0x10, 0)
    image[cluster(2) + 64:cluster(2) + 96] = entry('BOOT       ', 0x10, 3)
    image[cluster(3):cluster(3) + 32] = entry('.          ', 0x10, 3)
    image[cluster(3) + 32:cluster(3) + 64] = entry('..         ', 0x10, 2)
    image[cluster(3) + 64:cluster(3) + 96] = entry('BOOTX64 EFI', 0x20, 4, len(binary))
    image[cluster(4):cluster(4) + len(binary)] = binary
    return image


if __name__ == '__main__':
    pathlib.Path(sys.argv[2]).write_bytes(create(sys.argv[1]))
