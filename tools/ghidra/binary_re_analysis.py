#!/usr/bin/env python3
"""
Comprehensive binary-level RE analysis of Canon 70D firmware ROMs.
Scans ROM0.BIN and ROM1.BIN directly without Ghidra dependency.
"""
import struct, os, sys, re

ROM0_PATH = "/app/70d/roms/70D/ROM0.BIN"
ROM1_PATH = "/app/70d/roms/70D/ROM1.BIN"

def load_rom(path, label):
    data = open(path, "rb").read()
    print(f"Loaded {label}: {len(data)} bytes")
    return data

def read_u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]

def get_string(data, offset, max_len=256):
    end = data.index(b'\x00', offset, min(offset+max_len, len(data)))
    return data[offset:end].decode('ascii', errors='replace')

def find_all(data, pattern, start=0):
    pos = start
    while True:
        pos = data.find(pattern, pos)
        if pos < 0:
            break
        yield pos
        pos += 1

def is_valid_func_ptr(val):
    """Check if a value looks like a Canon firmware function pointer"""
    if 0xFF000000 <= val <= 0xFFFFFFFF:
        return True
    return False

print("=" * 60)
print("70D FIRMWARE COMPREHENSIVE RE ANALYSIS")
print("=" * 60)

rom0 = load_rom(ROM0_PATH, "ROM0.BIN (assets)")
rom1 = load_rom(ROM1_PATH, "ROM1.BIN (code)")

# ============================================================
# PART 1: EVENTPROC TABLE DISCOVERY
# ============================================================
print("\n" + "=" * 60)
print("PART 1: EVENTPROC TABLE FINDER")
print("=" * 60)

# Strategy: In the ROM1 binary, look for 8-byte structures:
# {string_ptr (4 bytes LE), func_ptr (4 bytes LE)}
# terminated by {0, 0}
# The string pointer points into ROM0 or ROM1 text
# The function pointer is a runtime address (0xFFxxxxxx)

rom0_str_addrs = set()
for i in range(len(rom0)):
    if rom0[i] >= 0x20 and rom0[i] < 0x7F:
        # Check if we're at start of a string
        if i > 0 and rom0[i-1] == 0:
            # Start of string candidate
            end = rom0.find(b'\x00', i, i+128)
            if end > i:
                s = rom0[i:end].decode('ascii', errors='replace')
                if len(s) >= 3 and s[0].isalpha():
                    rom0_str_addrs.add((0xF0000000 + i, s))
                    i = end
                    continue
        i += 1
    else:
        i += 1

print(f"ROM0 strings: {len(rom0_str_addrs)}")

# Now scan ROM1 for eventproc tables
# Entry format: {str_ptr u32, func_ptr u32}
eventproc_entries = []
rom1_len = len(rom1)

for i in range(0, rom1_len - 8, 4):
    str_ptr = read_u32(rom1, i)
    func_ptr = read_u32(rom1, i + 4)
    
    if str_ptr == 0:
        continue
    
    # Check if str_ptr points into either ROM
    in_rom0 = 0xF0000000 <= str_ptr < 0xF0800000
    in_rom1 = 0xF8000000 <= str_ptr < 0xF9000000
    
    if not (in_rom0 or in_rom1):
        continue
    
    # Check func_ptr
    if not is_valid_func_ptr(func_ptr):
        continue
    
    # Get the ROM offset
    if in_rom0:
        str_off = str_ptr - 0xF0000000
    else:
        str_off = str_ptr - 0xF8000000  # runtime addr, not ROM
    
    # Try to read the string
    try:
        if in_rom0:
            s = get_string(rom0, str_off, 80)
        else:
            # Check if str_ptr is runtime (0xFF...) or ROM (0xF8...)
            if str_ptr >= 0xFF000000:
                str_rom_off = str_ptr - 0xFF000000
                if str_rom_off < len(rom1):
                    s = get_string(rom1, str_rom_off, 80)
                else:
                    continue
            else:
                s = get_string(rom1, str_off, 80)
    except:
        continue
    
    if s is None or len(s) < 2:
        continue
    
    # Verify next entry
    if i + 12 < rom1_len:
        nstr = read_u32(rom1, i + 8)
        nfunc = read_u32(rom1, i + 12)
        has_next = (nstr == 0 and nfunc == 0) or \
                   (is_valid_func_ptr(nfunc) and (
                       (0xF0000000 <= nstr < 0xF0800000) or 
                       (0xF8000000 <= nstr < 0xF9000000) or
                       (0xFF000000 <= nstr <= 0xFFFFFFFF)
                   ))
        if not has_next:
            continue
    
    rom_addr = 0xF8000000 + i  # ROM1 address
    eventproc_entries.append((rom_addr, func_ptr, s))

