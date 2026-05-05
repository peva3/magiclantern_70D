# Canon 70D Firmware Analysis Plan

## Overview

Reverse engineering the Canon 70D firmware (version 1.1.2) would provide significant insights for Magic Lantern development, particularly:
- Finding the missing LV_FOCUS_DATA memory locations
- Understanding FPS timer registers for the override fix
- Discovering audio control registers
- Mapping the complete memory layout

## Obtaining the Firmware

### Official Download
**Source:** Canon Hong Kong  
**File:** `eos70d-v112-win.zip` (23.80 MB)  
**URL:** https://hk.canon/en/support/0400270102  
**Contains:** `70D00112.FIR` (23,454,336 bytes)

### Alternative Sources
- Canon USA: https://www.usa.canon.com/internet/portal/us/home/support/
- Canon Europe: https://www.canon-europe.com/

### Firmware Details
- **Model:** EOS 70D
- **Version:** 1.1.2
- **File:** `70D00112.FIR`
- **Size:** ~23 MB
- **Release Date:** September 13, 2016

## Firmware Extraction Process

### Step 1: Extract FIR File
```bash
# Extract the downloaded zip
unzip eos70d-v112-win.zip
# Results in: 70D00112.FIR
```

### Step 2: Extract ROM Dumps
Magic Lantern has tools for this in `tools/indy/`:

```python
# Using dump_fir.py
python3 tools/indy/dump_fir.py 70D00112.FIR

# This extracts:
# - main firmware dump
# - memory mapping
# - string references
```

### Step 3: Analyze with Ghidra
```bash
# Using ML's Ghidra scripts
cd tools/ghidra
python3 create_project_from_roms.py /path/to/firmware/dump

# Load in Ghidra for disassembly
# Use DIGIC V processor definition
```

## What We Can Discover

### High Priority for 70D Development

| Current Issue | What We Need to Find | Potential Location |
|--------------|---------------------|-------------------|
| LV_FOCUS_DATA missing | Focus distance/position data during AF | DIGIC image processor memory |
| FPS Override broken | Timer A/B register definitions, blanking regs | 0xC0F06000-0xC0F06FFF range |
| Audio controls | ASIF/Audio IC register mapping | Audio subsystem memory |
| RAW zebras | RAW buffer memory layout, EDMAC channels | Sensor/RAW path memory |
| Focus stacking | Lens position tracking memory | AF drive subsystem |

### Other Discoveries
- Complete property list (all 0x#### IDs)
- State object structures
- Task memory layouts
- Interrupt handlers
- GUI rendering calls

## Analysis Tools Available

### In This Repository

| Tool | Purpose |
|------|---------|
| `tools/indy/dump_fir.py` | Extract FIR to raw binary |
| `tools/indy/fir_tool2.py` | Advanced FIR analysis |
| `tools/firmware/compute_sig.py` | Compute firmware signature |
| `tools/firmware/edmac_config.py` | EDMAC register mapper |
| `tools/firmware/decode_armv7_mmu_tables.py` | MMU table decoder |
| `tools/ghidra/create_project_from_roms.py` | Ghidra project setup |

### External Tools

| Tool | Purpose |
|------|---------|
| **Ghidra** | Disassembler/decompiler (free NSA tool) |
| **IDA Pro** | Professional disassembler (commercial) |
| **radare2** | Open-source reverse engineering framework |

## Analysis Workflow

### Phase 1: Basic Extraction (1-2 hours)
1. Download firmware from Canon
2. Extract FIR file
3. Run dump_fir.py to get ROM dumps
4. Extract string references

### Phase 2: Structural Analysis (4-8 hours)
1. Load main ROM dump in Ghidra
2. Identify DIGIC V processor type (ARM946E-S)
3. Find known entry points (compare with ML stubs)
4. Map memory regions

### Phase 3: Feature-Specific Analysis (8-40 hours)

#### For LV_FOCUS_DATA:
1. Search for AF-related strings
2. Find AF task memory structures
3. Monitor memory during AF operations (need camera or QEMU)
4. Identify where focus distance is computed

#### For FPS Registers:
1. Search for timer/clock-related functions
2. Compare with known working cameras (5D3, 6D)
3. Map FPS_REGISTER_A/B addresses
4. Identify blanking register locations

#### For Audio:
1. Find ASIF subsystem code
2. Map audio codec registers
3. Identify gain control addresses

## Expected Output

### If We Get the Firmware

| File | Description |
|------|-------------|
| `70D_112_ROM0.BIN` | Main firmware dump (~16MB) |
| `70D_112_strings.txt` | All string references |
| `70D_112_funcs.txt` | Function name guesses |
| `70D_112_memory.map` | Memory layout |

### Analysis Results

| Discovery | Impact |
|-----------|--------|
| Focus data location | Fix focus confirmation, Magic Zoom |
| FPS timer registers | Enable FPS override |
| Audio register map | Enable audio controls |
| RAW buffer layout | Fix RAW zebras |

## Current Workaround

Without the firmware, we're using:
1. **Host-side testing** - Test logic without camera
2. **Memory spy** - Once we have a camera, dump memory during AF
3. **Remote testing** - Partner with 70D owners
4. **Cross-camera inference** - Use 6D/5D3 similarities

## Next Steps

1. **Download firmware** - User needs to obtain from Canon
2. **Extract and analyze** - Run extraction tools
3. **Search for key addresses** - Focus, FPS, audio
4. **Verify in code** - Cross-reference with ML stubs

## Resources

- Magic Lantern Firmware Analysis: https://www.magiclantern.fm/forum/index.php?topic=16512
- Ghidra Download: https://ghidra-sre.org/
- DIGIC V Technical Details: https://www.magiclantern.fm/forum/index.php?topic=17548

## Risk Considerations

- **Legal:** Firmware analysis is generally allowed for interoperability/educational purposes
- **Warranty:** Modifying firmware may void warranty
- **Brick Risk:** Never flash modified firmware without understanding

---

*Last Updated: 2026-04-22*
*Status: Waiting for firmware acquisition*