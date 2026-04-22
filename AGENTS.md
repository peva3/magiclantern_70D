# Magic Lantern Codebase Deep Dive: Canon 70D (DIGIC V)

## Development Requirements (MANDATORY)

**These requirements must be followed for ALL future work:**

1. **Documentation Updates:** Keep AGENTS.md and README.md files continuously updated with all findings, changes, and discoveries
2. **Task Tracking:** Maintain TODO.md with current task status, marking completed items and adding new tasks as discovered
3. **Testing:** Test every change before committing - use QEMU emulation, host-side compilation, and simulation frameworks
4. **Commit Protocol:** After completing each task or milestone, commit changes with clear messages and push to GitHub
5. **Git Identity:** Always use pmwoodward3@gmail.com (email) and peva3 (username) for commits
6. **Incremental Progress:** Complete work in small, testable chunks - never batch multiple untested changes

**Repository:** https://github.com/peva3/magiclantern_70D  
**Current Phase:** Week 1 - Foundation Setup  
**Last Updated:** 2026-04-22

---

This document contains a comprehensive architectural analysis of the Magic Lantern (ML) simplified codebase specifically focusing on the Canon 70D (firmware 1.1.2) platform.

## 1. Boot & Initialization Architecture

The 70D (being a DIGIC V camera) supports two boot paths, but relies primarily on the `AllocateMemory` pool boot mechanism.

*   **AllocateMemory Boot (Primary - `boot-d45-am.c`):** The camera boots by shrinking the Canon `AllocateMemory` pool. For the 70D, the pool start address is dynamically patched from `0x44C000` to `0x4E0000`, reserving 592KB of RAM for Magic Lantern. It relocates Canon's `init_task` and `CreateTaskMain` and returns a patched `init_task`.
*   **Classic Boot (`boot-d45.c`):** Copies firmware from `MAIN_FIRMWARE_ADDR` (`0xFF0C0000`) to a relocation address, patches the BSS segment, and injects ML's `my_init_task`. This is only used by the ML installer (`ML-SETUP.FIR`). Uses `RESTARTSTART = 0xFAF00` for installer vs `0x44C100` for normal boot.

**Initialization Sequence (`init.c`):**
1.  `boot_pre_init_task()`: Installs a task dispatch hook before Canon's `init_task`.
2.  Canon's `init_task` runs (with the ML hook intercepting task creation).
3.  `boot_post_init_task()`: Installs the custom assert handler (`DRYOS_ASSERT_HANDLER = 0x7AAA0`), waits for the GUI to start, and creates the `ml_init` task.
4.  `my_big_init_task()`: The main ML initialization thread. It initializes memory (`_mem_init`), finds the SD card (`_find_ml_card`), applies MMU cache hacks, loads fonts, initializes callbacks and menus, runs all registered `INIT_FUNC` routines (including modules), loads config, creates all `TASK_CREATE` tasks.

**Key globals:** `_hold_your_horses` (blocks task dispatch until ready), `ml_started`, `ml_gui_initialized`.

## 2. 70D Specific Configuration (`platform/70D.112/`)

The 70D port is heavily configured via feature toggles and memory constants.

### Key Addresses (`consts.h`)
| Constant | Address | Notes |
|----------|---------|-------|
| LED | `0xC022C06C` | |
| RAW_PHOTO_EDMAC | `0xc0f04008` | Same group as 5D3/6D/700D/650D/EOSM/100D |
| DEFAULT_RAW_BUFFER | `MEM(0x7CFEC+0x30)` | |
| SRM_BUFFER_SIZE | `0x2314000` | |
| EVF_STATE | `0x7CFEC` | |
| MOVREC_STATE | `0x7CE48` | |
| SSS_STATE | `0x91BD8` | |
| VIDEO_PARAMETERS_SRC_3 | `MEM(0x7CFD0)` | Source for FRAME_ISO/SHUTTER/APERTURE |
| Focus confirmation | `0x91A54` | |
| UNAVI_BASE | `0x9FC74` | Workaround for missing CancelUnaviFeedBackTimer |

### FPS Registers (`fps-engio_per_cam.h`)
| Register | Address |
|----------|---------|
| FPS_REGISTER_A | `0xC0F06008` |
| FPS_REGISTER_B | `0xC0F06014` |
| FPS_REGISTER_CONFIRM_CHANGES | `0xC0F06000` |
| TG_FREQ_BASE | `32000000` | Most others use 28800000 |

