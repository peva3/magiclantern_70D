#!/usr/bin/env python3
"""
ARM disassembly analysis of Canon 70D firmware using Capstone.
Disassembles key functions and discovers eventproc tables.
"""
import struct
from capstone import *
from capstone.arm import *

rom1 = open("/app/70d/roms/70D/ROM1.BIN", "rb").read()

MD = Cs(CS_ARCH_ARM, CS_MODE_ARM)
MD.detail = True
MD2 = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
MD2.detail = True

def disasm_code(data, offset, size=256, mode='arm'):
    """Disassemble at file offset"""
    code = data[offset:offset+size]
    md = MD if mode == 'arm' else MD2
    result = []
    for insn in md.disasm(code, 0):
        result.append((offset + insn.address, insn.mnemonic, insn.op_str))
    return result

def read_u32(off):
    return struct.unpack_from('<I', rom1, off)[0]

def read_u16(off):
    return struct.unpack_from('<H', rom1, off)[0]

def get_string_at(off, max_len=128):
    end = rom1.find(b'\x00', off, min(off+max_len, len(rom1)))
    if end > off:
        return rom1[off:end].decode('ascii', errors='replace')
    return None

print("=" * 70)
print("70D FIRMWARE DISASSEMBLY ANALYSIS")
print("=" * 70)

# ================================================================
# PART 1: call() FUNCTION AT 0xFF14339C (file offset 0x14339C)
# ================================================================
print("\n" + "-" * 70)
print("PART 1: call() DISPATCH FUNCTION")
print("File offset: 0x14339C (runtime 0xFF14339C)")
print("-" * 70)

call_off = 0x14339C
# Read a good chunk after call()
insns = disasm_code(rom1, call_off, 400)
for off, mnem, op_str in insns:
    print(f"  0x{off:06X}:  {mnem:10s} {op_str}")

# ================================================================
# PART 2: TABLE WALKER AT 0xF8144420 (file offset 0x144420)
# ================================================================
print("\n" + "-" * 70)
print("PART 2: TABLE WALKER (0xF8144420 = file 0x144420)")
print("-" * 70)

walker_off = 0x144420
insns = disasm_code(rom1, walker_off, 128)
for off, mnem, op_str in insns:
    print(f"  0x{off:06X}:  {mnem:10s} {op_str}")

# ================================================================
# PART 3: DISPATCH HELPER AT 0xF8144340 (file 0x144340)
# ================================================================
print("\n" + "-" * 70)
print("PART 3: DISPATCH HELPER (0xF8144340 = file 0x144340)")
print("-" * 70)

helper_off = 0x144340
insns = disasm_code(rom1, helper_off, 256)
for off, mnem, op_str in insns:
    print(f"  0x{off:06X}:  {mnem:10s} {op_str}")

# ================================================================
# PART 4: Find data references from call() - look for LDR Rx, =addr
# ================================================================
print("\n" + "-" * 70)
print("PART 4: DATA REFERENCES FROM call() REGION")
print("-" * 70)

# LDR Rx, [PC, #imm] loads from literal pool
# Scan call() for LDR instructions that reference eventproc tables
for off in range(call_off, call_off + 400, 4):
    word = read_u32(off)
    # LDR Rt,[PC,#imm12]: cond=1110, 0101, U=1, 1, 0, Rt, imm12
    # Format: 0xE59Fxxxx
    if (word & 0x0F7F0000) == 0x059F0000:
        rt = (word >> 12) & 0xF
        imm = word & 0xFFF
        # PC is current_addr + 8
        pc = off + 8
        target_off = pc + imm
        if target_off < len(rom1):
            loaded_val = read_u32(target_off)
            print(f"  0x{off:06X}: LDR R{rt}, [PC,#0x{imm:X}] -> load from 0x{target_off:06X} = 0x{loaded_val:08X}")

# ================================================================
# PART 5: Find eventproc table structure by tracing the walker
# ================================================================
print("\n" + "-" * 70)
print("PART 5: EVENTPROC TABLE STRUCTURE ANALYSIS")
print("-" * 70)

# From the walker at 0x144420, let me understand the table format
# Look at the LDR instructions in the helper (0x144340) which loads table entries

# First identify all LDR instructions in the call dispatch region
print("LDR instructions in call() region (0x14339C - 0x144500):")
for off in range(0x14339C, 0x144500, 4):
    word = read_u32(off)
    # LDR type instructions
    if (word >> 24) == 0xE5:  # LDR/STR class
        rt = (word >> 12) & 0xF
        rn = (word >> 16) & 0xF
        is_load = (word >> 20) & 1
        if is_load:  # LDR (bit 20 = 1 for LDR)
            if (word & 0x02000000):  # immediate offset
                imm = word & 0xFFF
                if (word & 0x00800000):  # U=0 -> subtract
                    imm = -imm
                print(f"  0x{off:06X}: LDR R{rt}, [R{rn}, #{imm:+d}]")

# Check for LDM (load multiple) instructions  
print("\nLDM instructions in call() region:")
for off in range(0x14339C, 0x144500, 4):
    word = read_u32(off)
    if (word >> 24) == 0xE8 and ((word >> 20) & 1):  # LDM
        rn = (word >> 16) & 0xF
        reg_list = word & 0xFFFF
        print(f"  0x{off:06X}: LDM R{rn}, {{{reg_list:016b}}}")

# ================================================================
# PART 6: LITERAL POOL SCAN
# The eventproc table addresses are loaded from literal pools
# Scan the literal pool near call() for pointers to tables
# ================================================================    
print("\n" + "-" * 70)
print("PART 6: LITERAL POOL NEAR call()")
print("-" * 70)

# Scan for potential table pointers in literal pool area
# The literal pool is typically right after the function code
for off in range(0x144400, 0x144600, 4):
    val = read_u32(off)
    # Check if this looks like a pointer to a table
    # Table would have: {str_ptr, func_ptr} entries
    if 0xF8000000 <= val < 0xF9000000:
        table_off = val - 0xF8000000
        if table_off < len(rom1):
            # Check first few entries
            sp = read_u32(table_off)
            fp = read_u32(table_off + 4)
            print(f"  0x{off:06X}: 0x{val:08X} (table?) sp=0x{sp:08X} fp=0x{fp:08X}")
    elif 0xFF000000 <= val <= 0xFFFFFFFF:
        # Runtime address - convert to file offset
        foff = val - 0xFF000000
        if 0 <= foff < len(rom1):
            word = read_u32(foff)
            print(f"  0x{off:06X}: 0x{val:08X} (runtime -> file 0x{foff:X}, word=0x{word:08X})")
