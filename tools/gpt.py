import pathlib
import struct
import sys
import uuid
import zlib


def write_gpt(esp_file, destination):
    esp = pathlib.Path(esp_file).read_bytes()
    if len(esp) % 512:
        raise ValueError('Partition must have sector alignment')
    count = len(esp) // 512
    start = 2048
    sectors = start + count + 34
    disk = bytearray(sectors * 512)
    mbr = memoryview(disk)[:512]
    mbr[446:462] = bytes.fromhex('000200eeffffff00') + struct.pack('<II', 1, min(sectors-1, 0xffffffff))
    mbr[510:512] = b'\x55\xaa'
    entries = bytearray(128 * 128)
    entries[0:16] = uuid.UUID('c12a7328-f81f-11d2-ba4b-00a0c93ec93b').bytes_le
    entries[16:32] = uuid.UUID('4ba539dd-b7fe-46d7-afec-95f55afcc091').bytes_le
    struct.pack_into('<QQQ', entries, 32, start, start + count - 1, 0)
    entries[56:56 + 2 * len('QUARTZIE EFI')] = 'QUARTZIE EFI'.encode('utf-16le')
    crc = zlib.crc32(entries)
    disk[2*512:34*512] = entries
    disk[(sectors-33)*512:(sectors-1)*512] = entries

    def header(current, backup, table):
        buf = bytearray(512)
        struct.pack_into('<8sIIIIQQQQ16sQIII', buf, 0,
                         b'EFI PART', 0x00010000, 92, 0, 0, current, backup,
                         start, sectors - 34,
                         uuid.UUID('6cc04253-82a3-4d14-8cd4-683c369f5cdf').bytes_le,
                         table, 128, 128, crc)
        struct.pack_into('<I', buf, 16, zlib.crc32(buf[:92]))
        return buf

    disk[512:1024] = header(1, sectors-1, 2)
    disk[(sectors-1)*512:] = header(sectors-1, 1, sectors-33)
    disk[start*512:(start+count)*512] = esp
    pathlib.Path(destination).write_bytes(disk)


if __name__ == '__main__':
    write_gpt(sys.argv[1], sys.argv[2])
