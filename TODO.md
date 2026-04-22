# Magic Lantern 70D - Development Roadmap & TODO

This document outlines the development sprints for implementing the future work identified for the Canon 70D port of Magic Lantern.

## Project Overview

**Target Camera:** Canon 70D (firmware 1.1.2, DIGIC V)  
**Base Repository:** https://github.com/peva3/magiclantern_70D  
**Forked From:** https://github.com/reticulatedpines/magiclantern_simplified  
**Developer Identity:** pmwoodward3@gmail.com / peva3  
**Current Phase:** Week 1 - Foundation Setup  
**Last Updated:** 2026-04-22

### Key Contributors (from forum research)
- **nikfreak:** Primary 70D port developer
- **David_Hugh:** Found FPS Timer A workaround (HiJello-FastTv)
- **ArcziPL:** crop_rec_4k with 14-bit lossless
- **theBilalFakhouri:** sd_uhs module enhancements
- **a1ex:** Main ML developer, fps-engio and lossless support

---

## Sprint 0 — Foundation & Setup (Week 1 - COMPLETED)

### Status: ✅ COMPLETED

- [x] **S0.1** Create GitHub repository and clone to /app/70d ✅
- [x] **S0.2** Verify build system works end-to-end (build autoexec.bin for 70D) ✅
  - **BUILD SUCCESS:** 435KB autoexec.bin produced
  - Location: `platform/70D.112/build/autoexec.bin`
  - Version string: `2026-04-22.70D.112`
- [x] **S0.3** Document current deployed build state ✅
- [x] **S0.4** Set up QEMU emulation layer for 70D ✅
  - qemu-eos cloned to `/app/70d/qemu-eos`
  - Note: 70D not yet supported in QEMU (will need adaptation)
- [x] **S0.5** Create firmware backup/recovery documentation ✅
- [x] **S0.6** Create comprehensive documentation (AGENTS.md, FUTURE-WORK.md, TESTING_FRAMEWORK.md) ✅
- [x] **S0.7** Update README.md with current status and progress tracking ✅
- [x] **S0.8** Create host-side test framework ✅
  - tests/ directory with mock stubs
  - test_focus.c runs successfully
  - Validates CONFIG_70D detection

**Deliverables:**
- ✅ Working build environment with ARM toolchain installed
- ✅ Verified autoexec.bin (435KB) that builds successfully from source
- ✅ Complete documentation suite
- ✅ QEMU infrastructure cloned and ready
- ✅ Host-side test framework operational

### Confirmed Working Features (from forum)

**Working (do not break):**
- ✅ Zebras (over/under) in photo mode
- ✅ Focus Peak in photo mode (greyscale)
- ✅ Crop Marks in photo and play modes
- ✅ Ghost Image, Spotmeter, False Color
- ✅ Waveform (sometimes flickers), Vector scope
- ✅ Histogram (sometimes freezes - reboot helps)
- ✅ Audio meters
- ✅ RAW video (works with limitations - hot pixels at ISO 1600+)
- ✅ Dual ISO (photo mode)
- ✅ ETTR
- ✅ Crop_rec (3x zoom) in photo mode

**Known Issues (user-reported):**
- 🔶 Level indicator freezes after ~1 min in LV
- 🔶 Histogram sometimes freezes
- 🔶 ML menu flickers in LiveView/movie mode
- 🔶 Shutter speed sometimes ignored (only decreasing works)
- 🔶 FPS sometimes changes 23.97 → 23.98 randomly

---

## Sprint 1 — Discovery & Safe Hooks (Weeks 2-3)

### Status: PLANNED

**UPDATE:** 70D DOES have PROP_LV_LENS (0x80050000) with focus_pos data - use this as alternative to LV_FOCUS_DATA.

- [ ] **S1.1** Verify PROP_LV_LENS focus_pos data quality
  - Test update frequency during AF operations
  - Compare against lens encoder positions
  - Determine suitability for focus confirmation UI

- [ ] **S1.2** FPS register investigation (UPDATED)
  - David_Hugh found workaround: Timer A only via "HiJello-FastTv"
  - FPS_REGISTER_B (0xC0F06014) works differently on 70D
  - Test Timer A-only approach for stability
  - Document banding patterns and mitigations

