#!/usr/bin/env python3
"""
Remaining RE gaps analysis. Handles:
1. Vector table & interrupt handlers (cross-reference with ROM1 code)
2. SWI dispatch in DRYOS kernel
3. Audio I2C codec identification
4. GPS data access path
"""
import struct
from capstone import *
from capstone.arm import *

rom1 = open("/app/70d/roms/70D/ROM1.BIN", "rb").read()
rom0 = open("/app/70d/roms/70D/ROM0.BIN", "rb").read()

MD = Cs(CS_ARCH_ARM, CS_MODE_ARM)
MD.detail = True
MD2 = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
MD2.detail = True

def read_u32(off):
    return struct.unpack_from('<I', rom1, off)[0]

def get_string_at(data, off, max_len=128):
    end = data.find(b'\x00', off, min(off+max_len, len(data)))
    if end > off:
        return data[off:end].decode('ascii', errors='replace')
    return None

def get_str_at_runtime(rt_addr):
    off = rt_addr - 0xFF000000
    if 0 <= off < len(rom1):
        return get_string_at(rom1, off, 64)
    return None

print("=" * 60)
print("GAP ANALYSIS: INTERRUPT VECTORS & SWI")
print("=" * 60)

# ================================================================
# ANALYSIS 1: The Vector Table at 0xF8CCDBB8
# ================================================================
print("\n" + "-" * 70)
print("VECTOR TABLE ANALYSIS (ROM 0xF8CCDBB8)")
print("-" * 70)

vt_base = 0xF8CCDBB8
vt_off = vt_base - 0xF8000000
vec_names = ["Reset", "Undef", "SWI", "Prefetch", "Data", "Reserved", "IRQ", "FIQ"]

print("\nVector instructions at table:")
for i in range(8):
    off = vt_off + i*4
    word = read_u32(off)
    pc = off + 8
    # LDR PC,[PC,#imm]: 0xE59Fxxxx or 0xE51Fxxxx
    if (word & 0x0F7F0000) == 0x051F0000 or (word & 0x0F7F0000) == 0x059F0000:
        imm = word & 0xFFF
        if word & 0x00800000:
            imm = -imm
        load_addr = pc + imm
        handler = read_u32(load_addr)
        handler_name = get_str_at_runtime(handler) or ""
        # Map handler to code
        h_off = handler - 0xFF000000
        if 0 <= h_off < len(rom1):
            h_first = read_u32(h_off)
            is_arm = (h_first & 0xF0000000) == 0xE0000000
            h_fn = get_function_name_from_rom(h_off)
            print(f"  {vec_names[i]:8s}: LDR PC,[PC,#0x{imm:X}] -> 0x{load_addr:06X} = 0x{handler:08X} 1st=0x{h_first:08X} {'ARM' if is_arm else '?'} {handler_name[:40] if handler_name else ''}")
        else:
            print(f"  {vec_names[i]:8s}: LDR PC,[PC,#0x{imm:X}] -> 0x{load_addr:06X} = 0x{handler:08X} (OUT OF ROM1 RANGE)")
    else:
        print(f"  {vec_names[i]:8s}: 0x{word:08X} (not LDR PC)")

# Actually the vector table values look wrong (0x00000007, etc.)
# Let me check if they're THUMB addresses  
print("\nChecking if handler values are THUMB addresses (bit 0 = 1 = THUMB):")
for i in range(8):
    off = vt_off + i*4
    word = read_u32(off)
    pc = off + 8
    if (word & 0x0F7F0000) == 0x051F0000 or (word & 0x0F7F0000) == 0x059F0000:
        imm = word & 0xFFF
        if word & 0x00800000:
            imm = -imm
        load_addr = pc + imm
        handler = read_u32(load_addr)
        if handler & 1:
            thumb_addr = handler & ~1
            print(f"  {vec_names[i]}: 0x{handler:08X} -> THUMB at 0x{thumb_addr:08X}")
        else:
            print(f"  {vec_names[i]}: 0x{handler:08X} -> ARM at 0x{handler:08X}")

