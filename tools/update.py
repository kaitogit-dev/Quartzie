import argparse
import hashlib
import pathlib
import struct

def validate(payload):
    if len(payload)<256 or payload[:2]!=b"MZ":
        raise ValueError("Not an EFI executable")
    pe=struct.unpack_from("<I",payload,0x3c)[0]
    if pe<64 or pe+96>len(payload) or payload[pe:pe+4]!=b"PE\0\0":
        raise ValueError("Invalid PE header")
    if struct.unpack_from("<H",payload,pe+4)[0]!=0x8664:
        raise ValueError("UEFI executable must be x86-64")
    if struct.unpack_from("<H",payload,pe+24)[0]!=0x20b or struct.unpack_from("<H",payload,pe+24+68)[0]!=10:
        raise ValueError("PE subsystem is not EFI Application")

def pack(source, output, version):
    payload=pathlib.Path(source).read_bytes()
    validate(payload)
    if not version or len(version)>15 or any(c not in "0123456789.-" for c in version):
        raise ValueError("Invalid version")
    if len(payload)+64>8*1024*1024:
        raise ValueError("Update package exceeds 8 MiB")
    header=bytearray(64)
    header[:5]=b"QUPD\x01"
    header[5:5+len(version)]=version.encode("ascii")
    struct.pack_into("<I",header,21,len(payload))
    header[25:57]=hashlib.sha256(payload).digest()
    pathlib.Path(output).write_bytes(header+payload)

def check(path):
    data=pathlib.Path(path).read_bytes()
    if len(data)<320 or data[:5]!=b"QUPD\x01":
        raise ValueError("Invalid update header")
    version=data[5:21].split(b"\0",1)[0].decode("ascii")
    payload=data[64:]
    if struct.unpack_from("<I",data,21)[0]!=len(payload) or hashlib.sha256(payload).digest()!=data[25:57]:
        raise ValueError("Payload digest mismatch")
    validate(payload)
    print(f"Verified Quartzie {version}: {len(payload)} bytes")

def main():
    parser=argparse.ArgumentParser()
    sub=parser.add_subparsers(dest="command",required=True)
    make=sub.add_parser("pack")
    make.add_argument("firmware")
    make.add_argument("-o","--output",default="QUARTZIE.UPD")
    make.add_argument("--version",required=True)
    verify=sub.add_parser("check")
    verify.add_argument("package")
    args=parser.parse_args()
    if args.command=="pack":
        pack(args.firmware,args.output,args.version)
        check(args.output)
    else:
        check(args.package)

if __name__=="__main__":
    main()
