import pathlib
import struct
import sys

def entries(image, offset, count):
    return [image[offset+i*32:offset+(i+1)*32] for i in range(count)
            if image[offset+i*32] not in (0, 0xe5)
            and image[offset+i*32+11] != 0x0f]

def check(path):
    image=pathlib.Path(path).read_bytes()
    if len(image)!=65536*512:
        raise ValueError("Invalid user disk size")
    sectors=struct.unpack_from("<H",image,22)[0]
    reserved=struct.unpack_from("<H",image,14)[0]
    root_entries=struct.unpack_from("<H",image,17)[0]
    root=(reserved+2*sectors)*512
    data=root+root_entries*32
    root_items=entries(image,root,root_entries)
    marker=[x for x in root_items if x[:11]==b"QUSER   DAT"]
    if len(marker)!=1:
        raise ValueError("Missing data volume marker")
    marker_cluster=struct.unpack_from("<H",marker[0],26)[0]
    if image[data+(marker_cluster-2)*512:data+(marker_cluster-2)*512+16]!=b"QUARTZIE USER V1":
        raise ValueError("Data volume marker damaged")
    users=[x for x in root_items if x[:11]==b"USERS      " and x[11]&0x10]
    if len(users)!=1:
        raise ValueError("User folder not created")
    def directory(item):
        cluster=struct.unpack_from("<H",item,26)[0]
        if cluster<2:
            raise ValueError("Invalid directory cluster")
        return entries(image,data+(cluster-2)*512,16)
    profiles=directory(users[0])
    defaults=[x for x in profiles if x[:11]==b"DEFAULT    " and x[11]&0x10]
    if len(defaults)!=1:
        raise ValueError("Default profile was not created")
    profile=directory(defaults[0])
    folders=[x for x in profile if x[11]&0x10 and x[:11] not in (b".          ",b"..         ")]
    if len(folders)<6:
        raise ValueError("Incomplete user profile")
    print("PASS: persistent user disk contains a complete Default profile")

if __name__=="__main__":
    check(sys.argv[1])