- [ ] **S1.3** Verify `raw_lv_request` behavior on 70D
  - Test if ETTR can safely request raw buffers in LiveView
  - Document return values and failure modes

- [ ] **S1.4** Establish safe camera state save/restore mechanism
  - Identify critical registers that must be preserved
  - Create rollback hooks for semi-invasive patches

**Testing:**
- All tests read-only, no functional changes to camera behavior
- Unit tests for register read/write safety
- Verify no crashes or instability after test runs

---

## Sprint 2 — Focus Features (Weeks 4-7)

### Status: NOT YET STARTED

**UPDATE:** Use PROP_LV_LENS (0x80050000) instead of missing LV_FOCUS_DATA. Handler exists at lens.c:1900.

- [ ] **S2.1** Implement focus confirmation using PROP_LV_LENS
  - Create property-like interface using existing lens.c:1900 handler
  - Extract focus_pos from prop_lv_lens struct (lens.h:117, offset 0x23)
  - Handle polling fallback (LV_LENS updates slower than LV_FOCUS_DATA)

- [ ] **S2.2** Re-enable focus confirmation in Magic Zoom
  - Remove `#if !defined(CONFIG_70D)` guards in focus.c
  - Test focus bars with PROP_LV_LENS data
  - Tune sensitivity for slower update frequency

- [ ] **S2.3** Restore focus graph/misc task
  - Re-enable `focus_misc_task` for 70D
  - Use PROP_LV_LENS instead of PROP_LV_FOCUS_DATA
  - Test with various lenses (wide, tele, macro)

- [ ] **S2.4** Fix focus stacking bug (LOW PRIORITY)
  - Investigate "takes 1 behind and 1 before" issue
  - Address soft limit being reached quickly (lens.c line 677)
  - Test multi-shot stacking sequence

---

## Sprint 3 — FPS Override (Weeks 8-11)

### Status: NOT YET STARTED

**UPDATE:** David_Hugh found experimental workaround using Timer A only (HiJello-FastTv). FPS_REGISTER_B works differently on 70D.

- [ ] **S3.1** Test Timer A-only workaround
  - Verify HiJello-FastTv setting stability
  - Test 24fps, 30fps, 60fps baselines
  - Document any remaining banding issues

- [ ] **S3.2** Explore Timer A+B hybrid approach
  - Investigate why Timer B has "untraceable problems"
  - Test read-modify-write patterns carefully
  - Map blanking registers that may affect banding

- [ ] **S3.3** Banding mitigation
  - Test `TG_FREQ_BASE` adjustments (currently 32000000)
  - Explore blanking period modifications
  - Implement fallback if banding cannot be eliminated

- [ ] **S3.4** User interface for FPS selection
  - Add menu entries for 24/30/60 fps
  - Display current FPS and warnings
  - Add "safe mode" fallback to Timer A only

---

## Sprint 4 — RAW Zebras & Exposure (Weeks 12-14)

### Status: ✅ COMPLETED

**UPDATE:** ✅ DONE - CONFIG_NO_RAW_ZEBRAS added to internals.h at line 163. zebra.c updated to use proper config flag at line 4119. This documents the limitation cleanly for maintenance.

- [x] **S4.1** Add CONFIG_NO_RAW_ZEBRAS to internals.h ✅
  - Replace scattered `#if !defined(CONFIG_70D)` with proper config flag
  - This documents the limitation cleanly for maintenance

- [ ] **S4.2** Investigate RAW slurp timing conflict (if needed after S4.1)
  - Document when EDMAC RAW slurp occurs vs LV rendering
  - Identify race condition causing QuickReview corruption
  - Test vsync-locked RAW capture

- [ ] **S4.3** Implement double-buffered RAW capture (if needed)
  - Use existing double-buffer architecture from raw_vid module
  - Ensure RAW buffer is stable before zebra analysis
  - Add semaphore or lock to prevent concurrent access

- [ ] **S4.4** Test RAW zebras re-enablement
  - Remove zebra.c:4121 guard
  - Test under varied lighting (low, medium, high dynamic range)
  - Verify no QuickReview or LV corruption