print(f"\nEventproc entries found: {len(eventproc_entries)}")

# Group by table (consecutive entries)
if eventproc_entries:
    eventproc_entries.sort(key=lambda x: x[0])
    tables = []
    cur_table = [eventproc_entries[0]]
    for i in range(1, len(eventproc_entries)):
        diff = eventproc_entries[i][0] - eventproc_entries[i-1][0]
        if diff <= 8:
            cur_table.append(eventproc_entries[i])
        else:
            tables.append(cur_table)
            cur_table = [eventproc_entries[i]]
    tables.append(cur_table)
    
    print(f"\nEventproc Tables Summary: {len(tables)} tables")
    for ti, table in enumerate(tables):
        print(f"  Table {ti+1} at 0x{table[0][0]:X} ({len(table)} entries):")
        for rom_addr, func_ptr, s in table[:8]:
            print(f"    0x{func_ptr:08X} -> {s[:60]}")
        if len(table) > 8:
            print(f"    ... ({len(table)-8} more)")
    
    # Count unique names
    unique_names = set(e[2] for e in eventproc_entries)
    print(f"\nUnique eventproc names: {len(unique_names)}")
    
    # Save full map
    csv_path = "/app/70d/tools/ghidra/eventproc_full_map.csv"
    with open(csv_path, "w") as f:
        f.write("rom_addr,func_runtime_addr,name\n")
        for rom_addr, func_ptr, s in sorted(eventproc_entries, key=lambda x: x[0]):
            f.write(f"0x{rom_addr:08X},0x{func_ptr:08X},{s}\n")
    print(f"Full eventproc map saved to {csv_path}")

# ============================================================
# PART 2: INTERRUPT VECTORS
# ============================================================
print("\n" + "=" * 60)
print("PART 2: INTERRUPT/EXCEPTION VECTORS")
print("=" * 60)

# For high vectors at 0xFFFF0000, we need the runtime image
# The ROM1 is mapped at 0xF8000000 (physical) and copied to 0xFF000000 (runtime)
# But 0xFFFF0000 is above ROM1 range
# Check if ROM1 at offset 0x7F0000 contains vector data
# Also check for vector-related code

print("Searching for vector table setup in ROM1...")

# Search for code that sets VBAR (Vector Base Address Register)
# MCR p15, 0, Rt, c12, c0, 0  = set VBAR
# Encoding: 0xEE0C0F10 | (Rt << 12)  
# Look for 0xEE0C0Fxx pattern
vbar_pattern = re.compile(b'\xEE\x0C\x0F[\x10-\x1F]')
vbar_matches = list(find_all(rom1, b'\xEE\x0C\x0F'))
print(f"VBAR setting instructions: {len(vbar_matches)}")
for off in vbar_matches[:5]:
    word = read_u32(rom1, off)
    rt = (word >> 12) & 0xF
    rom_addr = 0xF8000000 + off
    print(f"  MCR VBAR at offset 0x{off:X} (ROM 0x{rom_addr:X}), Rt=R{rt}")

# Find LDR PC,[PC,#offset] instructions that look like vector handlers
# ARM LDR PC,[PC,#imm12]: 0xE59FFxxx
# Pre-indexed: 0xE51FFxxx
print("\nSearching for vector-like LDR PC,[PC] instructions...")
ldr_pc_count = 0
for off in range(0, rom1_len - 4, 4):
    word = read_u32(rom1, off)
    if (word & 0x0F7F0000) == 0x051F0000 or (word & 0x0F7F0000) == 0x059F0000:
        ldr_pc_count += 1