### Key Enabled Capabilities (`internals.h`)
| Flag | Description |
|------|-------------|
| `CONFIG_ALLOCATE_MEMORY_POOL` | Uses the bootloader memory trick described above. |
| `CONFIG_NEW_DRYOS_TASK_HOOKS` | Uses modern DryOS task hooking. |
| `CONFIG_VARIANGLE_DISPLAY` | Native support for flip-out screen. |
| `CONFIG_TOUCHSCREEN` | Native touch support. |
| `CONFIG_EVF_STATE_SYNC` | Perfect VSync synchronization via EVF_STATE object. |
| `CONFIG_EDMAC_RAW_SLURP` | High-speed raw memory scraping. |
| `CONFIG_EDMAC_MEMCPY` | EDMAC memcpy support. |
| `CONFIG_NO_DEDICATED_MOVIE_MODE` | No separate movie mode flag. |
| `CONFIG_FRAME_ISO_OVERRIDE_ANALOG_ONLY` | ISO override only affects analog. |

### Disabled / Missing Capabilities (`internals.h`, `features.h`)
| Flag | Reason |
|------|--------|
| `CONFIG_LV_FOCUS_INFO` | **Missing.** 70D firmware does not expose `LV_FOCUS_DATA`. Breaks focus confirmation, Magic Zoom, Focus Stacking. |
| `CONFIG_AUDIO_CONTROLS` | **Missing.** Cannot control audio settings from ML yet. |
| `CONFIG_FPS_UPDATE_FROM_EVF_STATE` | Doesn't work on 70D. |
| `CONFIG_BEEP` | Beep support disabled. |
| `CONFIG_LV_FOCUS_DATA` | No focus property available. |
| `FEATURE_FPS_OVERRIDE` | **Broken.** Timer B has untraceable issues. Timer A causes banding/patterns. Dev notes: "Tried it for a felt hundred hours." |
| `FEATURE_RAW_ZEBRAS` | **Broken.** Causes glitches in QuickReview and LiveView. |
| `FEATURE_FOLLOW_FOCUS` | Disabled (see lens.c). |
| `FEATURE_RACK_FOCUS` | Disabled (see lens.c). |
| `FEATURE_FOCUS_STACKING` | Disabled - buggy ("takes 1 behind and 1 before"). |
| `FEATURE_FLEXINFO` | Bottom bar flickers - time/battery display unstable. |

## 3. Stubs & Firmware Entry Points (`stubs.S`)

The 70D has 277 lines of firmware stubs covering:

**Startup:**
- `MAIN_FIRMWARE_ADDR = firmware_entry`
- `cstart = 0xFF0C1BA8`
- `init_task = 0xFF0C54CC`

**File I/O:** `fopen`, `fclose`, `fread`, `fwrite`, `FIO_ReadFile`, `FIO_WriteFile`, `fio_malloc`, `fio_malloc_temp`, `fio_free`

**GUI:** `gui_massive_event_loop`, `disp_check`, `draw_txt_rect`, `bm_put_chars`, `redraw`

**Audio/ASIF:** `ASIF`, `sounddev_task`, `ASIF_SetADC`, `ASIF_SetDAC`, `audio_thread`

**Properties:** `prop_register_slave`, `prop_get_property`, `prop_del_property`, `prop_patch_slave`, `PROP_HANDLER_*` macros

**EDMAC/DMA:** `ConnectReadEDMAC`, `ConnectWriteEDMAC`, `StartEDMAC`, `StopEDMAC`, `SetEDMAC`

**Tasks:** `CreateTask`, `CreateStateObject`, `SetHPTimer`, `SetTimerAfter`

**Memory:** `AllocateMemory`, `GetMemory`, `CreateStateObject`, `srm.*` functions

**Lens:** `AdjustFocusLens`, `SetLensFocalLength`, `set_motor_position`, `LvLensDrive`

## 4. Property System (`property.h`)

70D-specific property IDs:
```c
PROP_HI_ISO_NR = 0x80000049
PROP_HTP = 0x8000004a
PROP_MULTIPLE_EXPOSURE = 0x0202000c
PROP_MULTIPLE_EXPOSURE_SETTING = 0x8000003F
PROP_MLU = 0x80000047
```

