# Magic Lantern 70D: Future Work & Improvement Opportunities

Based on the deep dive into the Magic Lantern simplified codebase for the Canon 70D, here is a consolidated list of identified bugs, missing features, and areas for potential engineering improvement.

## High-Priority Fixes (Broken Core Features)

### 1. LiveView Focus Data (`LV_FOCUS_DATA`) Emulation or Discovery
- **The Issue:** The 70D firmware does not expose focus distance/data properties during LiveView (`internals.h`).
- **Impact:** Focus confirmation in Magic Zoom is broken. Focus graphing is disabled. Focus stacking is buggy ("takes 1 behind and 1 before, all others are before").
- **Future Work:** Reverse engineer the DIGIC V memory space during AF operations to find where focus peaking data or lens step counts are temporarily stored. If Canon's property system doesn't broadcast it, ML needs a direct memory spy hook.
- **Code Reference:** `focus.c` line 926 explicitly notes "70D unfortunately has no LV_FOCUS_DATA property". The entire focus_misc task (lines 930-1054) is wrapped in `#if !defined(CONFIG_70D)`.

### 2. FPS Override Resolution (`FEATURE_FPS_OVERRIDE`)
- **The Issue:** Modifying the camera's frame rate is disabled for the 70D.
- **Impact:** Cannot shoot undercranked/overcranked video or do advanced timelapses.
- **Future Work:** The developer notes state "Timer B has untraceable problems" and "Timer A causes banding". This requires deep register-level debugging of the `FPS_REGISTER_A` (`0xC0F06008`) and Engio timing systems. It may require adjusting the `TG_FREQ_BASE` (currently `32000000` on 70D) or finding the blanking registers.
- **Code Reference:** `features.h` - "Really, this simply doesn't work. Tried it for a felt hundred hours. TIMER_B has untraceable problems. Using TIMER_A_ONLY causes banding/patterns."

### 3. RAW Zebras / Overexposure Overlays
- **The Issue:** `FEATURE_RAW_ZEBRAS` is disabled because it causes visual corruption in QuickReview and LiveView.
- **Impact:** Professional exposure monitoring is limited to processed YUV zebras, not true sensor RAW clipping.
- **Future Work:** Investigate the EDMAC RAW slurp timing. The visual corruption likely stems from ML reading the RAW buffer concurrently while Canon's image processor is rendering the LV frame. Needs a safer `vsync` lock or double-buffering approach.
- **Code Reference:** `zebra.c` has 70D-specific comment about RAW zebras being broken.

## Module Enhancements

### 4. Advanced Crop Recording (`crop_rec` module)
- **The Issue:** The 70D currently only supports the basic `3x3_1X` preset.
- **Future Work:** To support 1:1, 3K, 4K, or UHD crop modes, the exact CMOS, ADTG, and ENGIO register addresses for the 70D sensor need to be mapped. (Registers like `0xC0F04008`).
- **Code Reference:** Falls into "is_basic" category with only 3x3_1X preset. Needs CMOS/ADTG/ENGIO register addresses.

### 5. Dual ISO Movie Mode
- **The Issue:** `dual_iso.c` notes that movie mode is broken on the 70D (though photo mode works).
- **Future Work:** Needs investigation into how the 70D handles ISO switching per-scanline in H.264 vs RAW video. The ADTG register injection might be happening too late in the VSYNC cycle.
- **70D Specifics:**
  - `PHOTO_CMOS_ISO_START = 0x404e5664`
  - COUNT = 7, SIZE = 20, CMOS_ISO_BITS = 3, CMOS_FLAG_BITS = 2
  - CMOS_EXPECTED_FLAG = 3

### 6. MLV v3.0 Architecture Porting
- **The Issue:** The newer, cleaner `raw_vidx` module (which uses the superior MLV v3.0 library and double-buffering producer-consumer pattern) is not enabled/configured for the 70D.
- **Future Work:**
  1. Map exact crop dimensions for the 70D into `raw_vidx` (currently hardcoded to MLV_3_CROP_WIDTH/HEIGHT = 1792x896).
  2. Tune the worker priority thread values (`WORKER_PRIO=0x11`, `WORKER_WRITE_PRIO=0x9`) which were optimized for the 200D.
  3. Refactor the MLV v3 library to eliminate its reliance on global variables (`raw_info`, `lens_info`, `camera_model`), making it truly cross-camera safe.
  4. Address FIXME: "should stop using edmac_copy() directly since not all cams have it"

### 7. ETTR (Auto Exposure to the Right)
- **The Issue:** May not work in LiveView if the `raw_lv_request` stub returns 0 on 70D.
- **Future Work:** Verify that `raw_lv_request` works properly on 70D for ettr auto-exposure calculations.