print(f"LDR PC,[PC,#imm] instructions: {ldr_pc_count}")

# Try to find the vector table by looking for 8 consecutive LDR PC at 4-byte intervals
print("\nLooking for 8 consecutive vector-like entries...")
for off in range(0, rom1_len - 32, 4):
    count = 0
    for j in range(8):
        word = read_u32(rom1, off + j*4)
        if (word & 0x0F7F0000) == 0x051F0000 or (word & 0x0F7F0000) == 0x059F0000:
            count += 1
        else:
            break
    if count >= 6:
        rom_addr = 0xF8000000 + off
        vec_names = ["Reset", "Undef", "SWI", "Prefetch", "Data", "Reserved", "IRQ", "FIQ"]
        print(f"\nVector table at offset 0x{off:X} (ROM 0x{rom_addr:X}):")
        for j in range(count):
            word = read_u32(rom1, off + j*4)
            offset_field = word & 0xFFF
            if word & 0x00800000:
                offset_field = -offset_field
            pc = 0xF8000000 + off + j*4 + 8
            load_addr = pc + offset_field
            if 0 <= load_addr - 0xF8000000 < len(rom1):
                handler_ptr = read_u32(rom1, load_addr - 0xF8000000)
                print(f"  {vec_names[j]}: LDR PC -> load from 0x{load_addr:X} = 0x{handler_ptr:08X}")
        if count >= 6:
            break

# ============================================================
# PART 3: GPS STRINGS
# ============================================================
print("\n" + "=" * 60)
print("PART 3: GPS STRINGS IN ROM0")
print("=" * 60)

gps_strings = set()
for off in range(len(rom0)):
    if rom0[off] >= 0x20 and rom0[off] < 0x7F:
        # Check if start of string
        if off == 0 or rom0[off-1] == 0:
            end = rom0.find(b'\x00', off, min(off+64, len(rom0)))
            if end > off:
                s = rom0[off:end].decode('ascii', errors='replace')
                up = s.upper()
                if 'GPS' in up or 'Gps' in s:
                    gps_strings.add((0xF0000000 + off, s))

for addr, s in sorted(gps_strings)[:40]:
    print(f"  0x{addr:X}: {s}")
print(f"Total GPS strings: {len(gps_strings)}")

# ============================================================
# PART 4: AUDIO STRINGS
# ============================================================
print("\n" + "=" * 60)
print("PART 4: AUDIO-RELATED STRINGS")
print("=" * 60)

audio_strings = set()
for off in range(len(rom0)):
    if off > 0 and rom0[off-1] == 0 and 0x20 <= rom0[off] < 0x7F:
        end = rom0.find(b'\x00', off, min(off+64, len(rom0)))
        if end > off:
            s = rom0[off:end].decode('ascii', errors='replace')
            up = s.upper()
            if any(x in up for x in ['AUDIO', 'CODEC', 'ASIF', 'MIC', 'DAC ', ' ADC', 'I2S', 'SOUND']):
                audio_strings.add((0xF0000000 + off, s))

for addr, s in sorted(audio_strings)[:25]:
    print(f"  0x{addr:X}: {s}")
print(f"Total audio strings: {len(audio_strings)}")

# ============================================================
# PART 5: SWI DISPATCH (SUBS PC,LR,#4)
# ============================================================
print("\n" + "=" * 60)
print("PART 5: SWI DISPATCH (SUBS PC,LR,#4)")
print("=" * 60)

# ARM SUBS PC,LR,#4 encoding:
# ARM mode: 0xE25EF004 (SUBS PC, LR, #4)
# THUMB2: 0xF1xx 0x0F04 (SUBS PC, LR, #4) in 32-bit THUMB
# THUMB: different encoding

# Search for ARM-mode SUBS PC,LR,#4
arm_pattern = struct.pack('<I', 0xE25EF004)
swi_count = 0
for off in find_all(rom1, arm_pattern):
    rom_addr = 0xF8000000 + off
    swi_count += 1
    # Get function context
    # Look backward for function prologue
    print(f"  SUBS PC,LR,#4 at ROM 0x{rom_addr:X}")