Drive modes shared with 5D3:
- `DRIVE_SILENT = 0x13`
- `DRIVE_SILENT_CONTINUOUS = 0x14`

Model ID: `MODEL_EOS_70D = 0x80000325`
Firmware signature: `SIG_70D_112 = 0xd8698f05`

## 5. RAW Processing & EDMAC (`raw.c`, `edmac.c`, `edmac-memcpy.c`)

**EDMAC Configuration (DIGIC V - 48 channels):**
- `off1 bits = 19`, `off2 bits = 32`
- memcpy read channel: `0x19`
- memcpy write channel: `0x11`
- raw_write_chan (slurp): `0x12`
- 16 bytes/transfer on DIGIC V

**RAW Buffer Details:**
- RAW_PHOTO_EDMAC = `0xc0f04008` (connection #0)
- Color matrix: same as 6D — `(7034,-804,-1014,-4420,12564,2058,-851,1994,5758)`
- Black level: `0x3bc7`
- column_factor = 8 (like 5D3, different from other DIGIC V at 4)
- Dynamic range bounds: `{1091, 1070, 1046, 986, 915, 837, 746, 655, 555}`

**LV raw skip:**
- skip_top = 28
- skip_left = 144
- skip_right = 8 (zoom) or 0

**Photo raw skip:**
- skip_left = 142
- skip_top = 52
- skip_right = 8

## 6. Focus System (`focus.c`)

**Critical Limitation:** Line 926 notes "70D unfortunately has no LV_FOCUS_DATA property"

This means:
- No focus confirmation bars in Magic Zoom
- Focus graph/focus_misc task entirely disabled (lines 930-1054 wrapped in `#if !defined(CONFIG_70D)`)
- No focus_mag plotting
- No PROP_LV_FOCUS_DATA handler
- No focus_misc_task runs on 70D
- Trap_focus behavior differs on 70D vs other cameras

### Alternative Focus Data: PROP_LV_LENS

Despite lacking `PROP_LV_FOCUS_DATA` (0x80050026), the 70D DOES receive focus position data via `PROP_LV_LENS` (0x80050000):

- **Handler:** `lens.c:1900` - `PROP_HANDLER(PROP_LV_LENS)` extracts:
  - `lens_info.focus_dist` (uint16, focus distance in cm)
  - `lens_info.focus_pos` (int16, fine steps from lens, updates during motor movement)
- **Struct:** `lens.h:117` - `prop_lv_lens` struct with `focus_pos` at offset 0x23
- **Registration:** Unlike typical handlers, this property is registered via Canon property system (auto-registered from PROP_HANDLER macro)

This data is used in `lens.c:1868-1889` for tracking lens position changes but is NOT exposed to focus confirmation UI (Magic Zoom) which requires the higher-frequency `PROP_LV_FOCUS_DATA` format.

## 7. Investigation: Broken Features & Fix Potential

### 7.1 LV_FOCUS_DATA (Focus Confirmation) - MEDIUM FIX POTENTIAL

**Problem:** 70D firmware doesn't expose `PROP_LV_FOCUS_DATA` (0x80050026), which is required for focus confirmation bars in Magic Zoom and focus peaking.

**Current state:**
- Entire `focus_misc_task` wrapped in `#if !defined(CONFIG_70D)` (focus.c:930-1054)
- `PROP_LV_FOCUS_DATA` handler never registered for 70D

**Alternative available:** `PROP_LV_LENS` (0x80050000) IS available and provides:
- `lens_info.focus_dist` - focus distance in cm
- `lens_info.focus_pos` - fine steps from lens (updates during motor movement)

**Already implemented:** The lens.c:1900 handler already extracts focus_pos from PROP_LV_LENS into `lens_info.focus_pos`:
```c
lens_info.focus_pos = (int16_t) bswap16( lv_lens->focus_pos );
```

**Fix approach:** Re-enable focus_misc_task and modify to use `lens_info.focus_pos` instead of `PROP_LV_FOCUS_DATA`. The main limitation is update frequency - LV_LENS updates slowly, LV_FOCUS_DATA updates quickly during AF. Basic focus tracking is possible but may have latency.

### 7.2 FPS Override - MEDIUM FIX POTENTIAL

**Problem:** Explicitly disabled in `platform/70D.112/features.h:15`:

```c
// Really, this simply doesn't work
// Tried it for a felt hundred hours
// TIMER_B has untraceable problems
// Using TIMER_A_ONLY causes banding / patterns
#undef FEATURE_FPS_OVERRIDE
```

**Details:**
- FPS_REGISTER_A = 0xC0F06008 (Timer A - controls row readout)
- FPS_REGISTER_B = 0xC0F06014 (Timer B - controls frame timing)
- FPS_REGISTER_CONFIRM_CHANGES = 0xC0F06000
- TG_FREQ_BASE = 32000000 (different from most other cameras at 28800000)

**Forum fix (David_Hugh):** Found that Timer A-only workaround works via "HiJello-FastTv" setting. FPS_REGISTER_B works differently on 70D than other DIGIC V cameras.

**Fix approach:** Test Timer A-only FPS override, verify stability. Requires hardware testing to validate.

### 7.3 RAW Zebras - FIXED (HIGH FIX POTENTIAL)

**Problem:** Explicitly disabled in code at `zebra.c:4121`:

```c
// 70D has problems with RAW zebras
// TODO: Adjust with appropriate internals-config: CONFIG_NO_RAW_ZEBRAS
#if !defined(CONFIG_70D) && defined(FEATURE_RAW_ZEBRAS)
if (zebra_draw && raw_zebra_enable == 1) raw_needed = 1;
#endif
```

**Symptom:** Causes visual glitches in QuickReview and LiveView

**Fix applied:** Added `CONFIG_NO_RAW_ZEBRAS` to `platform/70D.112/internals.h` and updated zebra.c to use the proper config flag instead of hardcoded `#if !defined(CONFIG_70D)`. This properly documents the limitation for maintainability.

## 8. Lens System (`lens.c`)

- 70D uses same digital zoom handling as 600D (`CONFIG_600D || CONFIG_70D`) with `PROP_DIGITAL_ZOOM_RATIO`
- **Focus features bug** (line 677): "70D focus features don't play well with this and soft limit is reached quickly"
- Focus stacking: "still buggy and takes 1 behind and 1 before all others afterwards are before at the same position no matter what's set in menu"
- 70D shares `prop_lv_lens` struct layout with 6D/5D3/100D/750D/80D/7D2/5D4 (line 104)

## 9. Custom Functions (`cfn.c`)

- ALO, HTP, MLU accessed via generic macros (not CFn on 70D - in main menus instead)
- `PROP_CFN_TAB` = `0x80010007`, length `0x1c`
- Position 7 = AF button assignment: `0=AF`, `1=metering`, `2=AeL`
- ML tracks AF button state via direct memory array `some_cfn[0x1c]`

## 9. AF Microadjustment (`afma.h`)

- `PROP_AFMA = 0x80010006`, buffer size `0x22`
- Mode at offset `0xD`
- Per-lens wide at offset `20`
- Per-lens tele at offset `21`
- All lenses at offset `23`

## 10. GUI & Touch (`gui.h`)

**Touch Events:**
- `BGMT_TOUCH_1_FINGER = 0x6f`
- `BGMT_UNTOUCH_1_FINGER = 0x70`
- `BGMT_TOUCH_MOVE = 0x71`
- Two-finger events defined but noted "unavailable on this camera"

**Buttons:**
- REC and LV share same button code: `0x1E`
- METERING/AF-area button toggle commented out ("unreliable")
- Play mode zebras mapped to LIGHT button

## 11. State Objects (`state-object.h`)

- `DISPLAY_STATE = DISPLAY_STATEOBJ`
- `INPUT_ENABLE_IMAGE_PHYSICAL_SCREEN_PARAMETER = 23` — vsync source
- `EVF_STATE`, `MOVREC_STATE`, `SSS_STATE` pointers verified by nikfreak

## 12. MVR (Movie Recorder) (`mvr.h`)

70D-specific struct (140 lines, 0x1D8 bytes):
- QScale configuration
- GOP sizes, opt sizes for all frame rates:
  - 30/25/24 fps FullHD
  - 60/50 fps HD
  - 30/25 fps VGA
- 70D-specific: `uint32_t unk[0x192]`, size `0x658`

## 13. Modules (`modules.included`)

**Enabled (21 modules):** adv_int, arkanoid, autoexpo, bench, crop_rec, deflick, dot_tune, dual_iso, edmac, ettr, file_man, img_name, lua, mlv_lite, mlv_play, mlv_snd, pic_view, sd_uhs, selftest, silent

**Not built by default:** mlv_rec, raw_vid, raw_vidx, script, io_crypt, bolt_rec, bulb_nd, yolo, fpu_emu

### Module-Specific 70D Details

**dual_iso:**
- `PHOTO_CMOS_ISO_START = 0x404e5664`
- COUNT = 7, SIZE = 20, CMOS_ISO_BITS = 3, CMOS_FLAG_BITS = 2
- CMOS_EXPECTED_FLAG = 3
- Movie mode broken (needs investigation)

**sd_uhs:**
- CID_hook = `0xff7cf394`
- sd_setup_mode = `0xFF33E078`
- sd_setup_mode_in = `0xFF33E100`
- sd_set_function = `0xFF7CE4B8`
- sd_write_clock = `0xff7d18a0`
- sd_read_clock = `0xff7d1e68`
- SD_ReConfiguration = `0xFF7D086C`
- 160MHz1 doesn't work — needs PauseReadClock/PauseWriteClock hooks

**mlv_lite / mlv_rec:**
- Both use `cam_70d = is_camera("70D", "1.1.2")` pattern
- dialog_refresh_timer_addr = `0xff558ff0`

**edmaclog:**
- hooks_arm_70D.c with hooks for CreateResLockEntry, StartEDMAC, ConnectReadEDMAC, ConnectWriteEDMAC, SetEDMAC

**lossless (silent):**
- Different ProcessTwoInTwoOutLosslessPath
- Different register settings (e.g., `0xC0F373B4 = 0`)

## 14. raw_vid / raw_vidx Architecture

These represent a cleaner, newer architecture than mlv_rec:

**Producer-Consumer Pattern:**
1. event_pusher (CBR context, fast) pushes events to queue
2. worker (separate task, slower) consumes events
3. Event pool with semaphore protection (20 events)
4. Double-buffered capture: two fio_malloc buffers alternated each vsync
5. Double-buffered write: while one crop buffer fills via EDMAC, other flushes to disk

**MLV v3 Library (raw_vidx only):**
- Session-based API: `start_mlv_session()`, `stop_mlv_session()`, `write_file_headers()`, `write_video_frame_header()`
- Binary compatible with MLV 2.0 but with enum-based block types
- Still has FIXMEs: global variable dependencies (`raw_info`, `lens_info`, `camera_model`)

**70D NOT enabled for raw_vid/raw_vidx** — would need crop dimension mapping and worker priority tuning.

## 15. TCC (Tiny C Compiler)

- Located in `modules/script/tcc/`
- Version: 0.9.26 (Fabrice Bellard)
- Configured for ARMv5 (`HOST_ARM=1`, `TCC_ARM_VERSION=5`)
- Statically linked, no dynamic loading (`CONFIG_TCC_STATIC`, `CONFIG_NOLDL`)
- Used for on-camera C script compilation

## 16. Notable Architectural Quirks & Hacks

- **Cache Hacks:** Unlike DIGIC 6+ cameras using MMU table manipulation, 70D uses CPU cache line locking hacks in `patch.c` to patch ROM instructions on the fly.
- **AF Button Swap:** 70D handles CFn differently — ALO, HTP, MLU are in main menus, not CFn. Uses `some_cfn[0x1c]` array.
- **A/B Firmware Toggle:** `reboot.c` contains workaround for 70D dual firmware partitions.
- **SD Overclocking:** `sd_uhs` module works but `160MHz1` fails immediately on 70D SD host controller.
- **FlexInfo Flickering:** Bottom bar (time, battery) flickers due to GUI rendering conflict. Uses `UNAVI_BASE` workaround.
- **Missing CancelUnaviFeedBackTimer:** 70D firmware lacks this function, requiring alternative approach.

## 18. Forum Findings (Discovered Issues & Solutions)

The following issues and discoveries were found through the Magic Lantern forum thread (topic 14309, 140 pages):

### Working Features (Confirmed by Users)
- **Zebras (over/under):** OK in photo mode, fast zebras only work (raw zebras disabled)
- **Focus Peak:** OK in photo mode (greyscale)
- **Crop Marks:** OK in photo and play modes
- **Ghost Image:** OK
- **Spotmeter:** OK (position works)
- **False Color:** OK (banding detection works)
- **Waveform:** Works but sometimes flickers
- **Vector scope:** OK
- **Histogram:** Works well (sometimes freezes, reboot battery helps)
- **Level indicator:** Freezes after ~1 minute in LV (known bug)
- **Audio meters:** OK
- **RAW video:** Works but with limitations (hot pixels at higher ISO)
- **Dual ISO (photo):** Works per user reports
- **ETTR:** Works
- **Crop_rec (3x zoom):** Works in photo mode with some caveats

### Known Issues & Solutions
- **Hot pixels in RAW video:** More prominent at ISO 1600+, especially in 3X crop mode. Solution: Keep ISO low or use 14-bit lossless compression.
- **FPS override:** Initially disabled due to vertical banding issues. Later experimentally re-enabled using Timer A only (Timer B has untraceable problems on 70D). David_Hugh found that `FPS_REGISTER_B` at address `C0F06014` works differently than other DIGIC V cameras. Workaround: Use Timer A via "HiJello-FastTv" setting.
- **SD card write speed:** Limited to ~40MB/s without overclocking. With sd_uhs module using 160MHz preset, can get up to ~70MB/s. Higher presets (192/240MHz) cause instability on 70D.
- **Electronic level freeze:** Freezes after ~1-2 minutes in LV. Workaround: Disable ML overlays by pressing INFO button, use Canon's level, then re-enable ML.
- **Level indicator freeze:** Reported as freezing after ~1 minute of use. Related to EVF_STATE rendering.
- **ML menu disappears:** Menu flickering/disappearing in LiveView/movie mode (also affects 6D).
- **RAW zebras:** Disabled because they cause problems in QuickReview and LV. Causes visual glitches.
- **Focus features:** Trap focus only works in photo mode (not LiveView) due to missing LV_FOCUS_DATA property. Other focus features (follow, rack, stacking) disabled.
- **Dual ISO video:** Not working initially, later fixes were attempted
- **Hot pixels from EDMAC_RAW_SLURP:** Were causing issues in early builds, fixed by disabling raw slurping
- **Shutter speed ignored:** Sometimes increasing shutter speed doesn't take effect (only decreasing works)
- **FPS changes randomly:** Users report 23.97fps randomly changing to 23.98

### Critical Discoveries by Users
- **FPS Register address:** `C0F06014` (Timer B) works differently on 70D than other cameras (discovered by David_Hugh)
- **SD clock registers:** Related to `C0F04xxx` range for SD speed (from nikfreak's investigation)
- **Focus pixel patterns:** 70D has its own focus pixel map, needs separate FPM file

### BitBucket Repository
- Main development repo: `https://bitbucket.org/nikfreak/magic-lantern/branch/70D_merge_fw112`
- Crop_rec_4k variants were developed by ArcziPL with 14-bit lossless support
- SD_UHS custom builds available from various contributors

### Key Contributors
- **nikfreak:** Primary 70D port developer
- **a1ex:** Main ML developer, helped with fps-engio and lossless
- **David_Hugh:** Found FPS timer solution
- **ArcziPL:** crop_rec_4k experiments with 14-bit lossless
- **theBilalFakhouri:** sd_uhs module enhancements

## 17. Codebase Statistics

- **137+ matches** for "70D" / "70d" references across the codebase
- **55+ files** read and analyzed
- **16 known bugs/TODOs** specifically for 70D
- Closely related to: **6D**, **5D3** (share color matrix, EDMAC channels, EVF_STATE)

## Conclusion

The Canon 70D is a capable DIGIC V platform with robust EDMAC RAW video capabilities (sharing much with the 5D3 and 6D). However, its biggest architectural blindspots are the complete lack of LiveView Focus Data (`LV_FOCUS_DATA`), broken FPS timers, and finicky audio controls. Development on this body requires heavy reliance on `EVF_STATE` hooks rather than clean properties. The 70D uses cache hacks for patching (not MMU like newer cameras), has unique crop_rec limitations, and the SD controller cannot handle aggressive overclocking.
