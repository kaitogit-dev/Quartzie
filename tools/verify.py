import pathlib
import struct
import sys
import zlib


def verify(iso_path, disk_path, esp_path, efi_path, iso_esp_path):
    efi = pathlib.Path(efi_path).read_bytes()
    iso = pathlib.Path(iso_path).read_bytes()
    disk = pathlib.Path(disk_path).read_bytes()
    esp = pathlib.Path(esp_path).read_bytes()
    optical = pathlib.Path(iso_esp_path).read_bytes()
    assert efi[:2] == b'MZ'
    pe = struct.unpack_from('<I', efi, 0x3c)[0]
    assert efi[pe:pe+4] == b'PE\0\0'
    assert struct.unpack_from('<H', efi, pe+4)[0] == 0x8664
    assert struct.unpack_from('<H', efi, pe+24)[0] == 0x20b
    assert struct.unpack_from('<H', efi, pe+24+68)[0] == 10
    assert iso[16*2048+1:16*2048+6] == b'CD001'
    catalogs = []
    for sector in range(16, 40):
        data = iso[sector*2048:(sector+1)*2048]
        if data[0] == 0 and data[1:6] == b'CD001' and data[7:30] == b'EL TORITO SPECIFICATION':
            catalogs.append(struct.unpack_from('<I', data, 71)[0])
    assert len(catalogs) == 1
    catalog = iso[catalogs[0]*2048:(catalogs[0]+1)*2048]
    assert sum(struct.unpack('<16H', catalog[:32])) % 65536 == 0
    candidates = []
    if catalog[1] == 0xef and catalog[32] == 0x88:
        candidates.append(struct.unpack_from('<I', catalog, 40)[0])
    for offset in range(64, 2048-32, 32):
        if catalog[offset] in (0x90, 0x91) and catalog[offset+1] == 0xef and catalog[offset+32] == 0x88:
            candidates.append(struct.unpack_from('<I', catalog, offset+40)[0])
    assert candidates
    assert any(iso[lba*2048:lba*2048+len(optical)] == optical for lba in candidates)
    assert len(optical) == 8192 * 512
    assert optical[510:512] == b'\x55\xaa'
    assert optical[54:62] == b'FAT16   '
    assert struct.unpack_from('<H', optical, 17)[0] == 512
    assert struct.unpack_from('<H', optical, 22)[0] == 32
    root_offset = (1 + 2 * 32) * 512
    assert optical[root_offset:root_offset + 11] == b'EFI        '
    data_start = (1 + 2 * 32 + 32) * 512
    assert optical[data_start + 2 * 512 + 64:data_start + 2 * 512 + 75] == b'BOOTX64 EFI'
    assert optical[data_start + 2 * 512: data_start + 2 * 512 + 11] == b'.          '
    assert optical[data_start + 3 * 512:data_start + 3 * 512 + len(efi)] == efi
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
    assert disk[450] == 0xee and struct.unpack_from('<I',disk,454)[0] == 1
    assert disk[2048*512:2048*512+len(esp)] == esp
    crc=struct.unpack_from('<I', disk, 512+16)[0]
    hdr=bytearray(disk[512:512+92]);hdr[16:20]=b'\0'*4
    assert zlib.crc32(hdr)==crc
    print('PASS: PE32+ AMD64, EFI application, FAT16 optical image, FAT32 GPT partition, UEFI El Torito ISO')


if __name__ == '__main__':
    verify(*sys.argv[1:])