print(f"ARM-mode SWI return instructions: {swi_count}")

# Also look for THUMB SUBS PC,LR,#4
# THUMB encoding varies, let's search more broadly
print("\nTHUMB-mode SWI candidate functions:")
# Look for functions that end with SUBS PC,LR or similar return
thumb_swi = 0
for off in range(0, rom1_len - 4, 2):
    # THUMB BX LR (0x4770) or POP {PC} (0xBDxx) near SUBS
    pass

# ============================================================
# PART 6: KNOWN FUNCTION REFERENCES
# ============================================================
print("\n" + "=" * 60)
print("PART 6: KNOWN FIRMWARE REFERENCES")
print("=" * 60)

# Check common firmware function addresses
known_functions = {
    "EngDrvOut": 0xFF0C6200,
    "prop_request_change": 0xFF0E5F20,
    "shamem_read": 0xFF0C5E60,
    "FIO_OpenFile": 0xFF1BBDE0,
    "CreateTask": 0xFF0C3D44,
    "malloc": 0xFF3F8D1C,
    "free": 0xFF3F8F38,
}

print("Checking known functions in ROM1:")
for name, rt_addr in known_functions.items():
    rom_off = rt_addr - 0xFF000000
    if rom_off < len(rom1):
        # Check first instruction
        first_inst = read_u32(rom1, rom_off)
        print(f"  {name}: 0x{rt_addr:08X} -> ROM offset 0x{rom_off:X}, first inst=0x{first_inst:08X}")
    else:
        print(f"  {name}: 0x{rt_addr:08X} -> OUT OF RANGE")

# ============================================================
# PART 7: MPU MESSAGE IDS
# ============================================================
print("\n" + "=" * 60)
print("PART 7: MPU MESSAGE ID REFERENCES")
print("=" * 60)

# Scan for MPU message IDs (0x8000xxxx range)
mpu_ids = set()
for off in range(0, rom1_len - 4, 4):
    val = read_u32(rom1, off)
    if 0x80000000 <= val < 0x80010000:
        mpu_ids.add(val)

print(f"Unique MPU message IDs referenced in code: {len(mpu_ids)}")
for msg_id in sorted(mpu_ids)[:50]:
    print(f"  0x{msg_id:08X}")
if len(mpu_ids) > 50:
    print(f"  ... ({len(mpu_ids)-50} more)")

# ============================================================
# SUMMARY
# ============================================================
print("\n" + "=" * 60)
print("FINAL SUMMARY")
print("=" * 60)
print(f"Eventproc entries: {len(eventproc_entries)}")
print(f"Unique eventproc names: {len(unique_names) if eventproc_entries else 0}")
print(f"GPS strings: {len(gps_strings)}")
print(f"Audio strings: {len(audio_strings)}")
print(f"SWI return instructions: {swi_count}")
print(f"MPU message IDs: {len(mpu_ids)}")

# Cross-reference with known stubs
print("\nStub cross-reference:")
stub_path = "/app/70d/platform/70D.112/stubs.S"
if os.path.exists(stub_path):
    stubs = {}
    with open(stub_path) as f:
        for line in f:
            line = line.strip()
            if line.startswith("NSTUB("):
                # NSTUB(name, addr)
                parts = line[6:-1].split(",")
                if len(parts) == 2:
                    name = parts[0].strip()
                    addr_str = parts[1].strip()
                    try:
                        addr = int(addr_str, 16)
                        stubs[name] = addr
                    except:
                        pass
    
    print(f"  Known stubs: {len(stubs)}")
    for name, addr in sorted(stubs.items()):
        rom_off = addr - 0xFF000000
        if 0 <= rom_off < len(rom1):
            first_word = read_u32(rom1, rom_off)
            print(f"    {name}: 0x{addr:08X} (first=0x{first_word:08X})")
        else:
            print(f"    {name}: 0x{addr:08X} (OUT OF RANGE!)")