## Quality of Life / UI Improvements

### 8. Audio Controls
- **The Issue:** `CONFIG_AUDIO_CONTROLS` is disabled. ML cannot set digital/analog gain overrides on the 70D.
- **Future Work:** Reverse engineer the audio IC via the `sounddev_task` or ASIF DMA.

### 9. FlexInfo Display Fixes
- **The Issue:** The bottom info bar (time, battery) flickers heavily, causing `FEATURE_FLEXINFO` to be disabled.
- **Future Work:** The GUI rendering loop is fighting Canon's bottom bar redraw. The workaround `UNAVI_BASE` (`0x9FC74`) is a hack. Needs alternative coordinate mapping or a reliable way to suspend Canon's UI timer (`CancelUnaviFeedBackTimer` is missing on 70D).

### 10. SD Card Overclocking Stability (`sd_uhs` module)
- **The Issue:** The `160MHz1` aggressive overclock fails immediately on 70D.
- **Future Work:** Determine if this is a hard hardware limitation of the 70D's SD controller, or if slightly tuning the `sd_write_clock` / `sd_read_clock` registers (`0xff7d18a0`, `0xff7d1e68`) could yield a stable middle-ground frequency (e.g., 120MHz or 133MHz).
- **Missing Hooks:** The 70D likely needs `PauseReadClock` and `PauseWriteClock` hooks implemented.

### 11. Beep Support
- **The Issue:** `CONFIG_BEEP` is disabled on 70D.
- **Future Work:** Investigate why beep doesn't work and re-enable if possible.

### 12. METERING/AF-Area Button Toggle
- **The Issue:** The button toggle for switching between metering and AF-area modes is commented out as "unreliable".
- **Future Work:** Debug the button handling to make it reliable.

## Additional Known Bugs / TODOs

### 13. PACK32_MODE Values Not Understood
- **The Issue:** Comment in `raw.c` (SJE): "I don't see these values getting written on 70D"
- **Values:** 0x20 and 0x120 mentioned but not verified working.

### 14. Focus Soft Limit Reached Quickly
- **Issue:** `lens.c` line 677: "70D focus features don't play well with this and soft limit is reached very quickly"
- **Impact:** Limits the usefulness of focus stacking and rack focus features.

### 15. Focus Stacking Bug
- **Issue:** "takes 1 behind and 1 before all others afterwards are before at the same position no matter what's set in menu"
- **Impact:** Focus stacking produces incorrect results.

### 16. Two-Finger Touch Unavailable
- **Issue:** Touch events for two-finger gestures are defined in `gui.h` but noted "unavailable on this camera"
- **Future Work:** Determine if this is a hardware or software limitation.

## Long-Term Architecture Improvements

### 17. Refactor Global Dependencies in MLV Library
- The MLV v3 library (`mlv_3.c`) has many FIXMEs about removing global variable dependencies:
  - `raw_info` global (should be passed in)
  - `lens_info` global
  - `camera_model` global
  - `raw_capture_info` global
- These global dependencies make the library difficult to port to new cameras.

### 18. EDMAC Abstraction Layer
- **Issue:** `edmac_copy_rectangle_cbr_start` is used directly but FIXME notes say "not all cameras have it"
- **Future Work:** Create an abstraction layer to hide camera-specific EDMAC differences.

### 19. mvr_struct_real Investigation
- **Issue:** The 70D-specific movie recorder struct (`mvr.h`) has `uint32_t unk[0x192]` - mostly unknown data.
- **Future Work:** Reverse engineer the unknown fields to enable better movie mode features.

### 20. A/B Firmware Toggle Maintenance
- **Issue:** `reboot.c` contains a workaround for 70D dual firmware partitions.
- **Future Work:** Ensure this continues to work as firmware updates are released.

## Summary Table

| Issue | Severity | Status | Effort |
|-------|----------|--------|--------|
| LV_FOCUS_DATA missing | High | Known limitation | High |
| FPS Override broken | High | Known limitation | Very High |
| RAW Zebras broken | High | Disabled | Medium |
| crop_rec advanced modes | Medium | Not implemented | High |
| Dual ISO movie | Medium | Broken | High |
| MLV v3 port | Medium | Not started | Medium |
| ETTR in LV | Medium | Unknown | Low |
| Audio Controls | Medium | Disabled | High |
| FlexInfo flickering | Low | Disabled | Medium |
| SD overclock | Low | Partial | Low |
| Beep support | Low | Disabled | Low |
| Focus stacking bug | Medium | Known bug | High |
