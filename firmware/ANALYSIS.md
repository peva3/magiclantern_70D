# Firmware Analysis Results - Canon EOS 70D

## Firmware Files Available

| File | Version | Size | SHA256 |
|------|---------|------|--------|
| `70D00112.FIR` | 1.1.2 | 23,454,336 bytes | `4c020f675448a1cfd845249bcc4dd171...` |
| `70D00113.FIR` | 1.1.3 | 23,448,256 bytes | `4c0ef3f82416823b949780e1a3ab6931...` |

## Analysis Summary

### Encryption/Compression Status

**Result: The firmware is heavily encrypted/compressed**

| Metric | Value | Interpretation |
|--------|-------|----------------|
| Entropy (all regions) | ~7.99-8.0 | Maximum randomness - encrypted |
| Visible strings | None in plaintext | Encrypted |
| Compression markers | Few false positives | Not standard compression |
| Byte distribution | Uniform | Encrypted |

### Difficulty Assessment: HIGH

The 70D firmware uses **Canon-proprietary encryption** that makes it difficult to reverse engineer without:

1. **Known decryption key** - Canon uses a unique per-model key
2. **Decryption stub** - The camera's ROM has a built-in decryption routine
3. **Official tools** - Canon uses internal tools for firmware creation

### What We Know About the FIR Structure

```
Offset 0x00: Magic "0x25030080" (Canon FIR header)
Offset 0x10: Version string "1.1.2"
Offset 0x20: Entry point / address info
...
Offset 0x165CAD+: Encrypted/compressed firmware body
```

## Approaches for Future Analysis

### Option 1: Use Canon Decryption (If Available)
Some older Canon firmware versions have known vulnerabilities. The 70D may or may not be crackable.

### Option 2: QEMU with ROM Extraction
If we can dump the firmware from a running camera (or QEMU):
- Extract the decrypted ROM from memory
- Analyze the actual running code

### Option 3: Compare With Similar Cameras
Compare with already-cracked 70D firmware (if available in ML community):
- Check ML's source for hints about addresses
- Use known working stubs as reference

### Option 4: Brute Force Memory Dumping
With a working 70D:
- Use ML to dump memory regions during operation
- Find focus data, timer registers, etc. empirically

## Current Workaround

Since we cannot easily decrypt the firmware, we continue with:

1. **Host-side testing** - Test logic without firmware
2. **Memory spy** - Once we have a camera, dump memory
3. **Cross-camera inference** - Use 6D/5D3 similarities
4. **Remote testing** - Partner with 70D owners

## Tools in This Repository

| Tool | Status | Use |
|------|--------|-----|
| `tools/indy/dump_fir.py` | Python 2 only | Legacy FIR extraction |
| `tools/indy/fir_tool2.py` | Python 2 only | Advanced FIR analysis |
| `tools/ghidra/create_project_from_roms.py` | Needs firmware | Ghidra integration |

## Recommendations

1. **Contact ML Community** - The nikfreak (original 70D port developer) may have decryption notes
2. **Check ML Forum** - Someone may have already cracked the 70D firmware
3. **Try QEMU** - If QEMU supports 70D, we can extract decrypted code from memory

## Files in This Directory

```
firmware/
├── 70D00112.FIR          # Canon 70D v1.1.2 firmware (23 MB)
├── 70D00113.FIR          # Canon 70D v1.1.3 firmware (23 MB)
├── eos70d-v112-win.zip   # Official download
├── eos70d-v113-win.zip   # Official download
└── update-procedure-pdf/ # Manuals (5 languages)
```

---

*Analysis Date: 2026-04-22*
*Status: Encrypted - direct reverse engineering not feasible without decryption key*