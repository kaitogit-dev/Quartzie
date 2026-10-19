import pathlib
import struct
import sys
from replace_app import boot_offset


def check(path, exe):
    image = pathlib.Path(path).read_bytes()
    base = boot_offset(image)
    fat32 = image[base+82:base+90] == b'FAT32   '
    reserved = struct.unpack_from('<H', image, base+14)[0]
    fats = image[base+16]
    fat_sectors = struct.unpack_from('<I' if fat32 else '<H', image, base+(36 if fat32 else 22))[0]
    root_sectors = 0 if fat32 else (struct.unpack_from('<H', image, base+17)[0]*32+511)//512
    data_sector = reserved+fats*fat_sectors+root_sectors
    root = base + (data_sector if fat32 else reserved+fats*fat_sectors)*512
    entry = image[root+32:root+64]
    assert entry[:11] == b'QSTART  EXE'
    length = struct.unpack_from('<I', entry, 28)[0]
    assert length == len(exe)
    first = struct.unpack_from('<H', entry, 26)[0]
    if fat32:
        first |= struct.unpack_from('<H', entry, 20)[0]<<16
    result = bytearray()
    visited = set()
    cluster = first
    fat = base + reserved*512
    while len(result)<length:
        assert cluster >= 2 and cluster not in visited
        visited.add(cluster)
        offset = base+(data_sector+cluster-2)*512
        assert offset+512<=len(image)
        result.extend(image[offset:offset+512])
        cluster = struct.unpack_from('<I' if fat32 else '<H', image, fat+cluster*(4 if fat32 else 2))[0]
        if fat32:
            cluster &= 0x0fffffff
    assert result[:length] == exe
    print('PASS: boot volume contains real Windows executable', pathlib.Path(path).name)


if __name__ == '__main__':
    exe=pathlib.Path(sys.argv[1]).read_bytes()
    for image in sys.argv[2:]:
        check(image, exe)