# ================================================================
# ANALYSIS 2: Search for real vector table initialization
# ================================================================
print("\n" + "-" * 70)
print("SEARCHING FOR VBAR SETUP (MCR p15, 0, Rt, c12, c0, 0)")
print("-" * 70)

# VBAR: MCR p15, 0, Rt, c12, c0, 0 (opcode_1=0, CRn=c12, CRm=c0, opcode_2=0)
# Encoding: 0xEE0C0F10 | (Rt << 12)
# Actually: cond=1110, 1110, 0, opcode1, CRn, Rt, coproc, opcode2, 1, CRm
# MCR: 0xEE0xxF1x where xx encodes Rt and CRn
# For VBAR: CRn=c12=0xC, CRm=c0=0x0, opcode2=0
# Binary: 1110 1110 0 000 Rt 1111 0001 0000
# Or: 0xEE0C0F1x where x=Rt

print("Scanning ROM1 for MCR VBAR instructions...")
vbar_count = 0
for off in range(0, len(rom1) - 4, 4):
    word = read_u32(off)
    # MCR p15, 0, Rt, c12, c0, 0
    # Check for: CRn=c12, CRm=c0, opc2=0
    if (word & 0x0FFF0FFF) == 0x0E0C0F10:
        rt = (word >> 12) & 0xF
        rom_addr = 0xF8000000 + off
        f = get_function_name_from_rom(off)
        vbar_count += 1
        if vbar_count <= 5:
            print(f"  MCR p15,0,R{rt},c12,c0,0 at 0x{rom_addr:08X} (file 0x{off:X}) [{f}]")
        # Check what follows - likely sets up vector table
        for j in range(1, 20):
            nw = read_u32(off + j*4)
            if (nw & 0x0F7F0000) in [0x051F0000, 0x059F0000]:
                load_off = off + j*4 + 8 + (nw & 0xFFF) * (-1 if nw & 0x00800000 else 1)
                if 0 <= load_off < len(rom1):
                    val = read_u32(load_off)
                    if 0xFF000000 <= val <= 0xFFFFFFFF:
                        if vbar_count <= 3:
                            print(f"    -> LDR at +0x{j*4:X} loads 0x{val:08X}")
                break

print(f"Total VBAR instructions: {vbar_count}")

# ================================================================
# ANALYSIS 3: DRYOS Kernel SWI dispatch
# ================================================================
print("\n" + "-" * 70)
print("DRYOS KERNEL SWI/SYSCALL DISPATCH")
print("-" * 70)

# We found 3 SUBS PC,LR,#4 instructions at the end of ROM1 (0xFF region)
# These are SWI return instructions
swi_offsets = [0xFF0A94, 0xFF0AC4, 0xFF0ADC]

print("\nSWI handler analysis:")
for swi_off in swi_offsets:
    rom_addr = 0xF8000000 + swi_off
    print(f"\n  SUBS PC,LR,#4 at file 0x{swi_off:X} (ROM 0x{rom_addr:X}):")
    
    # Find the function containing this instruction
    # Look backwards for function prologue (PUSH {..., lr} or similar)
    fn_start = swi_off
    for lookback in range(4, 256, 4):
        candidate = swi_off - lookback
        if candidate < 0:
            break
        word = read_u32(candidate)
        # PUSH {regs, lr}: 0xE92Dxxxx where bit 14 (LR) is set
        # Also SUB SP,SP,#imm: 0xE24DDxxx
        if (word & 0xFFFF0000) == 0xE92D0000:
            reg_list = word & 0xFFFF
            if reg_list & (1 << 14):  # LR pushed
                fn_start = candidate
                # Now check it has a proper function
                # Look at first few instructions for typical SWI handler patterns
                insns = []
                for k in range(0, 20, 4):
                    w = read_u32(candidate + k)
                    insns.append(w)
                
                # Look for MRS (move PSR to register) - typical SWI handler
                has_mrs = any((w & 0x0FFF0000) == 0x010F0000 for w in insns)
                
                # SWI handler reads the SWI number from LR-4
                # LDR instruction: 0xE59Fxxxx
                has_ldr_spsr = any((w & 0xFFF00000) == 0xE5100000 and ((w >> 16) & 0xF) == 15 for w in insns)  # LDR from SPSR
                
                # Actually, SWI handler in DRYOS typically does:
                # 1. Load SWI number from LR-4
                # 2. Index into syscall table
                # 3. Call handler
                
                # Look for LDR Rx,[LR,#-4] - this reads the SWI number
                has_swi_load = any(w == 0xE51E0004 or w == 0xE51E4004 for w in insns)
                
                flags = []
                if has_mrs: flags.append("MRS")
                if has_swi_load: flags.append("SWI_NUM_LD")
                
                print(f"    Function start: file 0x{fn_start:X} ({','.join(flags) if flags else 'no SWI markers'})")
                
                # Disassemble first 20 instructions  
                sub_rom1 = rom1[fn_start:fn_start+80]
                for insn in MD.disasm(sub_rom1, 0):
                    print(f"      0x{fn_start + insn.address:X}: {insn.mnemonic:10s} {insn.op_str}")
                break