---

## Sprint 5 — Crop Recording (Weeks 15-18)

### Status: NOT YET STARTED

**UPDATE:** User-reported working (ArcziPL crop_rec_4k with 14-bit lossless). Hot pixels at ISO 1600+ in 3X crop.

- [ ] **S5.1** Map 70D CMOS/ADTG/ENGIO registers
  - Identify register addresses for crop window control
  - Document safe values for 1:1 binning mode
  - Test 3K and 4K crop presets

- [ ] **S5.2** Implement crop_rec presets for 70D
  - ArcziPL developed crop_rec_4k variants with 14-bit lossless
  - Add 1:1 mode (full sensor width, line-skipped)
  - Add 3K mode (~3072px width) - reported working
  - Add 4K UHD mode (4096x2160 or 3840x2160)
  - Add preset menu entries

- [ ] **S5.3** Test crop modes with mlv_lite/mlv_rec
  - Verify crop dimensions match expected values
  - Test frame rate stability at each crop
  - Check for overheating or buffer underruns
  - Address hot pixel issue at ISO 1600+ in 3X crop mode

---

## Sprint 6 — Dual ISO Movie Mode (Weeks 19-22)

### Status: NOT YET STARTED

**UPDATE:** Photo mode works. Movie mode initially broken, later fixes attempted.

- [ ] **S6.1** Investigate dual ISO photo vs movie pipeline
  - Photo mode confirmed working by users
  - Identify why movie mode fails (ADTG injection timing?)
  - Compare VSYNC cycle timing between modes

- [ ] **S6.2** Implement movie mode ISO switching
  - Adjust ADTG register injection timing for movie pipeline
  - Test with various frame rates (24/30/60 fps)
  - Add per-scanline ISO switching verification

- [ ] **S6.3** Dual ISO calibration for video
  - Create calibration routine for video-specific ISO pairs
  - Document optimal ISO pairs for video (e.g., 100/800, 200/1600)
  - Add menu entries for dual ISO video

---

## Sprint 7 — MLV v3 Port (Weeks 23-28)

### Status: NOT YET STARTED

- [ ] **S7.1** Map 70D crop dimensions for raw_vidx
  - Determine safe crop window for 70D sensor
  - Define MLV_3_CROP_WIDTH/HEIGHT for 70D
  - Test crop buffer allocation and stability

- [ ] **S7.2** Enable raw_vidx module for 70D
  - Add 70D to modules.included
  - Configure worker priorities (currently tuned for 200D)
  - Test producer-consumer pipeline

- [ ] **S7.3** Refactor MLV v3 global dependencies
  - Remove `raw_info` global (pass as parameter)
  - Remove `lens_info` global
  - Remove `camera_model` global
  - Make library truly session-based

- [ ] **S7.4** Replace direct edmac_copy usage
  - Implement abstraction layer for edmac_copy
  - Handle cameras without edmac_copy gracefully
  - Add alignment requirement handling

---

## Sprint 8 — Audio Controls (Weeks 29-31)

### Status: NOT YET STARTED

- [ ] **S8.1** Reverse engineer audio IC registers
  - Map ASIF DMA registers for 70D
  - Identify digital gain, analog gain, mic select registers
  - Document safe value ranges

- [ ] **S8.2** Implement audio property handlers
  - Create PROP handlers for audio settings
  - Add menu interface for gain control
  - Implement remote audio shot support

- [ ] **S8.3** Test audio quality and stability
  - Record with various gain settings
  - Test for noise floor and distortion
  - Verify no interference with video recording

---

## Sprint 9 — Quality of Life Improvements (Weeks 32-34)

### Status: IN PROGRESS

**UPDATE:** Level indicator freezes after ~1 min (workaround: press INFO). SD UHS ~70MB/s max at 160MHz.

- [x] **S9.x** Enable CONFIG_ZOOM_HALFSHUTTER_UILOCK ✅
  - Enables UI lock during zoom operations with half-shutter pressed
  - Prevents UI glitches when zooming while holding shutter
  - Already enabled in 5D3 - verified working pattern

