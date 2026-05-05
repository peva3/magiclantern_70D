#!/usr/bin/env python3
"""Generate combined symbol table from RAMDUMP.md ML symbols + NSTUB cross-reference."""

import re
import csv
import sys

symbols = {}

# Extract from RAMDUMP.md Section 4 - ML Symbol Table
with open('RAMDUMP.md', 'r') as f:
    content = f.read()

# Find the table rows in the ML Symbol Table section
# Pattern: | \`symbol_name\` | \`0xADDR\` | desc |
for line in content.split('\n'):
    line_s = line.strip()
    if line_s.startswith('| `') and line_s.count('|') >= 4:
        parts = [p.strip() for p in line_s.split('|')]
        sym = parts[1].strip('`').strip()
        addr_str = parts[2].strip('`').strip()
        if addr_str.startswith('0x') or addr_str.startswith('0X'):
            try:
                addr = int(addr_str, 16)
                symbols[sym] = addr
            except ValueError:
                pass

print(f'Extracted {len(symbols)} ML symbols from RAMDUMP.md')

# Load NSTUBs
nstubs = {}
try:
    with open('tools/ghidra/nstub_crossref.csv', 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            name = row['Name'].strip()
            addr = row['Address'].strip()
            if addr.startswith('0x'):
                nstubs[name] = int(addr, 16)
    print(f'Loaded {len(nstubs)} NSTUBs from crossref')
except FileNotFoundError:
    print('Warning: NSTUB crossref not found')

# Merge (NSTUBs override where conflicts exist)
for k, v in nstubs.items():
    symbols[k] = v

print(f'Merged: {len(symbols)} total unique symbols')

# Write combined CSV
with open('tools/ghidra/all_symbols.csv', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['Name', 'Address', 'Source'])
    for name, addr in sorted(symbols.items(), key=lambda x: x[1]):
        source = 'NSTUB' if name in nstubs else 'ML_SYM'
        w.writerow([name, f'0x{addr:08X}', source])
print('Saved: tools/ghidra/all_symbols.csv')

# Count by address range
counts = {}
for _, addr in symbols.items():
    if addr >= 0xFF000000:
        k = 'ROM1 (0xFFxxxxxx)'
    elif addr >= 0xC0000000:
        k = 'MMIO (0xC0xxxxxx)'
    elif addr >= 0x40000000:
        k = 'RAM (0x40xxxxxx)'
    elif addr >= 0x00050000:
        k = 'RAM-module (0x0005xxxx)'
    elif addr >= 0x00030000:
        k = 'DryOS-kernel (0x0003xxxx)'
    else:
        k = 'low-RAM (0x0000xxxx)'
    counts[k] = counts.get(k, 0) + 1

print('\nBy address region:')
for k, v in sorted(counts.items()):
    print(f'  {k}: {v}')

# Also write Ghidra label import CSV
with open('tools/ghidra/all_symbols_ghidra.csv', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['Label', 'Address', 'Comment'])
    for name, addr in sorted(symbols.items(), key=lambda x: x[1]):
        w.writerow([name, f'0x{addr:08X}', ''])
print('Saved: tools/ghidra/all_symbols_ghidra.csv')