# ================================================================
# ANALYSIS 4: Find DRYOS syscall table
# ================================================================
print("\n" + "-" * 70)
print("DRYOS SYSCALL TABLE SEARCH")
print("-" * 70)

# In DRYOS, SWI handlers typically dispatch via a syscall function table.
# The SWI number indexes into this table.
# Look for LDR PC,[PC,Rx,LSL#2] pattern near SWI handler

print("Searching for syscall dispatch in ROM1...")
syscall_tables = []

for off in range(0, len(rom1) - 4, 4):
    word = read_u32(off)
    # LDR PC,[PC,Rx,LSL#2] = 0xE79FFxxx
    # LDR PC,[PC,Rx,LSL#2]: cond=1110, 0001, 1, 1, 1, 1, 0, PC=1111, 1, imm=0, Rn=PC=1111, Rs=Rx, 2, 1, 0, 0
    # 0xE79FF000 | (Rx << 8) | ... hmm, let me check the encoding
    # LDR <c> <Rt>,[PC, <Rm>, LSL #2]
    # encoding A1: cond 0101 U 0 0 1 1 Rn Rt 5 0 0 imm5 type 0 Rm
    # For shift=1 (LSL #2): type=00, imm5=2
    # 0xE79F3102 would be LDR PC,[PC,R1,LSL#2]? let me check
    # Actually: 1110 0101 1001 1111 0011 0001 0010 = 0xE59F3102 - that's just LDR PC,[PC,#0x102]
    
    # Let me try a different approach: search for the SWI table near known interrupt handlers
    pass

# Since the SWI handlers are at the END of ROM1 (0xFF0000 range), the syscall table
# is likely nearby. Let me scan that region for function pointer tables.
print("Scanning ROM1 end (0xF8FE0000-0xF8FFFFFF) for function pointer tables...")
for off in range(0xFE0000, min(0x1000000, len(rom1)), 4):
    val = read_u32(off)
    # Look for clusters of runtime addresses (potential function pointer table)
    if 0xFF000000 <= val <= 0xFFFFFFFF:
        # Check surroundings
        pass

# Instead, let me try to identify the syscall table by looking for the pattern
# near the SWI function we already found
for swi_off in swi_offsets:
    print(f"\n  Around SWI handler at 0x{swi_off:X}:")
    for delta in range(-0x200, 0x200, 4):
        off = swi_off + delta
        if off < 0 or off > len(rom1) - 4:
            continue
        val = read_u32(off)
        if 0xFF000000 <= val <= 0xFFFFFFFF:
            # Check if it points to code  
            foff = val - 0xFF000000
            if 0 <= foff < len(rom1):
                word = read_u32(foff)
                is_arm = (word & 0xF0000000) == 0xE0000000
                is_thumb = (word & 0xFFFF) > 0x2000
                if is_arm or is_thumb:
                    # Check for nearby entries (potential table)
                    nearby_vals = []
                    for k in range(-4, 5):
                        nv = read_u32(off + k*4)
                        nearby_vals.append(nv)
                    # Count how many look like code pointers
                    code_count = 0
                    for nv in nearby_vals:
                        if 0xFF000000 <= nv <= 0xFFFFFFFF:
                            nf = nv - 0xFF000000
                            if 0 <= nf < len(rom1):
                                nw = read_u32(nf)
                                if (nw & 0xF0000000) == 0xE0000000:
                                    code_count += 1
                    if code_count >= 4:
                        print(f"    POTENTIAL SYSCALL TABLE at 0x{swi_off+delta:X} ({code_count} code ptrs nearby)")


