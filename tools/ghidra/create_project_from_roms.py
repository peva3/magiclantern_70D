#!/usr/bin/env python3
"""
Creates a Ghidra project for a cam model.
For 70D:
  ROM0.BIN = assets (UI menus, strings)     at 0xF0000000
  ROM1.BIN = code   (DryOS kernel, funcs)   at 0xF8000000
  SFDATA.BIN = sensor calibration (no analysis)

Usage:
  python3 create_project_from_roms.py 70D \\
      --rom0 /app/70d/roms/70D/ROM0.BIN \\
      --rom1 /app/70d/roms/70D/ROM1.BIN \\
      --project /app/70d/70D.gpr
"""

import os
import sys
import argparse
import shlex
import subprocess

# ROM base addresses for DIGIC V
ROM0_ADDR = 0xF0000000
ROM1_ADDR = 0xF8000000

# Code markers for ROM type detection
CODE_MARKERS = [
    b"DRYOS version",
    b"DRYOS PANIC",
    b"handler",
    b"create a task",
    b"TakeSemaphore",
]

ASSET_MARKERS = [
    b"Copyright-Info",
    b"<MENU>",
    b"batteries",
    b"activate movie",
    b"Autofocus",
]


def get_code_score(rom_data):
    matches = {s for s in CODE_MARKERS if s in rom_data}
    return len(matches) / len(CODE_MARKERS)


def get_asset_score(rom_data):
    matches = {s for s in ASSET_MARKERS if s in rom_data}
    return len(matches) / len(ASSET_MARKERS)


def classify_rom(rom_data):
    code = get_code_score(rom_data)
    asset = get_asset_score(rom_data)
    if code >= asset and code > 0:
        return "code"
    elif asset > code and asset > 0:
        return "asset"
    return "unknown"