- [ ] **S9.1** FlexInfo/Level display fix
  - Investigate bottom bar flicker source
  - Level freezes after ~1-2 min in LV
  - Workaround: Disable ML overlays, use Canon's level, re-enable ML
  - Implement coordinate remapping or refresh sync

- [ ] **S9.2** SD UHS tuning
  - Test intermediate frequencies (120MHz, 133MHz)
  - 160MHz preset gives ~70MB/s (working)
  - 192/240MHz cause instability
  - Implement PauseReadClock/PauseWriteClock hooks if missing
  - Document stable overclock settings

- [ ] **S9.3** Beep support
  - Investigate why CONFIG_BEEP is disabled
  - Implement beep handler if hardware supports
  - Add menu toggle

- [ ] **S9.4** METERING/AF-area toggle reliability
  - Debug button handling for toggle
  - Add debounce or timeout if needed
  - Test with various button press patterns

---

## Sprint 10 — Bug Fixes & Polish (Weeks 35-36)

### Status: NOT YET STARTED

- [ ] **S10.1** PACK32_MODE investigation
  - Document values 0x20 and 0x120 behavior
  - Verify if 70D uses different packing

- [ ] **S10.2** Two-finger touch investigation
  - Determine if unavailable is hardware or software limitation
  - Document findings

- [ ] **S10.3** mvr_struct_real investigation
  - Document unknown fields in `uint32_t unk[0x192]`
  - Enable additional movie mode features if discovered

- [ ] **S10.4** A/B firmware toggle maintenance
  - Verify workaround continues to work
  - Test firmware update scenarios

---

## Long-Term Architecture (Ongoing)

These tasks span multiple sprints:

- [ ] **L1** EDMAC abstraction layer (started in S7.4)
- [ ] **L2** MLV v3 global dependency cleanup (started in S7.3)
- [ ] **L3** Cross-camera compatibility improvements
- [ ] **L4** Performance optimization (worker priorities, buffer sizes)

---

## Testing Infrastructure

### Required for all sprints:
- [ ] On-camera unit tests (automated where possible)
- [ ] Regression test suite (existing features remain stable)
- [ ] Field testing protocol (varied lighting, duration, lenses)
- [ ] Backup/recovery procedure documented and tested

### Safety principles:
1. Read-only probes before any write operations
2. Rollback mechanism for all patches
3. Firmware backup before any on-camera testing
4. Gradual rollout: emulator → lab → field → production

---

## Success Metrics

### Week 1 Goals (COMPLETED)
- ✅ Can build autoexec.bin from source
- ✅ QEMU boots 70D config (even if minimal)
- ✅ At least one host-side test running

### Future Milestones
- Month 1: One high-priority feature working in QEMU (focus or FPS)
- Month 3: Remote testing protocol established (if possible)
- Month 6: First major feature stable (raw zebras or crop modes)

---

## Risks & Mitigation

| Risk | Mitigation |
|------|------------|
| Cannot reproduce reference build | Compare commit hashes, check for local patches |
| QEMU doesn't support 70D | Focus on host-side testing, prioritize finding remote testers |
| Cannot test on real hardware | Maximize host-side test coverage, detailed test plans for remote testers |

---

## Resources Needed

### Already Available
- ✅ Source code repository
- ✅ Reference build (autoexec.bin, 70d-dev/)
- ✅ Documentation
- ✅ Testing framework

### Need to Acquire/Setup
- 🔲 ARM toolchain installation ✅ (DONE in Week 1)
- 🔲 QEMU build environment ✅ (DONE in Week 1)
- 🔲 Remote testing partnerships (optional but helpful)

---

## Contact Points

For collaboration or testing partnerships:
- GitHub: https://github.com/peva3/magiclantern_70D
- Reference: magiclantern_simplified upstream

---

## Summary

**Current Phase:** Week 1 - Foundation Setup  
**Next Milestone:** Build autoexec.bin from source ✅  
**Timeline:** 1-2 weeks to foundation, 2-3 months for first major features  
**Confidence:** High (strong documentation, reference build available, clear roadmap)

The path forward is clear: establish the development environment, verify we can build and emulate, then systematically tackle features starting with focus data discovery.
