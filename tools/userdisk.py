import argparse
import pathlib
import struct

SECTORS=65536
FAT_SECTORS=256
ROOT_SECTORS=32
RESERVED=1
START=RESERVED+2*FAT_SECTORS+ROOT_SECTORS
MARKER=b"QUARTZIE USER V1"

def create():
    image=bytearray(SECTORS*512)
    boot=memoryview(image)[:512]
    boot[:3]=b"\xeb\x3c\x90"
    boot[3:11]=b"QUARTZIE"
    struct.pack_into("<HBHBHHBHHHII",boot,11,512,1,RESERVED,2,512,0,0xf8,FAT_SECTORS,63,255,0,SECTORS)
    boot[36:39]=b"\x80\x00\x29"
    struct.pack_into("<I",boot,39,0x51555352)
    boot[43:54]=b"QUARTZIE   "
    boot[54:62]=b"FAT16   "
    boot[510:512]=b"\x55\xaa"
    fat=bytearray(FAT_SECTORS*512)
    for index,value in ((0,0xfff8),(1,0xffff),(2,0xffff)):
        struct.pack_into("<H",fat,2*index,value)
    image[RESERVED*512:(RESERVED+FAT_SECTORS)*512]=fat
    image[(RESERVED+FAT_SECTORS)*512:(RESERVED+2*FAT_SECTORS)*512]=fat
    root=(RESERVED+2*FAT_SECTORS)*512
    image[root:root+11]=b"QUSER   DAT"
    image[root+11]=0x20
    struct.pack_into("<H",image,root+26,2)
    struct.pack_into("<I",image,root+28,len(MARKER))
    image[START*512:START*512+len(MARKER)]=MARKER
    return image

def main():
    p=argparse.ArgumentParser()
    p.add_argument("output")
    args=p.parse_args()
    pathlib.Path(args.output).write_bytes(create())

if __name__=="__main__":
    main()