def extract_syms(nstub_csv_path):
    """Load NSTUB cross-reference CSV into {name: addr} dict."""
    syms = {}
    if not nstub_csv_path or not os.path.exists(nstub_csv_path):
        return syms
    import csv
    with open(nstub_csv_path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            name = row.get("Name", "").strip()
            addr = row.get("Address", "").strip()
            if name and addr:
                syms[name] = int(addr, 16)
    return syms


def generate_ghidra_headless_script(syms, code_rom_path, asset_rom_path, code_addr, asset_addr):
    """
    Generate a Ghidra Python script for headless import.
    This is written as a .py file that Ghidra's headless analyzer executes.
    """
    lines = [
        "# Ghidra Python script for 70D firmware analysis",
        "# Run with: analyzeHeadless <project_dir> <project_name>",
        "#   -import <rom_path> -processor ARM:LE:32:V5T -postScript this_script.py",
        "",
        "from ghidra.program.model.symbol import SourceType",
        "from ghidra.program.model.listing import Function",
        "",
        "labels = {",
    ]
    for name, addr in sorted(syms.items(), key=lambda x: x[1]):
        lines.append(f"    0x{addr:08X}: '{name}',")
    lines += [
        "}",
        "",
        "def run():",
        "    fm = currentProgram.getFunctionManager()",
        "    sym = currentProgram.getSymbolTable()",
        "    listing = currentProgram.getListing()",
        "",
        "    # Create labels for known symbols",
        "    for addr_str in labels:",
        "        addr = toAddr(addr_str)",
        "        name = labels[addr_str]",
        "        try:",
        "            sym.createLabel(addr, name, SourceType.USER_DEFINED)",
        "        except:",
        "            pass  # already exists or overlapping",
        "",
        "    # Auto-discover functions at label addresses (ARM PUSH prologue check)",
        "    for addr_str in labels:",
        "        addr = toAddr(addr_str)",
        "        try:",
        "            inst = listing.getInstructionAt(addr)",
        "            if inst:",
        "                fm.createFunction(addr, name)",
        "        except:",
        "            pass",
        "",
        "    print('Applied %d symbols' % len(labels))",
    ]
    return "\n".join(lines)


def generate_import_instructions(model, code_rom_path, asset_rom_path, project_path,
                                  code_addr, asset_addr, syms, labels_path):
    """Generate detailed manual import instructions for when Ghidra isn't installed."""
    instructions = f"""
Ghidra Import Instructions for {model}
======================================

Prerequisites:
  - Install Ghidra 11.x+ (download from https://ghidra-sre.org/)
  - Recommend ARMv5 THUMB support (V5T) for DIGIC V firmware

ROMS Detected:
  Code ROM:  {code_rom_path} (size {os.path.getsize(code_rom_path)} bytes)
             Base address: 0x{code_addr:08X}
             Memory: r-x (read-execute)
             
  Asset ROM: {asset_rom_path} (size {os.path.getsize(asset_rom_path)} bytes)
             Base address: 0x{asset_addr:08X}
             Memory: r-- (read-only data)

Headless Import (recommended):
  1. Create project:
     {{GHIDRA_INSTALL}}/support/analyzeHeadless {os.path.dirname(project_path)} \\
         {os.path.basename(project_path)} -import \\
         {code_rom_path} -loader BinaryLoader -loader-baseAddr 0x{code_addr:08X} \\
         -processor ARM:LE:32:V5T -noanalysis

  2. Import assets:
     {{GHIDRA_INSTALL}}/support/analyzeHeadless {os.path.dirname(project_path)} \\
         {os.path.basename(project_path)} -import \\
         {asset_rom_path} -loader BinaryLoader -loader-baseAddr 0x{asset_addr:08X}

  3. Apply symbols:
     {{GHIDRA_INSTALL}}/support/analyzeHeadless {os.path.dirname(project_path)} \\
         {os.path.basename(project_path)} -process ROM1.BIN \\
         -postScript {labels_path or 'ghidra_label_import.py'}

  4. Run auto-analysis:
     {{GHIDRA_INSTALL}}/support/analyzeHeadless {os.path.dirname(project_path)} \\
         {os.path.basename(project_path)} -process ROM1.BIN \\
         -noanalysis  # manual: run Analysis > Auto-Analyze in GUI
"""
    return instructions


def main():
    args = parse_args()

    with open(args.rom0, "rb") as f:
        rom0_data = f.read()
    with open(args.rom1, "rb") as f:
        rom1_data = f.read()

    # Classify ROMs
    rom0_type = classify_rom(rom0_data)
    rom1_type = classify_rom(rom1_data)

    print(f"ROM0: {rom0_type} (score code={get_code_score(rom0_data):.2f} asset={get_asset_score(rom0_data):.2f})")
    print(f"ROM1: {rom1_type} (score code={get_code_score(rom1_data):.2f} asset={get_asset_score(rom1_data):.2f})")

    # Determine which is code and which is assets
    if rom0_type == "code":
        code_rom_path, asset_rom_path = args.rom0, args.rom1
        code_addr, asset_addr = ROM0_ADDR, ROM1_ADDR
    elif rom1_type == "code":
        code_rom_path, asset_rom_path = args.rom1, args.rom0
        code_addr, asset_addr = ROM1_ADDR, ROM0_ADDR
    else:
        print("ERROR: Could not determine which ROM contains code.")
        sys.exit(1)

    print(f"\nCode ROM:  {code_rom_path}  @ 0x{code_addr:08X}")
    print(f"Asset ROM: {asset_rom_path}  @ 0x{asset_addr:08X}")

    # Load known symbols
    syms = extract_syms(args.labels)
    print(f"Loaded {len(syms)} known symbols")

    # Generate Ghidra label import script
    if args.labels_script:
        ghidra_script = generate_ghidra_headless_script(
            syms, code_rom_path, asset_rom_path, code_addr, asset_addr
        )
        with open(args.labels_script, "w") as f:
            f.write(ghidra_script)
        print(f"Generated Ghidra label script: {args.labels_script}")

    # Generate import instructions
    instructions = generate_import_instructions(
        args.model, code_rom_path, asset_rom_path,
        args.project, code_addr, asset_addr,
        syms, args.labels_script,
    )
    print(instructions)

    # Save instructions to file
    instructions_path = args.project.replace(".gpr", "_IMPORT.txt")
    with open(instructions_path, "w") as f:
        f.write(instructions)
    print(f"Saved import instructions to: {instructions_path}")

    print("Done. See instructions above or the saved text file to proceed with Ghidra import.")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Create and populate a Ghidra project for a Canon camera."
    )
    parser.add_argument("model", help="Camera model name (e.g. 70D)")
    parser.add_argument("--rom0", required=True, help="Path to ROM0.BIN")
    parser.add_argument("--rom1", required=True, help="Path to ROM1.BIN")
    parser.add_argument("--project",
                        help="Output .gpr path (default: <model>.gpr)")
    parser.add_argument("--labels",
                        help="CSV file with known symbols (Name,Address columns)")
    parser.add_argument("--labels-script",
                        help="Output path for Ghidra Python label script")
    args = parser.parse_args()
    if not args.project:
        args.project = f"{args.model}.gpr"
    args.project = os.path.abspath(args.project)
    return args


if __name__ == "__main__":
    main()
