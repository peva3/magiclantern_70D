#!/usr/bin/env python3
"""
Eventproc table discovery - v2
Uses known eventproc names to find table structure.
"""
import struct

ROM1 = open("/app/70d/roms/70D/ROM1.BIN", "rb").read()
ROM0 = open("/app/70d/roms/70D/ROM0.BIN", "rb").read()

# Mapping schemes observed in firmware:
# Scheme A: runtime = 0xFF000000 + file_offset (used by call(), shamem_read, EngDrvOut)
# Scheme B: runtime = 0xFF0C0000 + file_offset (used by FA_SetProperty, etc.)
# We need to check both when looking for function pointer tables

def is_valid_runtime_addr(val):
    """Check if a value looks like a runtime function pointer"""
    return 0xFF000000 <= val <= 0xFFFFFFFF

def get_string_at(data, offset, max_len=128):
    end = data.find(b'\x00', offset, min(offset+max_len, len(data)))
    if end > offset:
        s = data[offset:end]
        try:
            return s.decode('ascii', errors='replace')
        except:
            return None
    return None

def check_scheme(runtime_addr):
    """Map runtime addr to file offset using both schemes"""
    for base, name in [(0xFF000000, "A"), (0xFF0C0000, "B")]:
        offset = runtime_addr - base
        if 0 <= offset < len(ROM1):
            # Check if there's valid code at this offset
            word = struct.unpack_from('<I', ROM1, offset)[0]
            # ARM instructions typically start with 0xE (cond field)
            # THUMB: 0x0-0xFFFF
            if offset + 4 <= len(ROM1):
                return offset, name
    return None, None

print("=" * 60)
print("EVENTPROC TABLE DISCOVERY v2")
print("=" * 60)

# Step 1: Load known eventproc names from existing map
known = {}
with open("/app/70d/tools/ghidra/eventproc_map.csv") as f:
    for line in f.readlines()[1:]:
        parts = line.strip().split(",")
        if len(parts) >= 3:
            name = parts[2]
            rt_addr = int(parts[1], 16)
            known[name] = rt_addr

print(f"Known eventproc entries: {len(known)}")

# Step 2: Find each name as a string in ROM1
print("\nSearching for eventproc names in ROM1...")
name_locations = {}  # name -> file_offset_of_string
for name in known:
    needle = name.encode() + b'\x00'
    pos = ROM1.find(needle)
    if pos >= 0:
        name_locations[name] = pos

print(f"Strings found in ROM1: {len(name_locations)}")

# Step 3: For each found name, search backwards for func ptr table entries
# The table entry is {func_ptr (1-3 runtime addresses), 0 or string_ptr}
print("\nAnalyzing table structure around each name...")
table_start_candidates = set()

for name, str_off in sorted(name_locations.items()):
    rt_addr = known[name]
    
    # Search backwards for function pointers that match
    # Look for structure: [func_ptr, func_ptr, ..., name_string_bytes]
    for search_back in range(4, 256, 4):  # check 4-256 bytes back
        candidate_off = str_off - search_back
        if candidate_off < 0:
            break
        
        val = struct.unpack_from('<I', ROM1, candidate_off)[0]
        if is_valid_runtime_addr(val):
            # Found a potential function pointer
            # Check if it points to valid code
            _, scheme = check_scheme(val)
            if scheme:
                # Now check: is this part of an eventproc table?
                # Eventproc table format: {str_ptr_4B, func_ptr_4B}[N], {0,0}
                # OR: {func_ptr_4B}[N], {string_name, ...}
                
                # Check for known pattern: entry for FA_SetProperty at 0xF81D5DD8
                # has consecutive func ptrs then string name
                
                # Try format: consecutive func ptrs (no string ptr interleaved)
                # followed by string
                
                # Check if there are multiple consecutive valid func ptrs
                num_fptrs = 0
                for j in range(16):
                    v = struct.unpack_from('<I', ROM1, candidate_off + j*4)[0]
                    if is_valid_runtime_addr(v) and check_scheme(v)[0] is not None:
                        num_fptrs += 1
                    else:
                        break
                
                if num_fptrs >= 2:
                    table_start_candidates.add(candidate_off)

print(f"\nTable start candidates: {len(table_start_candidates)}")

