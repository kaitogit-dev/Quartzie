import pathlib
import struct
import sys

def verify(path):
    data=pathlib.Path(path).read_bytes()
    if len(data)<256 or data[:2]!=b"MZ":
        raise ValueError("Expected PE image")
    offset=struct.unpack_from("<I",data,0x3c)[0]
    if offset+160>len(data) or data[offset:offset+4]!=b"PE\0\0":
        raise ValueError("Invalid PE header")
    machine,sections=struct.unpack_from("<HH",data,offset+4)
    header=offset+24
    magic=struct.unpack_from("<H",data,header)[0]
    virtual_size=struct.unpack_from("<I",data,header+56)[0]
    subsystem=struct.unpack_from("<H",data,header+68)[0]
    if machine!=0x8664 or magic!=0x20b or subsystem!=10:
        raise ValueError("Expected x64 UEFI executable")
    if sections<2 or sections>20:
        raise ValueError("Unexpected PE section count")
    if virtual_size>16*1024*1024:
        raise ValueError(f"Firmware image reserves excessive virtual memory: {virtual_size}")
    print(f"PASS: PE32+ EFI layout, virtual image {virtual_size:,} bytes, {sections} sections")

if __name__=="__main__":
    verify(sys.argv[1])
