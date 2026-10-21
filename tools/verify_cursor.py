import base64
import pathlib
import struct
import sys
from io import BytesIO
from PIL import Image

def check(cursor_path,artwork_path):
    data=pathlib.Path(cursor_path).read_bytes()
    image=Image.open(BytesIO(base64.b64decode(pathlib.Path(artwork_path).read_text().strip()))).convert("RGBA")
    if image.size!=(64,64):
        raise ValueError("Unexpected artwork dimensions")
    header=struct.unpack_from("<HHH",data,0)
    if header!=(0,2,4):
        raise ValueError("Invalid cursor header")
    for index,size in enumerate((24,32,48,64)):
        width,height,colors,reserved,hot_x,hot_y,length,offset=struct.unpack_from("<BBBBHHII",data,6+index*16)
        if (width,height,colors,reserved,hot_x,hot_y)!=(size,size,0,0,0,0):
            raise ValueError("Cursor size or hotspot mismatch")
        mask_bytes=((size+31)//32)*4*size
        expected=40+size*size*4+mask_bytes
        if length!=expected or offset+length>len(data):
            raise ValueError("Cursor bitmap is truncated")
        bw,bh=struct.unpack_from("<ii",data,offset+4)
        if (bw,bh)!=(size,size*2):
            raise ValueError("CUR bitmap headers are inconsistent")
    rendered=Image.open(BytesIO(data)).convert("RGBA")
    if rendered.size!=(64,64):
        raise ValueError("CUR preview size mismatch")
    alpha_offset=struct.unpack_from("<I",data,6+3*16+12)[0]+40
    first=data[alpha_offset+(63*64*4)+3]
    clear=data[alpha_offset+(63*64*4)+(63*4)+3]
    if first==0 or clear!=0:
        raise ValueError("CUR transparency or hotspot mismatch")
    print("PASS: multi-size transparent CUR, hotspot (0,0), 24/32/48/64 pixels")

if __name__=="__main__":
    check(sys.argv[1],sys.argv[2])