# Group nearby candidates
if table_start_candidates:
    sorted_candidates = sorted(table_start_candidates)
    tables = []
    cur_table = [sorted_candidates[0]]
    for i in range(1, len(sorted_candidates)):
        if sorted_candidates[i] - sorted_candidates[i-1] < 16:
            cur_table.append(sorted_candidates[i])
        else:
            tables.append(cur_table)
            cur_table = [sorted_candidates[i]]
    tables.append(cur_table)
    
    print(f"\nUnique tables: {len(tables)}")
    for ti, table in enumerate(tables):
        start = table[0]
        print(f"\nTable {ti+1} at file offset 0x{start:X} (ROM1 0xF8{start:07X}):")
        
        # Read entries
        off = start
        entries = []
        while off < len(ROM1) - 8:
            # Check if this is a string (name or name-like)
            # Look for null-terminated ASCII
            first = ROM1[off]
            if first == 0:
                break  # end of table
            
            val = struct.unpack_from('<I', ROM1, off)[0]
            
            if is_valid_runtime_addr(val):
                file_off, scheme = check_scheme(val)
                if file_off is not None:
                    # Read function name from nearby
                    entries.append((off, val, file_off, scheme, None))
                    off += 4
                    continue
            
            # Check if it's a string (ASCII printable start)
            if 0x20 <= first < 0x7F:
                s = get_string_at(ROM1, off, 48)
                if s and len(s) >= 2 and s[0].isalpha():
                    # This is a string - table may interleave func ptrs and names
                    entries.append((off, 0, 0, None, s))
                    off += len(s) + 1  # skip string + null
                    continue
            
            # Check for 0x00 (end of table)
            if val == 0:
                if len(entries) > 0 and entries[-1][1] == 0:
                    break  # double null = end
                entries.append((off, 0, 0, None, None))
                off += 4
                continue
            
            entries.append((off, val, None, None, None))
            off += 4
        
        for e_off, val, f_off, scheme, s in entries[:20]:
            if s:
                print(f"  0x{e_off:X} (STR): \"{s}\"")
            elif val == 0:
                print(f"  0x{e_off:X}: NULL")
            elif f_off is not None:
                rt_str = f"0x{val:08X}" 
                print(f"  0x{e_off:X} (FUNC): {rt_str} (file 0x{f_off:X}, scheme {scheme})")
            else:
                print(f"  0x{e_off:X} (DATA): 0x{val:08X}")
        if len(entries) > 20:
            print(f"  ... ({len(entries)-20} more)")

# Step 4: Also scan for the known 8-byte entry format
# {string_ptr, func_ptr} pairs
print("\n" + "=" * 60)
print("ALTERNATIVE: 8-byte entry format {string_ptr, func_ptr}")
print("=" * 60)

alt_entries = []
for i in range(0, len(ROM1) - 16, 4):
    str_ptr = struct.unpack_from('<I', ROM1, i)[0]
    func_ptr = struct.unpack_from('<I', ROM1, i+4)[0]
    
    if str_ptr == 0:
        continue
    
    # Check if string pointer points to ROM0 or ROM1
    str_in_rom0 = 0xF0000000 <= str_ptr < 0xF0800000
    str_in_rom1 = 0xF8000000 <= str_ptr < 0xF9000000
    
    if not (str_in_rom0 or str_in_rom1):
        continue
    
    # Get the string
    if str_in_rom0:
        s = get_string_at(ROM0, str_ptr - 0xF0000000, 64)
    else:
        s = get_string_at(ROM1, str_ptr - 0xF8000000, 64)
    
    if not s or len(s) < 3 or not s[0].isalpha():
        continue
    
    # Check if it looks like an eventproc name (camelCase or FA_ prefixed)
    if not ('_' in s or any(c.isupper() for c in s[1:5])):
        continue
    
    # Check function pointer
    if not is_valid_runtime_addr(func_ptr):
        continue
    
    # Verify next entry
    if i + 12 < len(ROM1):
        next_s = struct.unpack_from('<I', ROM1, i+8)[0]
        next_f = struct.unpack_from('<I', ROM1, i+12)[0]
        has_next = (next_s == 0 and next_f == 0) or \
                   (is_valid_runtime_addr(next_f) and 
                    (0xF0000000 <= next_s < 0xF0800000 or 0xF8000000 <= next_s < 0xF9000000 or next_s == 0))
        if not has_next:
            continue
    
    alt_entries.append((i, str_ptr, func_ptr, s))

print(f"8-byte format entries found: {len(alt_entries)}")
for off, sp, fp, s in alt_entries[:30]:
    rom_addr = 0xF8000000 + off
    print(f"  ROM1 0x{rom_addr:08X}: str=0x{sp:08X} func=0x{fp:08X} \"{s[:50]}\"")
if len(alt_entries) > 30:
    print(f"  ... ({len(alt_entries)-30} more)")

print(f"\nAlternative format total: {len(alt_entries)} unique entries")

# Step 5: Save full results
with open("/app/70d/tools/ghidra/eventproc_full_map_v2.csv", "w") as f:
    f.write("rom_addr,str_ptr,func_ptr,name\n")
    for off, sp, fp, s in alt_entries:
        f.write(f"0xF8{off:07X},0x{sp:08X},0x{fp:08X},{s}\n")
print("Saved to eventproc_full_map_v2.csv")
