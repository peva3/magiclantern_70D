#!/usr/bin/env python3
"""
Eventproc table discovery v3 - based on the actual table walker disassembly.
Table format confirmed: {string_ptr (4B), func_ptr (4B)}[N], {0, anything}
"""
import struct

rom1 = open("/app/70d/roms/70D/ROM1.BIN", "rb").read()
rom0 = open("/app/70d/roms/70D/ROM0.BIN", "rb").read()

def read_u32(off):
    return struct.unpack_from('<I', rom1, off)[0]

def read_u32_rom0(off):
    return struct.unpack_from('<I', rom0, off)[0]

def get_string_at(data, off, max_len=128):
    end = data.find(b'\x00', off, min(off+max_len, len(data)))
    if end > off:
        return data[off:end].decode('ascii', errors='replace')
    return None

def is_valid_str_ptr(val):
    """Check if value points to valid ROM string"""
    in_rom0 = 0xF0000000 <= val < 0xF0800000
    in_rom1 = 0xF8000000 <= val < 0xF9000000
    in_rt = 0xFF000000 <= val <= 0xFFFFFFFF
    
    off = None
    data = None
    if in_rom0:
        off = val - 0xF0000000
        data = rom0
    elif in_rom1:  
        off = val - 0xF8000000
        data = rom1
    elif in_rt:
        off = val - 0xFF000000
        data = rom1
    
    if off is None or data is None:
        return False
    
    s = get_string_at(data, off, 64)
    if s and len(s) >= 2 and s[0].isalpha():
        return True
    return False

def get_str_from_ptr(val):
    """Get string from pointer value"""
    in_rom0 = 0xF0000000 <= val < 0xF0800000
    in_rom1 = 0xF8000000 <= val < 0xF9000000
    in_rt = 0xFF000000 <= val <= 0xFFFFFFFF
    
    if in_rom0:
        return get_string_at(rom0, val - 0xF0000000, 80)
    elif in_rom1:
        return get_string_at(rom1, val - 0xF8000000, 80)
    elif in_rt:
        return get_string_at(rom1, val - 0xFF000000, 80)
    return None

def is_valid_func_ptr(val):
    """Check if value looks like a runtime function pointer"""
    if 0xFF000000 <= val <= 0xFFFFFFFF:
        off = val - 0xFF000000
        if 0 <= off < len(rom1) - 4:
            word = read_u32(off)
            if (word & 0xF0000000) == 0xE0000000:  # ARM instruction
                return True
            # Also check for THUMB
            if (word & 0xFFFF) in range(0x4000, 0xE000):
                return True
    elif 0x00010000 <= val < 0x00100000:  # RAM function
        return True
    return False

print("=" * 60)
print("EVENTPROC TABLE DISCOVERY v3")
print("=" * 60)

# Step 1: Find the data structure referenced by the dispatch helper
# At 0x14434C: ldr r4, [pc, #0x194] -> load from PC+8+0x194 = 0x1444E4
# At 0x1444E4 we should find a pointer to the eventproc data structure

print("\nLiteral pool references from dispatch helper:")
for off in [0x1444E4, 0x1444EC, 0x1444F0]:
    val = read_u32(off)
    if 0xFF000000 <= val <= 0xFFFFFFFF:
        foff = val - 0xFF000000
        word = read_u32(foff)
        print(f"  Literal 0x{off:06X}: 0x{val:08X} -> file 0x{foff:X}, word=0x{word:08X}")
    elif 0xF8000000 <= val < 0xF9000000:
        foff = val - 0xF8000000
        print(f"  Literal 0x{off:06X}: 0x{val:08X} -> file 0x{foff:X}")

# Step 2: Look at the data structure at 0x14421C
data_off = 0x14421C
print(f"\nData structure at file 0x{data_off:X}:")
for i in range(16):
    off = data_off + i*4
    val = read_u32(off)
    s = get_str_from_ptr(val)
    label = f'"{s[:40]}"' if s else ""
    
    if is_valid_func_ptr(val):
        foff = val - 0xFF000000
        word = read_u32(foff)
        label = f"(ARM 0x{word:08X})"
    
    print(f"  +0x{i*4:02X} (0x{off:06X}): 0x{val:08X} {label}")

# Step 3: The dispatch helper loads [r4, #4] as the first table
# r4 = [0x1444E4] - 0x1c = data_struct_base
# The eventproc table pointer should be at [r4+4] or similar

# Let me look at what r4 actually points to
# From code: ldr r0, [pc, #0x144] at 0x1443A4 -> load from 0x1443A8+0x144 = 0x1444EC
# sub r4, r0, #0x1c  -> r4 = [0x1444EC] - 0x1c

