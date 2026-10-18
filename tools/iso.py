import pathlib
import struct
import sys

BLOCK = 2048
BOOT_LBA = 32
KERNEL_BLOCKS = 160
CATALOG_LBA = 20
ROOT_LBA = 22
README_LBA = BOOT_LBA + 1 + KERNEL_BLOCKS


def dual16(value):
    return struct.pack('<H', value) + struct.pack('>H', value)


def dual32(value):
    return struct.pack('<I', value) + struct.pack('>I', value)


def datetime7():
    return bytes([126, 10, 14, 0, 0, 0, 0])


def record(name, extent, length, flags=0):
    identifier = name if isinstance(name, bytes) else name.encode('ascii')
    pad = b'\0' if not len(identifier) % 2 else b''
    size = 33 + len(identifier) + len(pad)
    return bytes([size, 0]) + dual32(extent) + dual32(length) + datetime7() + bytes([flags, 0, 0]) + dual16(1) + bytes([len(identifier)]) + identifier + pad


def padded(content, blocks=1):
    if len(content) > blocks * BLOCK:
        raise ValueError('Sector capacity exceeded')
    return content.ljust(blocks * BLOCK, b'\0')


def create(bootpath, kernelpath, readmepath, destination):
    boot = pathlib.Path(bootpath).read_bytes()
    kernel = pathlib.Path(kernelpath).read_bytes()
    readme = pathlib.Path(readmepath).read_bytes()
    if len(boot) != 512 or boot[510:512] != b'\x55\xaa':
        raise ValueError('Invalid CD boot sector')
    if len(kernel) > KERNEL_BLOCKS * BLOCK:
        raise ValueError('Kernel is too large')
    if len(readme) > BLOCK:
        raise ValueError('README is too large')
    sectors = README_LBA + 1
    image = bytearray(sectors * BLOCK)

    def setblock(index, data):
        if len(data) > BLOCK:
            raise ValueError('Block overflow')
        image[index * BLOCK:index * BLOCK + len(data)] = data

    entries = [record(b'\0', ROOT_LBA, BLOCK, 2), record(b'\1', ROOT_LBA, BLOCK, 2), record('BOOT.CAT;1', CATALOG_LBA, BLOCK), record('QUARTZIE.BIN;1', BOOT_LBA, (1 + KERNEL_BLOCKS) * BLOCK), record('README.TXT;1', README_LBA, len(readme))]
    directory = b''.join(entries)
    setblock(ROOT_LBA, directory)

    pvd = bytearray(BLOCK)
    pvd[0:7] = b'\x01CD001\x01'
    pvd[8:40] = b'QUARTZIE'.ljust(32, b' ')
    pvd[40:72] = b'QUARTZIE_0_31'.ljust(32, b' ')
    pvd[80:88] = dual32(sectors)
    pvd[120:124] = dual16(1)
    pvd[124:128] = dual16(1)
    pvd[128:132] = dual16(BLOCK)
    pvd[132:140] = dual32(10)
    struct.pack_into('<I', pvd, 140, 19)
    struct.pack_into('>I', pvd, 148, 21)
    pvd[156:190] = record(b'\0', ROOT_LBA, BLOCK, 2)
    pvd[881] = 1
    setblock(16, pvd)

    boot_record = bytearray(BLOCK)
    boot_record[0:7] = b'\x00CD001\x01'
    boot_record[7:39] = b'EL TORITO SPECIFICATION'.ljust(32, b'\0')
    struct.pack_into('<I', boot_record, 71, CATALOG_LBA)
    setblock(17, boot_record)
    setblock(18, b'\xffCD001\x01')

    path_l = bytes([1, 0]) + struct.pack('<I', ROOT_LBA) + struct.pack('<H', 1) + b'\0\0'
    path_b = bytes([1, 0]) + struct.pack('>I', ROOT_LBA) + struct.pack('>H', 1) + b'\0\0'
    setblock(19, path_l)
    setblock(21, path_b)

    validation = bytearray(32)
    validation[0] = 1
    validation[4:28] = b'QUARTZIE 0.31'.ljust(24, b' ')
    validation[30:32] = b'\x55\xaa'
    checksum = -sum(struct.unpack('<16H', validation)) & 0xffff
    struct.pack_into('<H', validation, 28, checksum)
    initial = bytearray(32)
    initial[0] = 0x88
    struct.pack_into('<H', initial, 6, 4)
    struct.pack_into('<I', initial, 8, BOOT_LBA)
    setblock(CATALOG_LBA, validation + initial)

    setblock(BOOT_LBA, padded(boot))
    offset = (BOOT_LBA + 1) * BLOCK
    image[offset:offset + KERNEL_BLOCKS * BLOCK] = padded(kernel, KERNEL_BLOCKS)
    setblock(README_LBA, readme)
    pathlib.Path(destination).write_bytes(image)
    print(f'Created {destination}: {len(image)} bytes, {len(kernel)} kernel bytes')


if __name__ == '__main__':
    create(*sys.argv[1:])
