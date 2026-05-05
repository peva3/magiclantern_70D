"""Ghidra PyGhidra script: Decompile a function at a given address and print its C code
Usage: analyzeHeadless <proj_dir> <proj_name> -process ROM1.BIN -postScript DecompileFunction.py <hex_addr> [label]
@category Canon70D
"""

import sys

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import TaskMonitor

args = getScriptArgs()
if len(args) < 1:
    print("ERROR: Usage: DecompileFunction.py <hex_addr> [label]")
    sys.exit(1)

addr_str = args[0]
label = args[1] if len(args) > 1 else ("func_" + addr_str)

if addr_str.startswith("0x") or addr_str.startswith("0X"):
    addr_str = addr_str[2:]

af = currentProgram.getAddressFactory()

val = int(addr_str, 16)
if val >= 0xFF000000 and val <= 0xFFFFFFFF:
    rom_val = val - 0xFF000000 + 0xF8000000
    addr = af.getAddress("%08X" % rom_val)
else:
    addr = af.getAddress(addr_str.upper())

if addr is None:
    print("ERROR: Invalid address: " + args[0])
    sys.exit(1)

fm = currentProgram.getFunctionManager()
f = fm.getFunctionAt(addr) or fm.getFunctionContaining(addr)

if f is None:
    print("ERROR: No function found at " + str(addr))
    sys.exit(1)

print("=== " + label + " ===")
print("Name: " + f.getName())
print("Address: 0x" + addr.toString(False))
print("Body: " + str(f.getBody()))

decomp = DecompInterface()
decomp.openProgram(currentProgram)

res = decomp.decompileFunction(f, 300, TaskMonitor.DUMMY)
if res is not None and res.decompileCompleted():
    code = res.getDecompiledFunction().getC()
    print(code)
elif res is not None:
    print("Decompile FAILED: " + res.getErrorMessage())
    print("\n=== Instruction Listing ===")
    listing = currentProgram.getListing()
    inst_iter = listing.getInstructions(addr, True)
    count = 0
    while inst_iter.hasNext() and count < 200:
        instr = inst_iter.next()
        if not f.getBody().contains(instr.getAddress()):
            break
        print(str(instr))
        count += 1
else:
    print("Decompile returned None")

decomp.dispose()
