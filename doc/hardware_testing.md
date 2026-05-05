# Hardware Testing Checklist - Canon 70D crop_rec

## Prerequisites
- [ ] Physical Canon 70D with firmware 1.1.2
- [ ] Magic Lantern autoexec.bin (S21 build, 452KB)
- [ ] SD card (UHS-I recommended, 32GB+)
- [ ] Battery fully charged (or AC adapter)
- [ ] Lens attached (any EF/EF-S)
- [ ] Backup of original firmware

## S5.5 - CMOS Register Calibration

**Goal:** Verify CMOS register values for crop modes

**Test Procedure:**
1. Boot ML on 70D
2. Enable crop_rec module
3. Test each crop preset:
   - [ ] 1080p (CROP_PRESET_1080)
   - [ ] 3K (CROP_PRESET_3K)
   - [ ] UHD (CROP_PRESET_UHD)
   - [ ] 4K (CROP_PRESET_4K)
   - [ ] 3X_TALL (CROP_PRESET_3X_TALL)

**Expected Results:**
- Clean image with no artifacts
- Correct aspect ratio
- No vertical banding
- Stable frame rate

**Calibration Values to Test:**
- CMOS[2]: 0x10E, 0x0BE, 0x08E, 0x07E, 0x09E (per preset)
- CMOS[6]: 0x170, 0x370 (highlight fix)
- CMOS[7]: Vertical windowing (copied from 5D3 CMOS[1])

## S5.6 - ENGIO Register Calibration

**Goal:** Verify ENGIO top-bar and end-column offsets

**Registers to Calibrate:**
- 0xC0F06800: Top-bar offsets (currently 0x1F0017, 0x1D0017 from 5D3)
- 0xC0F06804: End-column values (currently 0x1AA, 0x20A, 0x22A from 5D3)
- HEAD3/4 base: 0x2B4, 0x26D (from 5D3 60p)

**Test Procedure:**
1. Enable crop_rec in desired mode
2. Capture test frames
3. Inspect for:
   - [ ] Top black bar presence/size
   - [ ] Right edge corruption
   - [ ] Frame alignment

## S5.7 - CROP_PRESET_3X ENGIO Override

**Issue:** Currently commented out - "fixme: corrupted image"

**Test Procedure:**
1. Uncomment CROP_PRESET_3X ENGIO override in crop_rec.c
2. Enable 3X crop mode
3. Capture frames
4. Inspect for corruption patterns

**Expected:** Clean 3X crop without corruption

## S5.8 - ADTG readout_end Extraction

**Issue:** Current code uses `shamem_read(0xC0F06804) >> 16` marked "fixme: D5 only"

**Test Procedure:**
1. Check if DIGIC V (70D) uses different ADTG register
2. Compare with 5D3 readout_end values
3. Test alternative register addresses

## S5.9 - Final Hardware Validation

**Complete Test Matrix:**
| Crop Mode | Resolution | FPS | CMOS OK | ENGIO OK | Stable | Notes |
|-----------|-----------|-----|---------|----------|--------|-------|
| 1080p | 1920x1080 | 30/25/24 | [ ] | [ ] | [ ] | |
| 3K | 3072x1536 | 30/25/24 | [ ] | [ ] | [ ] | |
| UHD | 3840x1920 | 30/25/24 | [ ] | [ ] | [ ] | |
| 4K | 4096x2160 | 24 | [ ] | [ ] | [ ] | |
| 3X_TALL | 1920x1080 (3X) | 30/25/24 | [ ] | [ ] | [ ] | |

## Known Issues to Verify

- [ ] FPS override stability (Timer A-only via HiJello/FastTv)
- [ ] Vertical banding in any mode
- [ ] Hot pixels at high ISO
- [ ] Frame alignment issues
- [ ] Menu navigation stability

## Reporting Results

For each test, document:
1. Crop mode tested
2. Resolution and FPS
3. Visual artifacts (if any)
4. Frame stability
5. Recommended register values

---

**Next Steps After Hardware Testing:**
1. Update CMOS register values in crop_rec.c
2. Update ENGIO register values
3. Enable CROP_PRESET_3X ENGIO override
4. Fix ADTG readout_end for DIGIC V
5. Final build verification (size < 600KB)
6. Commit with calibration results