r0_val = read_u32(0x1444EC)  # The value loaded by [pc, #0x144]
if r0_val >= 0xFF000000:
    r0_base = r0_val - 0xFF000000
    r4 = r0_base - 0x1c
    print(f"\nDispatch data structure (from [0x1444EC]-0x1c):")
    print(f"  r0_val = 0x{r0_val:08X}, r0_base = 0x{r0_base:X}, r4 = 0x{r4:X}")
    
    # Now structure is at r4:
    # [r4+4] = first table? Or look at code more carefully
    # At 0x1443B4: ldr r0, [r4, #4] - this is loaded with key
    # At 0x1443C4: ldr r2, [r4, #8]! - func ptr with writeback
    # At 0x1443CC: ldr r0, [r4, #0xc] - more data
    
    for i in range(12):
        off = r4 + i*4
        val = read_u32(off)
        s = get_str_from_ptr(val) or ""
        if is_valid_func_ptr(val):
            foff = val - 0xFF000000
            if 0 <= foff < len(rom1):
                word = read_u32(foff)
                s = f"(ARM 0x{word:08X})"
            else:
                s = f"(RAM addr)"
        print(f"  [r4{'+0x%02X' % (i*4)}] 0x{val:08X} {s[:50] if s else ''}")

# Step 4: Now scan the ENTIRE ROM1 for {str_ptr, func_ptr} tables
# The format from the walker: each entry is 8 bytes
# Termination: str_ptr == 0 (the walker checks [r4] == 0)

print("\n" + "=" * 60)
print("FULL ROM1 SCAN FOR EVENTPROC TABLES")
print("=" * 60)

# Strategy: find all tables that satisfy:
# 1. str_ptr points to a valid ASCII string (in ROM0, ROM1, or runtime space)
# 2. func_ptr is a valid runtime or RAM function pointer
# 3. Multiple consecutive valid entries
# 4. Terminated by a 0 str_ptr

tables = []
current_table = []
table_start = None

for off in range(0, len(rom1) - 8, 4):
    str_ptr = read_u32(off)
    func_ptr = read_u32(off + 4)
    
    if str_ptr == 0:
        if table_start is not None and len(current_table) >= 2:
            tables.append((table_start, current_table[:]))
        table_start = None
        current_table = []
        continue
    
    if not (is_valid_str_ptr(str_ptr) and is_valid_func_ptr(func_ptr)):
        if table_start is not None and len(current_table) >= 2:
            tables.append((table_start, current_table[:]))
        table_start = None
        current_table = []
        continue
    
    if table_start is None:
        table_start = off
    
    s = get_str_from_ptr(str_ptr)
    if s and len(s) >= 2:
        current_table.append((off, str_ptr, func_ptr, s))

# Also check if last table was valid
if table_start is not None and len(current_table) >= 2:
    tables.append((table_start, current_table[:]))

print(f"Found {len(tables)} potential eventproc tables")

# Filter: only keep tables that have real eventproc names
valid_tables = []
for start, entries in tables:
    # Check if names look like eventproc functions
    has_eventproc_name = False
    for _, _, _, s in entries[:5]:
        if '_' in s and s[0].isalpha() and len(s) >= 4:
            has_eventproc_name = True
            break
        if s.startswith("FA_") or s.startswith("fw_") or s.startswith("gui_"):
            has_eventproc_name = True
            break
    if has_eventproc_name:
        valid_tables.append((start, entries))

print(f"Tables with eventproc-looking names: {len(valid_tables)}")

for start, entries in valid_tables:
    rom_addr = 0xF8000000 + start
    print(f"\nTable at 0x{rom_addr:08X} ({len(entries)} entries):")
    for off, sp, fp, s in entries[:10]:
        print(f"  0x{off:06X}: str=0x{sp:08X} func=0x{fp:08X} \"{s[:50]}\"")
    if len(entries) > 10:
        print(f"  ... ({len(entries)-10} more)")
    
    # Also check for the null terminator
    next_off = entries[-1][0] + 8
    next_str = read_u32(next_off)
    term_str = get_str_from_ptr(next_str) if next_str else ""
    print(f"  Terminator at 0x{next_off:06X}: str_ptr=0x{next_str:08X} ({term_str[:20] if term_str else 'NULL'})")

# Save full results
all_entries = []
for start, entries in valid_tables:
    for off, sp, fp, s in entries:
        all_entries.append((off, sp, fp, s))

with open("/app/70d/tools/ghidra/eventproc_full_map_v3.csv", "w") as f:
    f.write("file_offset,str_ptr,func_ptr,name\n")
    for off, sp, fp, s in all_entries:
        f.write(f"0x{off:08X},0x{sp:08X},0x{fp:08X},{s}\n")

print(f"\nSaved {len(all_entries)} entries to eventproc_full_map_v3.csv")
print(f"Unique names: {len(set(e[3] for e in all_entries))}")