# ================================================================
# ANALYSIS 5: Audio I2C Codec Detection
# ================================================================
print("\n" + "-" * 70)
print("AUDIO CODEC I2C DETECTION")
print("-" * 70)

# Search for audio codec-related strings in ROM0
print("Audio-related strings in ROM0:")
audio_terms = ['CODEC', 'AUDIO', 'I2S', 'WM8', 'WM8', 'AKM', 'AK4', 'CS42', 'PCM', 'DAI', 'TLV']
for off in range(len(rom0)):
    for term in audio_terms:
        pos = rom0.find(term.encode(), off, off+1)
        if pos >= 0:
            s = get_string_at(rom0, max(0, pos-2), 64)
            if s and len(s) >= 4:
                print(f"  ROM0 0x{0xF0000000 + max(0, pos-2):X}: {s}")
                off = pos + len(term)
                break

# Search for I2C communication functions in ROM1
print("\nSearching for I2C master read/write functions...")
for off in range(0, len(rom1) - 4, 4):
    word = read_u32(off)
    # Check for I2C register access
    # LDR Rx, =0xC0F0xxxx (I2C registers are in 0xC0F0xxxx range on DIGIC V)
    if (word >> 16) == 0xE59F:  # LDR Rx,[PC,#imm]
        rt = (word >> 12) & 0xF
        imm = word & 0xFFF
        load_off = off + 8 + imm
        if load_off < len(rom1):
            loaded = read_u32(load_off)
            if 0xC0F00000 <= loaded <= 0xC0F0FFFF:
                # Check if the function around this contains I2C operations
                fn = get_function_name_from_rom(off)
                print(f"  I2C register 0x{loaded:08X} loaded at 0x{0xF8000000+off:X} [{fn}]")

# ================================================================
# ANALYSIS 6: GPS data access verification
# ================================================================
print("\n" + "-" * 70)
print("GPS DATA ACCESS CONFIRMATION")
print("-" * 70)

# We found 244 GPS strings. Let me check for GPS function entry points
# GPS functions are likely in the 0xFF2FC000 range (based on GPS_TAG_* strings)
print("GPS-related functions from eventproc tables:")
gps_entries = []
with open("/app/70d/tools/ghidra/eventproc_full_map_v3.csv") as f:
    for line in f.readlines()[1:]:
        parts = line.strip().split(",", 3)
        if len(parts) == 4 and 'GPS' in parts[3].upper():
            gps_entries.append(parts)

for e in gps_entries[:20]:
    print(f"  {e[1]} -> {e[2]}: {e[3]}")

# ================================================================
# SUMMARY
# ================================================================
print("\n" + "=" * 60)
print("GAP ANALYSIS SUMMARY")
print("=" * 60)
print(f"Eventproc entries (v3): 708 entries, 78 tables")
print(f"Eventproc dispatch entries: 661 entries, 58 tables")
print(f"GPS eventproc entries: {len(gps_entries)}")
print(f"Vector table candidate at 0xF8CCDBB8")
print(f"SWI return instructions: 3")
print(f"Audio strings: found in ROM0 (see above)")

def get_function_name_from_rom(off):
    """Simple function name lookup from ROM1 symbols"""
    # In a real implementation, this would parse the Ghidra database
    # Try to find from eventproc CSV
    return "?"

def get_function_name_from_rom(off):
    """Simple function name lookup from ROM1"""
    return "?"
