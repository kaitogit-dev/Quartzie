import base64
import pathlib
import struct
import sys
from io import BytesIO
from PIL import Image

def dib(image):
    size=image.width
    rgba=image.convert("RGBA")
    xor=bytearray()
    mask=bytearray()
    for y in range(size-1,-1,-1):
        for x in range(size):
            r,g,b,a=rgba.getpixel((x,y))
            xor.extend((b,g,r,a))
    row_bytes=((size+31)//32)*4
    for y in range(size-1,-1,-1):
        row=bytearray(row_bytes)
        for x in range(size):
            if rgba.getpixel((x,y))[3]==0:
                row[x//8]|=128>>(x%8)
        mask.extend(row)
    header=struct.pack("<IiiHHIIiiII",40,size,size*2,1,32,0,len(xor),0,0,0,0)
    return header+bytes(xor)+bytes(mask)

def build(source,c_source,cur_file):
    image=Image.open(BytesIO(base64.b64decode(pathlib.Path(source).read_text().strip()))).convert("RGBA")
    cursor=image.resize((32,32),Image.Resampling.LANCZOS)
    pixels=list(cursor.getdata())
    lines=['#include "cursor.h"','const uint32_t q_cursor_pixels[Q_CURSOR_W*Q_CURSOR_H]={']
    for row in range(0,len(pixels),12):
        colors=pixels[row:row+12]
        lines.append(','.join('0x%08x'%((a<<24)|(r<<16)|(g<<8)|b) for r,g,b,a in colors)+',')
    lines.append('};')
    pathlib.Path(c_source).write_text('\n'.join(lines)+'\n',encoding="utf-8")
    entries=[]
    for size in (24,32,48,64):
        scaled=image.resize((size,size),Image.Resampling.LANCZOS)
        entries.append((size,dib(scaled)))
    parts=[struct.pack('<HHH',0,2,len(entries))]
    offset=6+16*len(entries)
    for size,content in entries:
        parts.append(struct.pack('<BBBBHHII',size,size,0,0,0,0,len(content),offset))
        offset+=len(content)
    pathlib.Path(cur_file).write_bytes(b''.join(parts+ [data for _,data in entries]))

if __name__=='__main__':
    build(sys.argv[1],sys.argv[2],sys.argv[3])
