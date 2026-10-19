import math
import pathlib
import struct
import sys

SECTORS = 131072
RESERVED = 32
FAT_SECTORS = 1024
DATA_START = RESERVED + 2 * FAT_SECTORS


def directory_entry(name, attr, cluster, length=0):
    result = bytearray(32)
    result[:11] = name.encode('ascii')
    result[11] = attr
    struct.pack_into('<H', result, 20, cluster >> 16)
    struct.pack_into('<H', result, 26, cluster & 65535)
    struct.pack_into('<I', result, 28, length)
    return result


def create(efi, application):
    file = pathlib.Path(efi).read_bytes()
    executable = pathlib.Path(application).read_bytes()
    clusters = math.ceil(len(file) / 512)
    app_clusters = math.ceil(len(executable) / 512)
    if clusters < 1 or clusters + app_clusters >= 10000:
        raise ValueError('EFI image size out of range')
    data = bytearray(SECTORS * 512)
    boot = memoryview(data)[:512]
    boot[:3] = b'\xeb\x58\x90'
    boot[3:11] = b'QUARTZIE'
    struct.pack_into('<HBHBHHBHHHII', boot, 11, 512, 1, RESERVED, 2, 0, 0, 0xf8, 0, 63, 255, 0, SECTORS)
    struct.pack_into('<IHHIHH', boot, 36, FAT_SECTORS, 0, 0, 2, 1, 6)
    boot[64:67] = b'\x80\x00\x29'
    struct.pack_into('<I', boot, 67, 0x12345678)
    boot[71:82] = b'QUARTZIE   '
    boot[82:90] = b'FAT32   '
    boot[510:512] = b'\x55\xaa'
    data[6 * 512:7 * 512] = data[:512]
    fsinfo = bytearray(512)
    struct.pack_into('<I', fsinfo, 0, 0x41615252)
    struct.pack_into('<IIII', fsinfo, 484, 0x61417272, 0xffffffff, 0xffffffff, 0xaa550000)
    data[512:1024] = fsinfo
    data[7 * 512:8 * 512] = fsinfo
    fat = bytearray(FAT_SECTORS * 512)
    for cluster, value in ((0, 0x0ffffff8), (1, 0xffffffff), (2, 0x0fffffff), (3, 0x0fffffff), (4, 0x0fffffff)):
        struct.pack_into('<I', fat, cluster * 4, value)
    for i in range(clusters):
        struct.pack_into('<I', fat, (i + 5) * 4, i + 6 if i + 1 < clusters else 0x0fffffff)
    start = 5 + clusters
    for i in range(app_clusters):
        struct.pack_into('<I', fat, (start + i) * 4, start + i + 1 if i + 1 < app_clusters else 0x0fffffff)
    data[RESERVED * 512:(RESERVED + FAT_SECTORS) * 512] = fat
    data[(RESERVED + FAT_SECTORS) * 512:DATA_START * 512] = fat

    def cluster_pos(n):
        return (DATA_START + n - 2) * 512

    data[cluster_pos(2):cluster_pos(2)+32] = directory_entry('EFI        ', 0x10, 3)
    data[cluster_pos(2)+32:cluster_pos(2)+64] = directory_entry('QSTART  EXE', 0x20, start, len(executable))
    data[cluster_pos(3):cluster_pos(3)+32] = directory_entry('.          ', 0x10, 3)
    data[cluster_pos(3)+32:cluster_pos(3)+64] = directory_entry('..         ', 0x10, 2)
    data[cluster_pos(3)+64:cluster_pos(3)+96] = directory_entry('BOOT       ', 0x10, 4)
    data[cluster_pos(4):cluster_pos(4)+32] = directory_entry('.          ', 0x10, 4)
    data[cluster_pos(4)+32:cluster_pos(4)+64] = directory_entry('..         ', 0x10, 3)
    data[cluster_pos(4)+64:cluster_pos(4)+96] = directory_entry('BOOTX64 EFI', 0x20, 5, len(file))
    data[cluster_pos(5):cluster_pos(5)+len(file)] = file
    data[cluster_pos(start):cluster_pos(start)+len(executable)] = executable
    return data


if __name__ == '__main__':
    pathlib.Path(sys.argv[2]).write_bytes(create(sys.argv[1], sys.argv[3]))
