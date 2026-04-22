# Magic Lantern 70D - Development Roadmap & TODO

This document outlines the development sprints for implementing the future work identified for the Canon 70D port of Magic Lantern.

## Project Overview

**Target Camera:** Canon 70D (firmware 1.1.2, DIGIC V)  
**Base Repository:** https://github.com/peva3/magiclantern_70D  
**Forked From:** https://github.com/reticulatedpines/magiclantern_simplified  
**Developer Identity:** pmwoodward3@gmail.com / peva3  
**Current Phase:** Week 1 - Foundation Setup  
**Last Updated:** 2026-04-22

---

## Sprint 0 — Foundation & Setup (Week 1 - COMPLETED)

### Status: ✅ COMPLETED

- [x] **S0.1** Create GitHub repository and clone to /app/70d
- [x] **S0.2** Verify build system works end-to-end (build autoexec.bin for 70D) ✅
- [x] **S0.3** Document current deployed build state (what modules are included, known bugs)
- [x] **S0.4** Set up QEMU emulation layer for 70D (cloned qemu-eos)
- [x] **S0.5** Create firmware backup/recovery documentation (critical for bricking scenarios)
- [x] **S0.6** Create comprehensive documentation (AGENTS.md, FUTURE-WORK.md, TESTING_FRAMEWORK.md)
- [x] **S0.7** Update README.md with current status and progress tracking ✅

**Deliverables:**
- ✅ Working build environment with ARM toolchain installed
- ✅ Verified autoexec.bin (435KB) that builds successfully from source
- ✅ Complete documentation suite
- ✅ QEMU infrastructure cloned and ready

---

## Sprint 1 — Discovery & Safe Hooks (Weeks 2-3)

### Status: PLANNED

- [ ] **S1.1** Map `LV_FOCUS_DATA` equivalent memory locations
  - Use memory spy hooks during AF operations
  - Dump candidate addresses during focus peaking
  - Identify where focus data is stored (if at all)

- [ ] **S1.2** FPS register investigation
  - Document `FPS_REGISTER_A` (0xC0F06008) and `FPS_REGISTER_B` (0xC0F06014) behavior
  - Test read/write patterns with safe values
  - Identify banding source when using Timer A

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

- [ ] **S2.1** Implement LV_FOCUS_DATA memory spy
  - If memory location identified in S1.1, create property-like interface
  - Handle cases where data is not broadcast (polling fallback)

- [ ] **S2.2** Re-enable focus confirmation in Magic Zoom
  - Remove `#if !defined(CONFIG_70D)` guards in focus.c
  - Test focus bars with spy/proxy data
  - Tune sensitivity and update frequency

- [ ] **S2.3** Restore focus graph/misc task
  - Re-enable `focus_misc_task` for 70D
  - Verify focus_mag plotting works
  - Test with various lenses (wide, tele, macro)

- [ ] **S2.4** Fix focus stacking bug
  - Investigate "takes 1 behind and 1 before" issue
  - Address soft limit being reached quickly (lens.c line 677)
  - Test multi-shot stacking sequence

---

## Sprint 3 — FPS Override (Weeks 8-11)

### Status: NOT YET STARTED

- [ ] **S3.1** Timer A/B register deep dive
  - Identify why Timer B has "untraceable problems"
  - Document Timer A banding pattern (frequency, severity)
  - Map blanking registers that may affect banding

- [ ] **S3.2** Implement safe FPS override prototype
  - Start with 24fps (closest to native, lowest risk)
  - Test 30fps, then 60fps (if 70D supports 1080p60)
  - Add guard rails to prevent unsafe values

- [ ] **S3.3** Banding mitigation
  - Test `TG_FREQ_BASE` adjustments (currently 32000000)
  - Explore blanking period modifications
  - Implement fallback if banding cannot be eliminated

- [ ] **S3.4** User interface for FPS selection
  - Add menu entries for 24/30/60 fps
  - Display current FPS and warnings
  - Add "safe mode" fallback

---

## Sprint 4 — RAW Zebras & Exposure (Weeks 12-14)

### Status: NOT YET STARTED

- [ ] **S4.1** Investigate RAW slurp timing conflict
  - Document when EDMAC RAW slurp occurs vs LV rendering
  - Identify race condition causing QuickReview corruption
  - Test vsync-locked RAW capture

- [ ] **S4.2** Implement double-buffered RAW capture
  - Use existing double-buffer architecture from raw_vid module
  - Ensure RAW buffer is stable before zebra analysis
  - Add semaphore or lock to prevent concurrent access

- [ ] **S4.3** Re-enable RAW zebras feature
  - Remove `FEATURE_RAW_ZEBRAS` disable flag
  - Test under varied lighting (low, medium, high dynamic range)
  - Verify no QuickReview or LV corruption

- [ ] **S4.4** Calibrate zebra thresholds
  - Map RAW values to zebra patterns
  - Add user-adjustable thresholds
  - Test accuracy vs histogram data

---

## Sprint 5 — Crop Recording (Weeks 15-18)

### Status: NOT YET STARTED

- [ ] **S5.1** Map 70D CMOS/ADTG/ENGIO registers
  - Identify register addresses for crop window control
  - Document safe values for 1:1 binning mode
  - Test 3K and 4K crop presets

- [ ] **S5.2** Implement crop_rec presets for 70D
  - Add 1:1 mode (full sensor width, line-skipped)
  - Add 3K mode (~3072px width)
  - Add 4K UHD mode (4096x2160 or 3840x2160)
  - Add preset menu entries

- [ ] **S5.3** Test crop modes with mlv_lite/mlv_rec
  - Verify crop dimensions match expected values
  - Test frame rate stability at each crop
  - Check for overheating or buffer underruns

---

## Sprint 6 — Dual ISO Movie Mode (Weeks 19-22)

### Status: NOT YET STARTED

- [ ] **S6.1** Investigate dual ISO photo vs movie pipeline
  - Document how ISO switching works in photo mode (working)
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

### Status: NOT YET STARTED

- [ ] **S9.1** FlexInfo display fix
  - Investigate bottom bar flicker source
  - Implement coordinate remapping or refresh sync
  - Test with various menu configurations

- [ ] **S9.2** SD UHS tuning
  - Test intermediate frequencies (120MHz, 133MHz)
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
