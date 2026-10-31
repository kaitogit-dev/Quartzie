import pathlib
import struct
import sys
import tempfile
import qapp

def create(path):
    with tempfile.TemporaryDirectory() as directory:
        exe=pathlib.Path(directory)/"sample.exe"
        data=bytearray(512)
        data[:2]=b"MZ"
        struct.pack_into("<I",data,0x3c,0x80)
        data[0x80:0x84]=b"PE\x00\x00"
        struct.pack_into("<HH",data,0x84,0x8664,1)
        struct.pack_into("<H",data,0x94,240)
        struct.pack_into("<H",data,0x98,0x20b)
        exe.write_bytes(data)
        qapp.pack(exe,path,"Quartzie Store Test","quartzie.storetest","1.0")

if __name__=="__main__":
    create(sys.argv[1])
