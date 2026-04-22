# Magic Lantern 70D: Future Work & Improvement Opportunities

Based on the deep dive into the Magic Lantern simplified codebase for the Canon 70D, here is a consolidated list of identified bugs, missing features, and areas for potential engineering improvement.

## High-Priority Fixes (Broken Core Features)

### 1. LiveView Focus Data (`LV_FOCUS_DATA`) Emulation or Discovery
- **The Issue:** The 70D firmware does not expose focus distance/data properties during LiveView (`internals.h`).
- **Impact:** Focus confirmation in Magic Zoom is broken. Focus graphing is disabled. Focus stacking is buggy ("takes 1 behind and 1 before, all others are before").
- **Status:** Partially fixed by enabling `CONFIG_LV_FOCUS_INFO` and using `PROP_LV_LENS` focus_pos with stability detection (see `update_focus_pos_70d` in `focus.c`). Focus confirmation menu now available, but may have limited accuracy due to slower update rate of lens position data.
- **Future Work:** Fine-tune stability detection thresholds, improve focus graph display, and investigate alternative sources of focus data (e.g., direct memory hooks).
- **Code Reference:** `focus.c` line 926 explicitly notes "70D unfortunately has no LV_FOCUS_DATA property". The 70D-specific focus tracking uses `lens_info.focus_pos` (from `PROP_LV_LENS`) with sliding-window stability detection.

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

## Brand new development

### 1) WiFi tethering / remote control module
- **Feasibility:** The 70D has built-in WiFi hardware; firmware includes `LiveViewWifiApp_handler` (stub at `0xFF7523B4`). Magic Lantern already has a DryOS socket API surface (`ml_socket.h`) and a working WiFi example (`yolo.c` module). However, 70D `stubs.S` currently lacks concrete networking hooks (`socket_create`, `wlan_connect`, `NwLimeInit`, etc.). Observation of related code on 200D (DIGIC 8) provides a blueprint for porting WiFi control to DIGIC V.

- **What it enables:** Remote trigger/shooting, live image transfer, remote UI control from a phone/tablet, timecode/data exchange for synchronized shoots, and potential live view streaming (low-resolution).

- **Required work:**
  1. **Reverse-engineer 70D DryOS networking entry points:** Find firmware addresses for:
     - Socket API: `socket_create`, `socket_bind`, `socket_connect`, `socket_send`, `socket_recv`, `socket_close_caller`
     - WiFi management: `wlan_connect`, `nif_setup`, `set_IP_address`
     - Canon WiFi initialization: `NwLimeInit`, `NwLimeOn`, `wlanpoweron`, `wlanup`, `wlanchk`, `wlanipset`
  2. **Add stubs to `platform/70D.112/stubs.S`** using `NSTUB` macros.
  3. **Test basic connectivity** using the existing `yolo` module (with modified config) to verify socket creation and WiFi association.
  4. **Develop a minimal remote control module (`ml_tether`)** providing:
     - Remote trigger (shutter)
     - Start/stop recording
     - Exposure setting changes (ISO, shutter, aperture)
     - Live view stream (low-resolution YUV)
  5. **Integrate with ML menu** for configuration (SSID, password, IP settings).

- **Technical findings:**
  - `ml_socket.h` defines socket API and `wlan_settings` struct (size `0xFC`).
  - `yolo.c` demonstrates the full WiFi sequence: Lime core init → WiFi power on → connect → IP setup → socket communication.
  - The `call()` function (variadic) is used to invoke firmware functions by name; requires a symbol table lookup (likely `calls_table`).
  - 70D currently has only one WiFi-related stub: `LiveViewWifiApp_handler`. All other networking stubs are missing.

- **Risks/notes:** Hardware-dependent; needs hardware verification; ensure not to destabilize LiveView streaming; may require reverse engineering of firmware symbols; power consumption considerations.

- **References:** `ml_socket.h` contents, `yolo.c` networking usage, 200D `stubs.S` networking section, forum threads on WiFi tethering.

### 2) Cinematographer Mode port
- Concept: Port Cinematographer mode (focus sequence capture and replay) from ML to 70D. This module records a sequence of lens focus points with transition speeds and replays them during filming.
- Why now: We already have a working lens focus data path from PROP_LV_LENS and an initial update_focus_pos_70d() with a sliding-window stability approach (Sprint 2). The 70D touchscreen and vari-angle screen provide ideal UX.
- What it does: lets a single operator rehearse a shot by recording focal positions and speeds, then replay the sequence while filming, with joystick-based focus adjustments during planning.
- Required work: port the Cinematographer-mode C code, map its lens control hooks to PROP_LV_LENS focus_pos, ensure safe toggling when ML menus are active, and implement CFG persistence (cinemato.cfg).
- Risks/notes: ensure compatibility with 70D EDMAC/ADTG flow; avoid interfering with live AF if not in manual mode.
- References: Cinematographer-mode project and thread (GitHub and forum thread 27053).

### 3) Dual Fast Picture (two-shot bursts)
- Concept: Implement DualFastPicture behavior: two rapid shots with independent ISO/shutter settings to simulate fast bracketing.
- Why now: There are public ports of this module (LamnaTheShark/MagicLanternDualFastPicture) for other bodies; 70D can leverage the same approach using existing shoot pipeline and bramp logic.
- What it does: capture two frames in quick succession with independently chosen exposure settings, enabling quick exposure bracketing for HDR-like workflow in-camera.
- Required work: port the module to 70D (module folder, Makefile adaptation, and 70D-specific shooting path integration); expose user menu to configure two shot parameters; ensure timing works with LV and LiveView.
- Risks/notes: verify shutter timing to avoid mis-sync and ensure it works in LiveView vs. standard view.
- References: MagicLanternDualFastPicture repo (LamnaTheShark).

### 4) Touchscreen-driven focus control
- Concept: Use the 70D touchscreen to modify focus points in LiveView with minimal UI overhead.
- Why now: 70D has CONFIG_TOUCHSCREEN enabled; there are touch events already defined in GUI. The UX could be significantly improved for on-shot focusing.
- What it does: map touch gestures to lens focus shifts (tap to set focus, drag to nudge, two-finger pinch to zoom within LiveView) and reflect the focused distance in the overlay.
- Required work: integrate touchscreen events with lens focus_position updates (via lens.c) and update UI overlays; keep compatibility with existing focus modes and avoid conflicting with autofocus.
- Risks/notes: maintain stability with Canon's LV stack; ensure not to disrupt AF assist.

## Summary Table

| Issue | Severity | Status | Effort |
|-------|----------|--------|--------|
| LV_FOCUS_DATA missing | High | Partially fixed (PROP_LV_LENS) | Medium |
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
| WiFi tethering missing | Medium | Not started | High |
