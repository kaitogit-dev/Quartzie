import pathlib
import struct
import sys


def u16(data, offset):
    return struct.unpack_from('<H', data, offset)[0]


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def boot_offset(image):
    if image[512:520] == b'EFI PART':
        partition = struct.unpack_from('<Q', image, 1024 + 32)[0]
        return partition * 512
    if image[16*2048+1:16*2048+6] != b'CD001':
        raise ValueError('Unsupported boot image')
    for sector in range(16, 40):
        record = image[sector*2048:(sector+1)*2048]
        if record[:1] != b'\0' or record[1:6] != b'CD001' or record[7:30] != b'EL TORITO SPECIFICATION':
            continue
        catalog_lba = u32(record, 71)
        catalog = image[catalog_lba*2048:(catalog_lba+1)*2048]
        if catalog[1] == 0xef and catalog[32] == 0x88:
            return u32(catalog, 40) * 2048
        for offset in range(64, len(catalog)-64, 32):
            if catalog[offset] in (0x90, 0x91) and catalog[offset+1] == 0xef and catalog[offset+32] == 0x88:
                return u32(catalog, offset+40) * 2048
    raise ValueError('UEFI boot catalog not found')


def replace(image, executable):
    if executable[:2] != b'MZ' or len(executable) < 128:
        raise ValueError('Expected a Windows PE executable')
    header = u32(executable, 60)
    if header + 26 > len(executable) or executable[header:header+4] != b'PE\0\0':
        raise ValueError('Invalid PE header')
    if u16(executable, header+4) != 0x8664 or u16(executable, header+24) != 0x20b:
        raise ValueError('Expected a PE32+ AMD64 executable')
    base = boot_offset(image)
    kind = image[base+82:base+90]
    fat32 = kind == b'FAT32   '
    if not fat32 and image[base+54:base+62] != b'FAT16   ':
        raise ValueError('EFI filesystem not recognized')
    reserved = u16(image, base+14)
    fats = image[base+16]
    fat_sectors = u32(image, base+36) if fat32 else u16(image, base+22)
    root_sectors = 0 if fat32 else (u16(image, base+17)*32+511)//512
    data_sector = reserved+fats*fat_sectors+root_sectors
    volume_sectors = u32(image, base+32) if fat32 else u16(image, base+19)
    available_clusters = volume_sectors-data_sector
    fat_start = base + reserved*512
    table = bytearray(image[fat_start:fat_start+fat_sectors*512])
    root = base + data_sector*512 if fat32 else base+(reserved+fats*fat_sectors)*512
    if image[root+32:root+43] != b'QSTART  EXE':
        raise ValueError('QSTART.EXE entry not found')
    entry = root+32
    previous = (u16(image, entry+20)<<16 if fat32 else 0)|u16(image, entry+26)
    item_size = 4 if fat32 else 2
    fat_mask = 0x0fffffff if fat32 else 0xffff
    eoc = 0x0ffffff8 if fat32 else 0xfff8

    def get_cluster(cluster):
        value = struct.unpack_from('<I' if fat32 else '<H', table, cluster*item_size)[0]
        return value & fat_mask

    def set_cluster(cluster, value):
        struct.pack_into('<I' if fat32 else '<H', table, cluster*item_size, value)

    seen = set()
    cluster = previous
    while 2 <= cluster < available_clusters+2 and cluster not in seen:
        seen.add(cluster)
        nxt = get_cluster(cluster)
        set_cluster(cluster, 0)
        if nxt >= eoc:
            break
        cluster = nxt
    needed = (len(executable)+511)//512
    free = []
    for cluster in range(2, available_clusters+2):
        if get_cluster(cluster) == 0:
            free.append(cluster)
            if len(free) == needed:
                break
    if len(free) < needed:
        raise ValueError('Not enough space in EFI partition')
    for index, cluster in enumerate(free):
        nxt = free[index+1] if index+1<len(free) else fat_mask
        set_cluster(cluster, nxt)
        offset = base+(data_sector+cluster-2)*512
        block = executable[index*512:(index+1)*512]
        image[offset:offset+512] = block.ljust(512, b'\0')
    for i in range(fats):
        offset = base + (reserved+i*fat_sectors)*512
        image[offset:offset+len(table)] = table
    first = free[0]
    if fat32:
        struct.pack_into('<H', image, entry+20, first>>16)
    struct.pack_into('<H', image, entry+26, first&0xffff)
    struct.pack_into('<I', image, entry+28, len(executable))


if __name__ == '__main__':
    if len(sys.argv) != 4:
        raise SystemExit('Usage: replace_app.py input.iso|input.img application.exe output.iso|output.img')
    image = bytearray(pathlib.Path(sys.argv[1]).read_bytes())
    executable = pathlib.Path(sys.argv[2]).read_bytes()
    replace(image, executable)
    pathlib.Path(sys.argv[3]).write_bytes(image)
    print('Updated QSTART.EXE')
