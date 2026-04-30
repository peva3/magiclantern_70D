#!/usr/bin/env python3
"""
check_module_symbols.py — Verify all module undefined symbols are resolvable.

Extracts undefined (UND) symbols from built .mo files and checks each
against the camera's symbol file. Reports any symbols that would fail
at TCC module load time.

Usage:
  python3 tests/check_module_symbols.py <mo_file> [<sym_file>]
  python3 tests/check_module_symbols.py --all-cams
  python3 tests/check_module_symbols.py --cam 70D.112

Exit code: 0 if all symbols OK, 1 if any issues found.
"""

import subprocess
import sys
import os
import re


def get_undefined_symbols(mo_file):
    """Extract undefined symbol names from a .mo module file."""
    result = subprocess.run(
        ["arm-none-eabi-readelf", "-sW", mo_file],
        capture_output=True, text=True, check=True
    )
    symbols = set()
    for line in result.stdout.splitlines():
        parts = line.split()
        # Look for: NUM: VALUE  SIZE  TYPE  BIND  VIS   NDX   NAME
        # UNDEFINED symbols have NDX = "UND"
        if len(parts) >= 8 and parts[6] == "UND":
            name = parts[7]
            # Skip internal module symbols
            if name.startswith("__module_"):
                continue
            symbols.add(name)
    return symbols


def get_defined_symbols(sym_file):
    """Extract defined symbols from a camera .sym symbol file.
    
    Format: ADDRESS NAME (one per line, sorted by address).
    """
    defined = set()
    with open(sym_file) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(None, 1)
            if len(parts) == 2:
                defined.add(parts[1])
    return defined


def check_module(mo_file, sym_file, verbose=True):
    """Check a single module against a symbol file."""
    if verbose:
        print(f"  Module: {os.path.basename(mo_file)}")
    
    undef = get_undefined_symbols(mo_file)
    if not undef:
        if verbose:
            print(f"    No undefined symbols — OK")
        return True
    
    defined = get_defined_symbols(sym_file)
    
    missing = undef - defined
    if missing:
        if verbose:
            print(f"    MISSING SYMBOLS ({len(missing)}):")
            for s in sorted(missing):
                if s in ("calloc", "realloc"):
                    print(f"      {s} (may be handled by TCC runtime)")
                else:
                    print(f"      {s}")
        return False
    else:
        if verbose:
            print(f"    All {len(undef)} symbols resolved — OK")
        return True


def find_modules():
    """Find all .mo files and .sym files in the build tree."""
    modules = []
    for root, dirs, files in os.walk("."):
        for f in files:
            if f.endswith(".mo"):
                modules.append(os.path.join(root, f))
    
    sym_files = {}
    for root, dirs, files in os.walk("platform"):
        for f in files:
            if f.endswith(".sym") and not f.startswith("."):
                cam = os.path.basename(root)
                if cam.startswith("build"):
                    continue
                parent = os.path.basename(os.path.dirname(root))
                if parent.isdigit():
                    key = f"{os.path.basename(os.path.dirname(os.path.dirname(root)))}.{os.path.basename(os.path.dirname(root))}"
                else:
                    key = f"{parent}.{cam}"
                sym_files[key] = os.path.join(root, f)
    
    return modules, sym_files


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    
    if sys.argv[1] == "--all-cams":
        modules, sym_files = find_modules()
        all_ok = True
        print(f"Found {len(sym_files)} camera symbol files")
        for cam, sym_file in sorted(sym_files.items()):
            print(f"\n{cam}:")
            found_any = False
            for mo in sorted(modules):
                if cam.replace(".", "/") in mo or cam.replace(".", ".") in mo:
                    if not check_module(mo, sym_file):
                        all_ok = False
                    found_any = True
            if not found_any:
                print(f"  No modules found for this cam")
        return 0 if all_ok else 1
    
    if sys.argv[1] == "--cam" and len(sys.argv) >= 3:
        cam = sys.argv[2]
        modules, sym_files = find_modules()
        
        if cam not in sym_files:
            print(f"Symbol file not found for {cam}")
            print(f"Available: {', '.join(sorted(sym_files.keys()))}")
            return 1
        
        sym_file = sym_files[cam]
        all_ok = True
        print(f"Checking {cam} modules against {sym_file}")
        for mo in sorted(modules):
            if cam.replace(".", "/") in mo or cam.replace(".", ".") in mo or cam.split(".")[0] in mo:
                if not check_module(mo, sym_file):
                    all_ok = False
        return 0 if all_ok else 1
    
    # Single module check
    mo_file = sys.argv[1]
    if not os.path.exists(mo_file):
        print(f"Module not found: {mo_file}")
        return 1
    
    sym_file = sys.argv[2] if len(sys.argv) >= 3 else "platform/70D.112/build/70D_112.sym"
    if not os.path.exists(sym_file):
        # Try to find it in platform/ dirs
        for root, dirs, files in os.walk("platform"):
            syms = [f for f in files if f.endswith(".sym")]
            if syms:
                sym_file = os.path.join(root, syms[0])
                break
    
    ok = check_module(mo_file, sym_file)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
