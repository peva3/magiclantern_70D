#!/usr/bin/env python3
"""
Search for eventproc name strings in both ROM files,
then locate the eventproc table entries that reference them.
Table structure: 8-byte pairs {string_ptr, func_ptr}, NULL terminated.
"""
import struct
import sys

def bytes_to_u32(data, off):
    return struct.unpack('<I', data[off:off+4])[0]

def main():
    # Load ROMs
    with open('roms/70D/ROM1.BIN', 'rb') as f:
        rom1 = f.read()
    with open('roms/70D/ROM0.BIN', 'rb') as f:
        rom0 = f.read()

    # Load eventproc names
    with open('/tmp/eventproc_names.txt', 'r') as f:
        names = [line.strip() for line in f if line.strip()]

    print(f"Loaded {len(names)} eventproc names")
    print(f"ROM0 size: {len(rom0)} bytes")
    print(f"ROM1 size: {len(rom1)} bytes")
    print()

    # For each name, search both ROMs for the string
    rom0_base = 0xF0000000
    rom1_base = 0xF8000000

    found_in_rom0 = {}
    found_in_rom1 = {}

    for name in names:
        bname = name.encode('ascii') + b'\x00'
        # Search ROM0
        pos = 0
        while True:
            p = rom0.find(bname, pos)
            if p < 0:
                break
            found_in_rom0.setdefault(name, []).append(rom0_base + p)
            pos = p + 1
        # Search ROM1
        pos = 0
        while True:
            p = rom1.find(bname, pos)
            if p < 0:
                break
            found_in_rom1.setdefault(name, []).append(rom1_base + p)
            pos = p + 1

    print(f"Found {len(found_in_rom0)} names in ROM0, {len(found_in_rom1)} names in ROM1\n")

    # Now for each found string, check if it's part of an eventproc table
    # Table entry: {str_ptr, func_ptr} — 8 bytes
    # str_ptr should point to the string we found
    # func_ptr should be a runtime address (0xFFxxxxxx)
    print("=== Eventproc table entries found in ROM1 ===")
    print()

    table_entries = {}
    for name in sorted(found_in_rom1.keys()):
        for str_addr in found_in_rom1[name]:
            # String found at ROM address (0xF8xxxxxx).
            # Table entries store RUNTIME addresses (0xFFxxxxxx).
            # Mapping: runtime_addr = (ROM_addr - 0xF8000000) + 0xFF000000
            str_runtime = str_addr - rom1_base + 0xFF000000
            str_bytes = struct.pack('<I', str_runtime)
            pos = 0
            while True:
                p = rom1.find(str_bytes, pos)
                if p < 0 or p > len(rom1) - 8:
                    break
                # Check if word at p+4 is a function pointer (runtime addr 0xFFxxxxxx)
                func_word = bytes_to_u32(rom1, p + 4)
                if func_word >= 0xFF000000 and func_word <= 0xFFFFFFFF:
                    # Calculate where this func would be in ROM (check existence)
                    func_rom_off = func_word - 0xFF000000
                    if 0 <= func_rom_off < len(rom1):
                        key = (p, str_runtime, func_word)
                        if key not in table_entries:
                            table_entries[key] = {
                                'name': name,
                                'str_runtime': str_runtime,
                                'func_runtime': func_word,
                                'rom_offset': p,
                            }
                pos = p + 1

    # Print unique table entries
    seen_funcs = set()
    print(f"\nTotal table entries found: {len(table_entries)}")
    
    for key, entry in sorted(table_entries.items(), key=lambda x: x[0][0]):
        p, str_runtime, func_word = key
        name = entry['name']
        if func_word in seen_funcs:
            continue
        seen_funcs.add(func_word)
        func_rom_off = func_word - 0xFF000000
        str_rom = str_runtime - 0xFF000000 + rom1_base
        print(f"  call(\"{name}\") -> 0x{func_word:08X}  [ROM str at 0x{str_rom:X}, table at ROM+0x{p:X}]")

    print(f"\nUnique function targets: {len(seen_funcs)}")
    
    # Save as CSV
    with open('tools/ghidra/eventproc_map.csv', 'w') as f:
        f.write("call_name,func_runtime_addr,rom_offset\n")
        seen_funcs.clear()
        for key, entry in sorted(table_entries.items(), key=lambda x: x[0][0]):
            p, str_runtime, func_word = key
            name = entry['name']
            if func_word in seen_funcs:
                continue
            seen_funcs.add(func_word)
            f.write(f'{name},0x{func_word:08X},0x{rom1_base+p:08X}\n')
    print("\nSaved to tools/ghidra/eventproc_map.csv")

if __name__ == '__main__':
    main()
