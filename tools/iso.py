import math
import pathlib
import struct
import sys

SECTOR = 2048
CATALOG_LBA = 20
ROOT_LBA = 21
PATH_L_LBA = 22
PATH_M_LBA = 23
ESP_LBA = 32


def both16(value):
    return struct.pack('<H', value) + struct.pack('>H', value)


def both32(value):
    return struct.pack('<I', value) + struct.pack('>I', value)


def record(lba, size, name, flags=0):
    encoded = name.encode('ascii') if isinstance(name, str) else name
    length = 33 + len(encoded) + (len(encoded) % 2 == 0)
    result = bytearray(length)
    result[0] = length
    result[2:10] = both32(lba)
    result[10:18] = both32(size)
    result[18:25] = bytes([126, 10, 19, 0, 0, 0, 0])
    result[25] = flags
    result[28:32] = both16(1)
    result[32] = len(encoded)
    result[33:33+len(encoded)] = encoded
    return result


def descriptor(kind, ident=b'CD001'):
    result = bytearray(SECTOR)
    result[0] = kind
    result[1:6] = ident
    result[6] = 1
    return result


def create(esp):
    if len(esp) % SECTOR:
        esp += bytes(SECTOR - len(esp) % SECTOR)
    esp_sectors = len(esp) // SECTOR
    sectors = ESP_LBA + esp_sectors
    iso = bytearray(sectors * SECTOR)
    main = descriptor(1)
    main[8:40] = b'QUARTZIE'.ljust(32, b' ')
    main[40:72] = b'QUARTZIE_X64_UEFI'.ljust(32, b' ')
    main[80:88] = both32(sectors)
    main[120:124] = both16(1)
    main[124:128] = both16(1)
    main[128:132] = both16(SECTOR)
    main[132:140] = both32(10)
    struct.pack_into('<I', main, 140, PATH_L_LBA)
    struct.pack_into('>I', main, 148, PATH_M_LBA)
    main[156:190] = record(ROOT_LBA, SECTOR, b'\x00', 2)
    main[881] = 1
    boot = descriptor(0)
    boot[7:30] = b'EL TORITO SPECIFICATION'
    struct.pack_into('<I', boot, 71, CATALOG_LBA)
    iso[16*SECTOR:17*SECTOR] = main
    iso[17*SECTOR:18*SECTOR] = boot
    iso[18*SECTOR:19*SECTOR] = descriptor(255)
    catalog = bytearray(SECTOR)
    catalog[0] = 1
    catalog[1] = 0xef
    catalog[4:4+18] = b'QUARTZIE UEFI BOOT'
    catalog[30:32] = b'\x55\xaa'
    validation = struct.unpack('<16H', catalog[:32])
    struct.pack_into('<H', catalog, 28, (-sum(validation)) & 0xffff)
    catalog[32] = 0x88
    catalog[33] = 0
    struct.pack_into('<H', catalog, 38, 4)
    struct.pack_into('<I', catalog, 40, ESP_LBA)
    iso[CATALOG_LBA*SECTOR:(CATALOG_LBA+1)*SECTOR] = catalog
    root = bytearray(SECTOR)
    parts = [record(ROOT_LBA, SECTOR, b'\x00', 2),
             record(ROOT_LBA, SECTOR, b'\x01', 2),
             record(CATALOG_LBA, SECTOR, 'BOOT.CAT;1'),
             record(ESP_LBA, len(esp), 'EFI.IMG;1')]
    offset = 0
    for part in parts:
        root[offset:offset+len(part)] = part
        offset += len(part)
    iso[ROOT_LBA*SECTOR:(ROOT_LBA+1)*SECTOR] = root
    path = bytearray(10)
    path[0] = 1
    struct.pack_into('<I', path, 2, ROOT_LBA)
    struct.pack_into('<H', path, 6, 1)
    iso[PATH_L_LBA*SECTOR:PATH_L_LBA*SECTOR+10] = path
    struct.pack_into('>I', path, 2, ROOT_LBA)
    struct.pack_into('>H', path, 6, 1)
    iso[PATH_M_LBA*SECTOR:PATH_M_LBA*SECTOR+10] = path
    iso[ESP_LBA*SECTOR:] = esp
    return iso


if __name__ == '__main__':
    pathlib.Path(sys.argv[2]).write_bytes(create(pathlib.Path(sys.argv[1]).read_bytes()))
