import pathlib
import struct
import sys
import zlib

BLOCK = 512
NAME = b'hello.exe'


def install(diskname, executable):
    path = pathlib.Path(diskname)
    image = pathlib.Path(executable).read_bytes()
    if not image or len(image) > 4096:
        raise ValueError('Executable must be 1-4096 bytes for QuartzieFS v1')
    with path.open('r+b') as disk:
        header = disk.read(BLOCK)
        if (header[:8] != b'CINMIXFS' or zlib.crc32(header[:28]) != struct.unpack_from('<I', header, 28)[0]):
            raise ValueError('Refusing to write an unsigned/non-QuartzieFS disk image')
        if tuple(struct.unpack_from('<IIIII', header, 8)) != (1, 32768, 16, 64, 16):
            raise ValueError('Unexpected filesystem geometry')
        dirs = []
        for sector in (1, 2):
            disk.seek(sector * BLOCK)
            directory = bytearray(disk.read(BLOCK))
            valid = len(directory) == BLOCK and directory[:8] == b'CMXDIR01' and zlib.crc32(directory[16:]) == struct.unpack_from('<I', directory, 12)[0]
            if valid:
                dirs.append((sector, directory, struct.unpack_from('<I', directory, 8)[0]))
        if not dirs:
            raise ValueError('Missing or corrupted QuartzieFS directory')
        latest_sector, directory, sequence = max(dirs, key=lambda d: d[2])
        slot = -1
        for idx in range(16):
            at = 16 + idx*28
            if directory[at+23] == 1 and directory[at:at+20].split(b'\0',1)[0] == NAME:
                slot=idx
                break
        if slot < 0:
            for idx in range(16):
                if directory[16+idx*28+23] == 0:
                    slot=idx
                    break
        if slot < 0:
            raise ValueError('No empty filesystem slots')
        pos = 16+slot*28
        old_active = directory[pos+23] & 1
        bank = 1 - directory[pos+22] if old_active else 0
        lba = 64 + slot*16 + bank*8
        disk.seek(lba*BLOCK)
        disk.write(image.ljust(((len(image)+BLOCK-1)//BLOCK)*BLOCK,b'\0'))
        directory[pos:pos+20] = NAME.ljust(20,b'\0')
        struct.pack_into('<H',directory,pos+20,len(image))
        directory[pos+22]=bank
        directory[pos+23]=1
        struct.pack_into('<I',directory,pos+24,zlib.crc32(image))
        struct.pack_into('<I',directory,8,sequence+1)
        struct.pack_into('<I',directory,12,zlib.crc32(directory[16:]))
        disk.seek((3-latest_sector)*BLOCK)
        disk.write(directory)
        disk.flush()
        print(f'Installed {NAME.decode()} ({len(image)} bytes) in QuartzieFS slot {slot}')


if __name__=='__main__':
    if len(sys.argv)!=3:
        raise SystemExit('Usage: python3 tools/install_pe.py <disk.img> <hello.exe>')
    install(sys.argv[1], sys.argv[2])
