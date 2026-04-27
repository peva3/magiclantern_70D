# QEMU 70D Validation Report

## Executive Summary

**Status:** ✅ All software-level validation passed  
**Hardware Required:** Yes, for full crop_rec calibration (S5.5-S5.9)  
**QEMU Limitations:** Cannot access LiveView sensor data or validate actual crop frames

---

## Validation Results

### ✅ Passed Tests (Software Only)

#### 1. Module Structure
- [x] crop_rec.mo exists (32,968 bytes)
- [x] Module included in magiclantern.zip
- [x] 70D-specific crop presets defined (8 presets)
- [x] 70D register addresses correct:
  - `CMOS_WRITE = 0x26B54`
  - `ADTG_WRITE = 0x2684C`
  - `ENGIO_WRITE = 0xFF2BC6C4`
- [x] 70D timer tables defined (TG_FREQ_BASE=32MHz)
- [x] Build size safe (451KB < 600KB limit)

#### 2. QEMU Emulation
- [x] Firmware boots: `startupInitializeComplete` at ~576ms
- [x] ML GUI initializes: `GuiFactoryRegisterEventCommissionProcedure` at ~608ms
- [x] 0 unknown MPU messages (all 26+ spells handled)
- [x] MPU communication stable (no hangs, no crashes)

#### 3. Code Review
- [x] 70D init block present in crop_rec.c
- [x] CMOS[7] used for vertical windowing (vs CMOS[1] on 5D3)
- [x] Timer A/B recalculated for 70D (32MHz base)
- [x] center_canon_preview() uses 70D sensor dims (5472x3648)
- [x] 3X_TALL CMOS override added

---

## QEMU Limitations

### What QEMU CANNOT Validate

1. **LiveView Sensor Access**
   - crop_rec requires LiveView to be active
   - QEMU does not emulate sensor readout
   - Cannot test actual crop frame capture

2. **ENGIO/CMOS Register Writes**
   - Register addresses verified in code
   - Actual hardware writes not emulated
   - Cannot validate register effects on image

3. **Visual Crop Frame Inspection**
   - No display emulation for crop boundaries
   - Cannot verify frame alignment
   - Cannot detect corruption patterns

4. **Timer Stabilization**
   - Timer A/B values calculated theoretically
   - Cannot verify actual FPS stability
   - Cannot detect banding from Timer B

---

## Hardware Testing Required

### S5.5 - CMOS Register Calibration
**Status:** 🔲 Requires physical 70D  
**Registers to Calibrate:**
- CMOS[2]: 0x10E, 0x0BE, 0x08E, 0x07E, 0x09E (per preset)
- CMOS[6]: 0x170, 0x370 (highlight fix)
- CMOS[7]: Vertical windowing values

**Test Method:** Capture crop frames, inspect for artifacts

### S5.6 - ENGIO Register Calibration
**Status:** 🔲 Requires physical 70D  
**Registers to Calibrate:**
- 0xC0F06800: Top-bar offsets
- 0xC0F06804: End-column values
- HEAD3/4 base values

**Test Method:** Inspect crop frames for alignment issues

### S5.7 - CROP_PRESET_3X ENGIO Override
**Status:** 🔲 Requires physical 70D  
**Issue:** Currently disabled ("fixme: corrupted image")  
**Test Method:** Enable override, capture frames, check for corruption

### S5.8 - ADTG Readout End
**Status:** 🔲 Requires physical 70D  
**Issue:** `shamem_read(0xC0F06804)` marked "fixme: D5 only"  
**Test Method:** Compare with 5D3, test alternative registers

### S5.9 - Final Validation
**Status:** 🔲 Requires physical 70D  
**Test Matrix:**
| Mode | Resolution | FPS | Status |
|------|-----------|-----|--------|
| 1080p | 1920x1080 | 30/25/24 | 🔲 |
| 3K | 3072x1536 | 30/25/24 | 🔲 |
| UHD | 3840x1920 | 30/25/24 | 🔲 |
| 4K | 4096x2160 | 24 | 🔲 |
| 3X_TALL | 1920x1080 (3X) | 30/25/24 | 🔲 |

---

## Next Steps

### Immediate (QEMU Testing)
1. ✅ Verify module loads without crash
2. ✅ Verify register addresses in code
3. ✅ Verify timer tables exist
4. ⏳ Test menu navigation (if QEMU display works)
5. ⏳ Test property access (PROP_LV_LENS, etc.)

### Hardware Testing (Requires Physical Camera)
1. Follow HARDWARE-TESTING.md checklist
2. Capture test frames for each crop mode
3. Calibrate CMOS/ENGIO registers
4. Enable and test CROP_PRESET_3X
5. Verify ADTG readout_end extraction
6. Document final register values

---

## Build Information

**Build Date:** 2026-04-27  
**autoexec.bin:** 451KB (462,592 bytes)  
**magiclantern.bin:** 448KB (458,944 bytes)  
**crop_rec.mo:** 32KB (32,968 bytes)  
**Build Command:** `cd platform/70D.112 && make clean && make -j$(nproc)`

**QEMU Test Command:**
```bash
./test_70d_qemu.sh --boot --no-build --timeout 40
```

---

## Conclusion

All software-level validation has passed. The crop_rec module is correctly configured for 70D with proper register addresses, timer tables, and crop presets. However, **full validation requires physical hardware** to:

1. Test actual crop frame capture
2. Calibrate CMOS/ENGIO registers
3. Verify image quality and stability
4. Enable and test 3X_TALL mode

See `HARDWARE-TESTING.md` for the complete hardware testing checklist.
