import pathlib
import struct
import sys
import zlib


def verify(iso_path, disk_path, esp_path, efi_path):
    efi = pathlib.Path(efi_path).read_bytes()
    iso = pathlib.Path(iso_path).read_bytes()
    disk = pathlib.Path(disk_path).read_bytes()
    esp = pathlib.Path(esp_path).read_bytes()
    assert efi[:2] == b'MZ'
    pe = struct.unpack_from('<I', efi, 0x3c)[0]
    assert efi[pe:pe+4] == b'PE\0\0'
    assert struct.unpack_from('<H', efi, pe+4)[0] == 0x8664
    assert struct.unpack_from('<H', efi, pe+24)[0] == 0x20b
    assert struct.unpack_from('<H', efi, pe+24+68)[0] == 10
    assert iso[16*2048+1:16*2048+6] == b'CD001'
    assert iso[17*2048+7:17*2048+30] == b'EL TORITO SPECIFICATION'
    catalog = iso[20*2048:21*2048]
    assert catalog[1] == 0xef and catalog[32] == 0x88
    assert sum(struct.unpack('<16H', catalog[:32])) % 65536 == 0
    boot_lba = struct.unpack_from('<I', catalog, 40)[0]
    assert iso[boot_lba*2048:boot_lba*2048+len(esp)] == esp
    assert esp[:3] == b'\xeb\x58\x90' and esp[510:512] == b'\x55\xaa'
    assert esp[82:90] == b'FAT32   '
    reserved = struct.unpack_from('<H', esp, 14)[0]
    fats = esp[16]
    size = struct.unpack_from('<I', esp, 36)[0]
    assert reserved == 32 and fats == 2 and size == 1024
    data_start = reserved + fats*size
    root = esp[(data_start)*512:(data_start+1)*512]
    assert root[:11] == b'EFI        '
    bootdir = esp[(data_start+2)*512:(data_start+3)*512]
    assert bootdir[64:75] == b'BOOTX64 EFI'
    assert esp[(data_start+3)*512:(data_start+3)*512+len(efi)] == efi
    assert disk[510:512] == b'\x55\xaa' and disk[512:520] == b'EFI PART'
    assert disk[2048*512:2048*512+len(esp)] == esp
    crc=struct.unpack_from('<I', disk, 512+16)[0]
    hdr=bytearray(disk[512:512+92]);hdr[16:20]=b'\0'*4
    assert zlib.crc32(hdr)==crc
    print('PASS: PE32+ AMD64, EFI application, FAT32 ESP, GPT, UEFI El Torito ISO')


if __name__ == '__main__':
    verify(*sys.argv[1:])
