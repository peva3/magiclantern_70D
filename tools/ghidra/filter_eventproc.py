#!/usr/bin/env python3
"""
Process eventproc v3 results to identify real dispatch tables.
Cross-reference with known entries and separate eventproc vs error-message tables.
"""
import struct

rom1 = open("/app/70d/roms/70D/ROM1.BIN", "rb").read()

# Load v3 results
entries = []
with open("/app/70d/tools/ghidra/eventproc_full_map_v3.csv") as f:
    for line in f.readlines()[1:]:
        parts = line.strip().split(",", 3)
        if len(parts) == 4:
            off = int(parts[0], 16)
            sp = int(parts[1], 16)
            fp = int(parts[2], 16)
            name = parts[3]
            entries.append((off, sp, fp, name))

# Load known eventproc names
known = set()
with open("/app/70d/tools/ghidra/eventproc_map.csv") as f:
    for line in f.readlines()[1:]:
        name = line.strip().split(",")[2]
        known.add(name)

print("=" * 60)
print("EVENTPROC TABLE FILTERING")
print("=" * 60)
print(f"\nTotal v3 entries: {len(entries)}")
print(f"Known eventproc names: {len(known)}")

# Group into tables
entries.sort(key=lambda x: x[0])
tables = []
cur_table = [entries[0]] if entries else []
for i in range(1, len(entries)):
    diff = entries[i][0] - entries[i-1][0]
    if diff <= 8:
        cur_table.append(entries[i])
    else:
        tables.append(cur_table)
        cur_table = [entries[i]]
if cur_table:
    tables.append(cur_table)

print(f"Tables: {len(tables)}")

# Identify which tables are eventproc dispatch tables
# Criteria: names look like function names (not error messages)
eventproc_tables = []
other_tables = []
for table in tables:
    first_names = [e[3] for e in table[:3]]
    all_names = [e[3] for e in table]
    
    # Count how many look like callable function names
    func_like = sum(1 for n in all_names if (
        n.startswith("FA_") or n.startswith("fw_") or n.startswith("gui_") or
        n.startswith("lv_") or n.startswith("TCH_") or n.startswith("PROP_") or
        n.startswith("SRM_") or n.startswith("FIO_") or n.startswith("FC_") or
        (n[0].isupper() and not n.endswith("failed") and not n.endswith("not found"))
    ))
    
    error_like = sum(1 for n in all_names if (
        "failed" in n or "not found" in n or "ERROR" in n or "error" in n or
        "cannot" in n.lower() or "NULL" in n or "Missing" in n
    ))
    
    # Check for known eventproc names
    known_match = sum(1 for n in all_names if n in known)
    
    if known_match > 0 or (func_like > error_like and func_like >= 2):
        eventproc_tables.append(table)
    else:
        other_tables.append(table)

print(f"\nEventproc dispatch tables: {len(eventproc_tables)}")
print(f"Other tables (error msgs, etc): {len(other_tables)}")

# Combine all eventproc entries
eventproc_entries = []
for table in eventproc_tables:
    eventproc_entries.extend(table)

# Extract unique eventproc names
eventproc_names = set(e[3] for e in eventproc_entries)
known_found = eventproc_names & known
known_missing = known - eventproc_names

print(f"\nEventproc dispatch entries: {len(eventproc_entries)}")
print(f"Unique eventproc names: {len(eventproc_names)}")
print(f"Known names matched: {len(known_found)}/{len(known)}")
print(f"Known names NOT found: {len(known_missing)}")

if known_missing:
    print("\nMissing known names that need manual cross-reference:")
    for name in sorted(known_missing)[:20]:
        print(f"  {name}")

# Save filtered eventproc map
with open("/app/70d/tools/ghidra/eventproc_dispatch_map.csv", "w") as f:
    f.write("file_offset,str_ptr_runtime,func_ptr_runtime,name\n")
    for off, sp, fp, name in sorted(eventproc_entries, key=lambda x: x[0]):
        f.write(f"0x{off:08X},0x{sp:08X},0x{fp:08X},{name}\n")

with open("/app/70d/tools/ghidra/eventproc_other_map.csv", "w") as f:
    f.write("file_offset,str_ptr_runtime,func_ptr_runtime,name\n")
    for table in other_tables:
        for off, sp, fp, name in table:
            f.write(f"0x{off:08X},0x{sp:08X},0x{fp:08X},{name}\n")

print(f"\nSaved {len(eventproc_entries)} dispatch entries")
print(f"Saved {sum(len(t) for t in other_tables)} other entries")

# Print eventproc tables
print("\n" + "=" * 60)
print("EVENTPROC DISPATCH TABLES")
print("=" * 60)
for table in eventproc_tables:
    start = table[0][0]
    rom_addr = 0xF8000000 + start
    known_count = sum(1 for e in table if e[3] in known)
    print(f"\nTable at ROM 0x{rom_addr:08X} ({len(table)} entries, {known_count} known):")
    for off, sp, fp, name in table[:8]:
        tag = " ***" if name in known else ""
        print(f"  0x{sp:08X} -> 0x{fp:08X} : {name}{tag}")
    if len(table) > 8:
        print(f"  ... ({len(table)-8} more)")

# Count unique function pointers
func_ptrs = set(e[2] for e in eventproc_entries)
print(f"\nUnique function pointers: {len(func_ptrs)}")

# Cross-check: how many func ptrs point to valid ARM code?
valid_code = 0
for fp in func_ptrs:
    foff = fp - 0xFF000000
    if 0 <= foff < len(rom1) - 4:
        word = struct.unpack_from('<I', rom1, foff)[0]
        if (word & 0xF0000000) == 0xE0000000:
            valid_code += 1
print(f"Function pointers pointing to ARM code: {valid_code}/{len(func_ptrs)}")
