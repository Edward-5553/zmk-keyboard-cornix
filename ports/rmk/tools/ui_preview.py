"""Convert CI-rendered PPM previews to PNG, using only the Python standard library."""
import base64
import struct
import zlib
from pathlib import Path


def chunk(name, data):
    return struct.pack('>I',len(data))+name+data+struct.pack('>I',zlib.crc32(name+data))


for path in Path('ui-preview').glob('*.ppm'):
    magic, size, maximum, pixels=path.read_bytes().split(b'\n',3)
    assert magic==b'P6' and maximum==b'255'
    w,h=map(int,size.split());assert len(pixels)==w*h*3
    data=b''.join(b'\x00'+pixels[y*w*3:(y+1)*w*3] for y in range(h))
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(data,9))+chunk(b'IEND',b'')
    path.with_suffix('.png').write_bytes(png)
    # A compact render is also readable through the job-log API for visual review.
    if path.stem=='typing':
        print('UI_PREVIEW_BASE64='+base64.b64encode(png).decode())
