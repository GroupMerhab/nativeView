import zlib
import struct
import os

def chunk(tag, data):
    return struct.pack("!I", len(data)) + tag + data + struct.pack("!I", zlib.crc32(tag + data) & 0xffffffff)

def write_png(width, height, raw_rgba, filename):
    # PNG Signature
    png = b"\x89PNG\r\n\x1a\n"
    
    # IHDR: width, height, bit_depth=8, color_type=6 (RGBA), compression=0, filter=0, interlace=0
    ihdr_data = struct.pack("!IIBBBBB", width, height, 8, 6, 0, 0, 0)
    png += chunk(b"IHDR", ihdr_data)

    # IDAT: Scanlines
    # Each scanline starts with a filter byte (0 = None)
    scanlines = b""
    stride = width * 4
    for i in range(height):
        scanlines += b"\x00"  # Filter byte
        scanlines += raw_rgba[i*stride : (i+1)*stride]
    
    compressed = zlib.compress(scanlines)
    png += chunk(b"IDAT", compressed)

    # IEND
    png += chunk(b"IEND", b"")

    with open(filename, "wb") as f:
        f.write(png)
    print(f"Generated {filename}")

def generate_icon(size, filename):
    data = bytearray()
    for y in range(size):
        for x in range(size):
            # Simple pattern: Blue background with white square in middle
            if size//4 <= x < 3*size//4 and size//4 <= y < 3*size//4:
                data.extend([255, 255, 255, 255]) # White
            else:
                data.extend([65, 105, 225, 255]) # Royal Blue
    
    write_png(size, size, bytes(data), filename)

if __name__ == "__main__":
    assets_dir = os.path.dirname(os.path.abspath(__file__))
    if not os.path.exists(assets_dir):
        os.makedirs(assets_dir)
        
    generate_icon(16, os.path.join(assets_dir, "icon_16.png"))
    generate_icon(32, os.path.join(assets_dir, "icon_32.png"))
