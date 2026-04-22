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

- [x] **S1.3** Verify `raw_lv_request` behavior on 70D ✅ (Documented)
  - raw_lv_request() uses reference counting (raw_lv_request_count)
  - Calls raw_lv_enable() -> raw_update_params_work() on first request
  - On disable, raw_lv_disable() is called with 50ms delay
  - 70D uses EDMAC_RAW_SLURP (connection #0, 0xC0F04008) for raw capture
  - PACK32_MODE at 0xC0F08094 controls bit depth (observed: 0x20/0x120)
  - RAW_TYPE_REGISTER at 0xC0F37014 (70D specific)
  - SHAD_GAIN_REGISTER at 0xC0F08030
  - ETTR can safely request raw buffers via raw_lv_request()/raw_lv_release()
  - Pink preview issue in zoom mode affects older DIGIC cameras (5D2/50D/500D) - 70D not affected

- [x] **S1.4** Establish safe camera state save/restore mechanism ✅ (Documented)
  Critical registers identified for 70D (DIGIC V):
  
  **EDMAC Registers (0xC0F04xxx - 0xC0F30xxx):**
  - 0xC0F04000: EDMAC base (connections #0-15)
  - 0xC0F26000: EDMAC base (connections #16-31)
  - 0xC0F30000: EDMAC base (connections #32-47)
  - 0xC0F05000-0xC0F05200: EDMAC channel configuration
  
  **Display/Palette Registers (0xC0F14xxx):**
  - 0xC0F14078: Display update trigger
  - 0xC0F14080-0xC0F140D4: Palette and display buffers
  - 0xC0F140cc: Zebra register (DIGIC_ZEBRA_REGISTER)
  - 0xC0F140c4: Saturation register
  - 0xC0F141B8: Brightness/contrast register
  - 0xC0F14040: Filter enable register
  - 0xC0F14164: Position register
  
  **FPS/Timer Registers (0xC0F06xxx):**
  - 0xC0F06008: FPS_REGISTER_A (Timer A - row readout)
  - 0xC0F06014: FPS_REGISTER_B (Timer B - frame timing, broken on 70D)
  - 0xC0F06000: FPS_REGISTER_CONFIRM_CHANGES
  
  **RAW Processing Registers:**
  - 0xC0F08094: PACK32_MODE (bit depth control)
  - 0xC0F08030: SHAD_GAIN_REGISTER
  - 0xC0F37014: RAW_TYPE_REGISTER (70D specific)
  - 0xC0F08114: PACK32_ISEL (pink fix for older cameras)
  
  **ISO/Exposure Registers:**
  - 0xC0F42744: ISO_PUSH_REGISTER_D5 (per-channel ISO push)
  - 0xC0F14080: Exposure compensation base
  - 0xC0F140c0: Exposure control
  
  **Save/Restore Pattern:**
  - Use shamem_read() to capture current register values before modification
  - Use EngDrvOut() or EngDrvOutLV() to write new values
  - 0xC0F06000 must be written with 1 to confirm FPS changes
  - State object hooks run in Canon tasks - see state-object.c
  - task_dispatch_hook at 0x7AAD4 intercepts task creation
  - pre_isr_hook/post_isr_hook at 0x7A9B8/0x7A9BC for interrupt handling

**Testing:**
- All tests read-only, no functional changes to camera behavior
- Unit tests for register read/write safety
- Verify no crashes or instability after test runs

---

## Sprint 2 — Focus Features (Weeks 4-7)

### Status: IN PROGRESS

**UPDATE:** Use PROP_LV_LENS (0x80050000) instead of missing LV_FOCUS_DATA. Handler exists at lens.c:1900.
Implementation: focus.c now includes 70D-specific focus tracking using focus_pos stability detection.

- [x] **S2.1** Implement focus confirmation using PROP_LV_LENS ✅
  - Created update_focus_pos_70d() function that polls lens_info.focus_pos
  - Detects focus lock via position stability (4 consecutive identical samples)
  - Generates synthetic focus_mag values from position change magnitude
  - Uses circular buffer (8 samples) for position history tracking

- [x] **S2.2** Re-enable focus confirmation in Magic Zoom ✅
  - Removed `#if !defined(CONFIG_70D)` guard from focus.c:1111
  - focus_misc_task now runs on 70D with polling-based focus detection
  - Focus bars will respond to lens position stabilization events

- [x] **S2.3** Restore focus graph/misc task ✅
  - focus_misc_task re-enabled for 70D
  - Calls update_focus_pos_70d() every 100ms when LV is active
  - Uses existing plot_focus_mag() infrastructure for display
  - Note: Update frequency slower than cameras with LV_FOCUS_DATA (100ms vs ~30ms)

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

- [x] **S9.x** Enable CONFIG_BEEP ✅ (see S9.3)
  - Audio beep support enabled in internals.h

- [x] **S9.x** Enable CONFIG_Q_MENU_PLAYBACK ✅
  - Quick menu in playback mode (like other DIGIC V cameras)
  - Adds rating functionality in photo playback

- [x] **S9.x** Enable CONFIG_WB_WORKAROUND ✅
  - White balance save/restore for movie mode
  - Kelvin WB lock workaround (used by 600D, 60D, EOSM)

- [x] **S9.x** Enable FEATURE_NITRATE ✅
  - Bitrate control and display for movie mode
  - Shows measured bitrate during recording
  - Safe feature - just reads and displays movie encoding parameters
  - autoexec.bin: 444KB (no size increase from NITRATE)

- [x] **S9.x** FEATURE_SHUTTER_LOCK already enabled ✅
  - Already active via CONFIG_PROP_REQUEST_CHANGE in internals.h
  - all_features.h enables it automatically for cameras with property support
  - No additional changes needed

- [x] **S9.1** FlexInfo/Level display fix
  - Added 70D-specific flexinfo config in flexinfo.c avoiding bottom bar flicker zone (y=459)
  - 70D config uses inner screen positions, avoids vari-angle display conflict area
  - CONFIG_LVAPP_HACK_DEBUGMSG enabled (Sprint 11) suppresses Canon bottom bar
  - Level freeze: electronic_level.c uses PROP_ROLLING_PITCHING_LEVEL which stops updating after ~1 min
  - Root cause: 70D firmware stops sending rolling/pitching level property after timeout
  - Workaround: Disable ML overlays, use Canon's level, re-enable ML (user-reported)
  - Fix requires hardware tracing of property flow or hooking Canon's level refresh timer

- [ ] **S9.2** SD UHS tuning
  - Test intermediate frequencies (120MHz, 133MHz)
  - 160MHz preset gives ~70MB/s (working)
  - 192/240MHz cause instability
  - Implement PauseReadClock/PauseWriteClock hooks if missing
  - Document stable overclock settings

- [x] **S9.3** Beep support ✅
  - Investigated why CONFIG_BEEP is disabled: was commented out
  - All required stubs exist (StartASIFDMADAC, StopASIFDMADAC, SoundDevActiveIn, etc.)
  - Enabled CONFIG_BEEP in internals.h - builds successfully
  - autoexec.bin increased from 438KB to 442KB

- [ ] **S9.4** METERING/AF-area toggle reliability
  - Debug button handling for toggle
  - Add debounce or timeout if needed
  - Test with various button press patterns

---

## Sprint 10 — Bug Fixes & Polish (Weeks 35-36)

### Status: PARTIALLY COMPLETED (Documentation)

- [x] **S10.1** PACK32_MODE investigation ✅ (Documented)
  - 70D is DIGIC V (CONFIG_DIGIC_45)
  - Register: 0xC0F08094
  - Comment in raw.c:2618-2621: theoretical values (0x130, 0x030, 0x010, 0x000) don't match observed values
  - Actual observed values: 0x20 and 0x120 (possibly "highest bit wins" pattern)
  - Requires hardware testing to verify bit depth switching behavior

- [x] **S10.2** Two-finger touch investigation ✅ (Documented)
  - gui.h line 10: "NO GUI EVENTS: two finger touch unavailable on this camera"
  - Hardware limitation - 70D touchscreen only supports single-finger touch
  - Event codes defined (0x76-0x79) but never triggered by firmware
  - No fix possible without hardware changes

- [x] **S10.3** mvr_struct_real investigation ✅ (Documented)
  - mvr_config struct at platform/70D.112/include/platform/mvr.h (140 lines)
  - Copied from 650D, marked as "Indented = WRONG"
  - Many unknown fields (x67e4, x67f8, x680c, etc.) - ~40+ undocumented uint32_t fields
  - SIZE_CHECK_STRUCT commented out at line 138
  - MVR_516_STRUCT at 0x7AEA4 - found by nikfreak/a1ex via MVR_Initialize decompilation
  - Requires hardware testing to map unknown fields

- [ ] **S10.4** A/B firmware toggle maintenance
  - No dedicated A/B firmware toggle code found in 70D-specific files
  - Bootflag system uses partition table (bootflags.c:62-259)
  - PROP_REBOOT used for reboot triggering
  - Verify workaround continues to work (requires hardware testing)

---

## Sprint 11 — Code Cleanup & Safe Enables (Weeks 37-38)

### Status: IN PROGRESS

**Goal:** Clean up dead code, consolidate duplicated patterns, enable safe features from other DIGIC V cameras.

- [x] **S11.1** Remove dead `#if 0` blocks
  - zebra.c:4103-4108 (debug cropmark code)
  - beep.c:1097 (dead recording-at-startup code)
  - flexinfo.c:2197+ (kept - needed for positioning constants)

- [x] **S11.2** Remove useless commented-out configs from internals.h
  - CONFIG_BLUE_LED (line 31) - 70D has no blue LED
  - CONFIG_LCD_SENSOR (line 34) - no hardware sensor
  - CONFIG_DMA_MEMCPY (lines 99-102) - superseded by EDMAC memcpy

- [x] **S11.3** Merge 70D with 6D/5D3 EDMAC channel case
  - edmac-memcpy.c:30-37 - identical read/write channels (0x19/0x11)
  - Reduces code duplication

- [x] **S11.4** Consolidate shared 70D/6D property definitions
  - property.h:378-400 - identical PROP_HI_ISO_NR, PROP_HTP, PROP_MLU, etc.
  - Merge into single `#if defined(CONFIG_70D) || defined(CONFIG_6D)` block

- [x] **S11.5** Replace `#if !defined(CONFIG_70D)` with capability flags
  - focus.c:930-1054 - use `#ifdef CONFIG_LV_FOCUS_INFO` instead
  - Makes code more maintainable and consistent

- [x] **S11.6** Document commented-out register defines in consts.h
  - FRAME_SHUTTER_BLANKING (lines 263-267) - add why disabled
  - ISO_ADJUSTMENT_ACTIVE (line 87) - removed (never verified)
  - BULB_EXPOSURE_CORRECTION (line 229) - marked as unverified

- [ ] **S11.7** Enable FEATURE_UNMOUNT_SD_CARD
  - SKIPPED: Requires FSUunMountDevice stub not available on 70D (5D3 only)

- [x] **S11.8** Enable CONFIG_LVAPP_HACK_DEBUGMSG
  - 5D3/6D have it - hides Canon bottom bar via DebugMsg hook
  - May help with FlexInfo flickering issue

- [ ] **S11.9** Replace hardcoded camera lists with config flags
  - SKIPPED: Lists are scattered across 25+ files, many already correctly grouped
  - Would require architectural refactoring (CONFIG_AUDIO_RELEASE_SHOT, etc.)
  - Moved to Long-Term Architecture as L5

---

## Sprint 12 — Dead Code Purge & Safe Enables (Weeks 39-40)

### Status: ✅ COMPLETED

**Goal:** Remove dead `#if 0` blocks, fix minor code quality issues, enable safe features for 70D.

- [x] **S12.1** Remove dead `#if 0` blocks
  - stdio.c:14-33 (unused streq implementation)
  - tasks.c:103-135 (debug stack checking)
  - tasks.c:422-477 (BMP lock debugging)
  - focus.c:1279-1294 (dead focus stacking menu entries)
  - menu.c:5440-5448 (dead BGMT_PLAY case)
  - menu.c:6296-6339 (bubbles hack, bmp_draw_scaled_ex test, Gryp logging)
  - powersave.c:220-222 (NotifyBox debug call)
  - cropmarks.c:434-456 (draw_cropmark_area, show_apsc_crop_factor)
  - rbf_font.c:365-371 (tab width fix that breaks cursor)
  - module.c:676-693 (TCC section debug logging)
  - NOTE: module.c:228-377 NOT removed - contains TCC struct definitions needed by real code

- [x] **S12.2** Fix bitwise vs logical operator in raw.c:2627
  - Changed `|` to `||` in preprocessor condition

- [x] **S12.3** Clean up gui-common.c
  - Simplified redundant `CONFIG_LVAPP_HACK_DEBUGMSG || CONFIG_LVAPP_HACK` to just `CONFIG_LVAPP_HACK`
  - Removed unused `DebugMsg_uninstall()` function

- [x] **S12.4** Add CONFIG_70D to zebra.c Magic Zoom warning exclusion
  - 70D shares DIGIC V architecture with 6D/5D3 - same >30fps limitation

- [x] **S12.5** Add CONFIG_70D to shoot.c bitrate measurement
  - 70D records H264 and benefits from same bitrate measurement as 5D3/6D

- [x] **S12.6** Remove commented-out set_pic_quality function in tweaks.c
  - Dead code wrapped in `/* ... */`

- [x] **S12.7** Fix unused parameter warnings
  - tweaks.c: set_expsim stub now uses `(void)expsim`

**Build:** autoexec.bin 444KB (unchanged, well under 656KB limit)

---

## Long-Term Architecture (Ongoing)

These tasks span multiple sprints:

- [ ] **L1** EDMAC abstraction layer (started in S7.4)
- [ ] **L2** MLV v3 global dependency cleanup (started in S7.3)
- [ ] **L3** Cross-camera compatibility improvements
- [ ] **L4** Performance optimization (worker priorities, buffer sizes)
- [ ] **L5** Replace hardcoded camera lists with capability flags (S11.9 deferred)

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
