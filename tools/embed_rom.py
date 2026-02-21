#!/usr/bin/env python3
"""
embed_rom.py - Converts a .gb ROM file into a C source file
Usage: python3 embed_rom.py your_game.gb > src/rom_data.c
"""
import sys
import os

def embed_rom(filename):
    with open(filename, 'rb') as f:
        data = f.read()
    
    # Try to read the title from the ROM header
    title = "UNKNOWN"
    if len(data) > 0x143:
        raw = data[0x134:0x144]
        title = ''.join(chr(b) if 32 <= b < 127 else '' for b in raw).strip()
    
    print(f"/* Auto-generated from {os.path.basename(filename)} */")
    print(f"/* ROM Title: {title} */")
    print(f"/* ROM Size:  {len(data)} bytes */")
    print()
    print('#include <tamtypes.h>')
    print()
    print(f'u8 rom_data[] = {{')
    
    for i, byte in enumerate(data):
        if i % 16 == 0:
            print(f'    /* 0x{i:04X} */ ', end='')
        print(f'0x{byte:02X}', end='')
        if i < len(data) - 1:
            print(', ', end='')
        if (i + 1) % 16 == 0:
            print()
    
    print()
    print('};')
    print()
    print(f'u32 rom_size = {len(data)};')

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <rom_file.gb>", file=sys.stderr)
        sys.exit(1)
    embed_rom(sys.argv[1])
