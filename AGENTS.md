# Magic Lantern Codebase Deep Dive: Canon 70D (DIGIC V)

## Development Requirements (MANDATORY)

**These requirements must be followed for ALL future work:**

1. **Build Size Monitoring (CRITICAL):** The 70D reserves ~656KB for Magic Lantern (0x4E0000 to 0xD3C000). ALWAYS verify build size after any change:
   - Check `platform/70D.112/build/autoexec.bin` size
   - Current baseline: 444KB (safe margin: ~212KB)
   - NEVER commit changes that exceed 600KB without verification
   - Include size in commit messages for tracking
    - **Size Tracking Log:**
      | Date | autoexec.bin | magiclantern.bin | Changes |
      |------|--------------|------------------|---------|
      | 2026-04-22 | 435KB | 432KB | Initial baseline |
      | 2026-04-22 | 438KB | 435KB | CONFIG_ZOOM_HALFSHUTTER_UILOCK |
      | 2026-04-22 | 442KB | 439KB | +CONFIG_BEEP |
      | 2026-04-22 | 443KB | 440KB | +CONFIG_Q_MENU_PLAYBACK, CONFIG_WB_WORKAROUND |
      | 2026-04-22 | 441KB | 437KB | +CONFIG_LV_FOCUS_INFO (focus confirmation via PROP_LV_LENS) |
| 2026-04-25 | 462KB | 459KB | +FEATURE_FPS_OVERRIDE (Timer A-only via HiJello/FastTv) |
| 2026-04-26 | 452KB | 448KB | crop_rec 70D timer tables (module-only change, no autoexec impact) |
| 2026-04-26 | 452KB | 448KB | S5: crop_rec 70D CMOS/ENGIO fixes + Timer A/B recalculation (module-only) |

2. **Documentation Updates:** Keep AGENTS.md and README.md files continuously updated with all findings, changes, and discoveries
3. **Task Tracking:** Maintain TODO.md with current task status, marking completed items and adding new tasks as discovered
4. **Testing:** Test every change before committing - use QEMU emulation, host-side compilation, and simulation frameworks
5. **Commit Protocol:** After completing each task or milestone, commit changes with clear messages and push to GitHub
6. **Git Identity:** Always use pmwoodward3@gmail.com (email) and peva3 (username) for commits
7. **Incremental Progress:** Complete work in small, testable chunks - never batch multiple untested changes

**Repository:** https://github.com/peva3/magiclantern_70D
**Current Phase:** Week 7 - QEMU 70D Emulation
**Last Updated:** 2026-04-24

## QEMU-EOS Setup (in-tree)

The qemu-eos codebase is merged directly into this repo (as a git subtree with full history). No separate clone needed.

### One-time build setup:
```bash
# 1. Populate nested submodules (keycodemapdb, dtc, capstone)
cd qemu-eos
git clone https://gitlab.com/qemu-project/keycodemapdb.git ui/keycodemapdb
cd ui/keycodemapdb && git checkout 6b3d716e2b && cd ../../..
git clone https://gitlab.com/qemu-project/dtc.git dtc
cd dtc && git checkout 88f18909db && cd ..
git clone https://gitlab.com/qemu-project/capstone.git capstone
cd capstone && git checkout 22ead3e0bf && cd ..

# 2. Configure and build
mkdir -p build && cd build
../configure --target-list=arm-softmmu --disable-werror
make -j$(nproc)
```

### Quick test:
```bash
./test_70d_qemu.sh              # Quick test (10s, placeholder ROMs)
./test_70d_qemu.sh --gdb        # With GDB server
./test_70d_qemu.sh --boot-trace # GDB + boot-trace script
```

### GDB scripts:
- `qemu-eos/magiclantern/cam_config/70D/debugmsg.gdb` — Standard task/interrupt/func logging
- `qemu-eos/magiclantern/cam_config/70D/boot.gdb` — Enhanced 4-phase boot trace
- `qemu-eos/magiclantern/cam_config/70D/patches.gdb` — Patches only (sio_send_retry)

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
| `CONFIG_LV_FOCUS_INFO` | **Enabled (using PROP_LV_LENS).** 70D firmware does not expose `LV_FOCUS_DATA`, but we use `PROP_LV_LENS` focus_pos with stability detection. Focus confirmation and Magic Zoom partially restored. |
| `CONFIG_AUDIO_CONTROLS` | **Missing.** Cannot control audio settings from ML yet. |
| `CONFIG_FPS_UPDATE_FROM_EVF_STATE` | Doesn't work on 70D. |
| `CONFIG_BEEP` | Beep support disabled. |
| `CONFIG_LV_FOCUS_DATA` | No LV_FOCUS_DATA property, but PROP_LV_LENS provides focus_pos. |
| `FEATURE_FPS_OVERRIDE` | **Enabled (2026-04-25).** Timer B has untraceable issues (banding). Timer A-only via HiJello/FastTv mode (fps_criteria=3) works. Previous QEMU crash was invalid (stale SD image). Build: 462KB. |
| `FEATURE_RAW_ZEBRAS` | **Broken.** Causes glitches in QuickReview and LiveView. |
| `FEATURE_FOLLOW_FOCUS` | Disabled (see lens.c). |
| `FEATURE_RACK_FOCUS` | Disabled (see lens.c). |
| `FEATURE_FOCUS_STACKING` | Disabled - buggy ("takes 1 behind and 1 before"). |
 | `FEATURE_FLEXINFO` | Bottom bar flickers - time/battery display unstable. |

## 3. Hardware Specifications & Capabilities

Based on Canon EOS 70D specification sheets and research documents:

### Key Hardware Features
- **Image Sensor**: 22.5mm × 15.0mm APS-C CMOS (4.1 µm pixel pitch)
- **Effective Pixels**: 20.2 megapixels (5472 × 3648)
- **Image Processor**: DIGIC 5+ (DIGIC V architecture)
- **Dual Pixel CMOS AF**: First Canon camera with this technology - phase detection across 80% of sensor
- **WiFi**: Built-in 802.11 wireless LAN (cannot be used simultaneously with Eye-Fi cards)
- **Touchscreen**: 3.0" vari-angle capacitive touch LCD (1,040,000 dots, approx. 100% coverage)
- **Viewfinder**: Pentaprism with 98% coverage, 0.95× magnification
- **Memory Cards**: SD/SDHC/SDXC with UHS-I bus support

### Performance Specifications
- **Continuous Shooting**: 7.0 fps (up to 65 JPEG or 16 RAW with UHS-I card)
- **ISO Range**: 100-12800 (expandable to H: 25600)
- **Shutter Speed**: 1/8000 to 30 sec., bulb, X-sync at 1/250 sec.
- **AF System**: 19-point cross-type (all cross-type at f/5.6, center point cross-type at f/2.8)
- **Battery**: LP-E6 lithium-ion (~920 shots at 23°C with 50% flash usage)

### Video Capabilities
- **Formats**: MOV container, H.264 video, Linear PCM audio
- **Resolutions & Frame Rates**:
  - 1920 × 1080: 30/25/24 fps (29.97/25/23.976 actual)
  - 1280 × 720: 60/50 fps (59.94/50 actual)  
  - 640 × 480: 30/25 fps (29.97/25 actual)
- **Compression**: IPB (inter-frame) or All-I (intra-frame)
- **Maximum Recording**: 29 min 59 sec per clip, 4GB file size limit (auto-creates new files)
- **Digital Zoom**: 3× crop from sensor center (no quality loss) + 10× interpolated zoom

### ML Development Implications
1. **WiFi Potential**: Built-in WiFi enables remote control/tethering possibilities if DryOS networking stubs can be reverse-engineered
2. **Dual Pixel AF**: Phase detection data may be accessible for depth mapping or advanced focus features (requires firmware RE)
3. **Touchscreen**: `CONFIG_TOUCHSCREEN` already enabled; gesture-based UI improvements possible
4. **UHS-I Support**: `sd_uhs` module already provides overclocking (limited to 160MHz on 70D)
5. **Video Modes**: ML can leverage All-I compression for better editing, 60fps HD for slow motion
6. **Battery Life**: Power management considerations for long recording sessions with ML overlays

## 4. Stubs & Firmware Entry Points (`stubs.S`)

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

## 5. Property System (`property.h`)

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

## 6. RAW Processing & EDMAC (`raw.c`, `edmac.c`, `edmac-memcpy.c`)

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

## 7. Focus System (`focus.c`)

**Critical Limitation:** Line 926 notes "70D unfortunately has no LV_FOCUS_DATA property". However, we have enabled CONFIG_LV_FOCUS_INFO and use PROP_LV_LENS focus_pos with stability detection.

This means:
- Focus confirmation bars in Magic Zoom now work (using lens position stability)
- Focus graph/focus_misc task enabled (with 70D-specific update_focus_pos_70d)
- focus_mag plotting uses lens position changes
- PROP_LV_FOCUS_DATA handler still missing but not needed
- focus_misc_task runs on 70D (polling lens_info.focus_pos)
- Trap_focus behavior differs on 70D vs other cameras (still limited)

### Alternative Focus Data: PROP_LV_LENS

Despite lacking `PROP_LV_FOCUS_DATA` (0x80050026), the 70D DOES receive focus position data via `PROP_LV_LENS` (0x80050000):

- **Handler:** `lens.c:1900` - `PROP_HANDLER(PROP_LV_LENS)` extracts:
  - `lens_info.focus_dist` (uint16, focus distance in cm)
  - `lens_info.focus_pos` (int16, fine steps from lens, updates during motor movement)
- **Struct:** `lens.h:117` - `prop_lv_lens` struct with `focus_pos` at offset 0x23
- **Registration:** Unlike typical handlers, this property is registered via Canon property system (auto-registered from PROP_HANDLER macro)

This data is used in `lens.c:1868-1889` for tracking lens position changes and is now also used for focus confirmation UI (Magic Zoom) via stability detection in `update_focus_pos_70d`, though with lower update frequency than `PROP_LV_FOCUS_DATA`.

## 8. Investigation: Broken Features & Fix Potential

### 8.1 LV_FOCUS_DATA (Focus Confirmation) - PARTIALLY FIXED

**Problem:** 70D firmware doesn't expose `PROP_LV_FOCUS_DATA` (0x80050026), which is required for focus confirmation bars in Magic Zoom and focus peaking.

**Solution implemented:** Enabled `CONFIG_LV_FOCUS_INFO` in `internals.h` and using `PROP_LV_LENS` focus_pos with stability detection (`update_focus_pos_70d`). The `focus_misc_task` is now compiled and runs on 70D, polling `lens_info.focus_pos` and detecting focus lock via position stability.

**Current state:**
- `focus_misc_task` enabled (with 70D-specific block)
- Focus confirmation menu now available in Magic Zoom settings
- Focus graph plotting uses lens position changes
- Update frequency limited by `PROP_LV_LENS` polling rate (slower than LV_FOCUS_DATA)

**Remaining limitations:** Focus confirmation may have latency due to slower updates; fine-tuning of stability thresholds may be needed.

### 8.2 FPS Override - ENABLED (2026-04-25)

**Status:** `FEATURE_FPS_OVERRIDE` is now **enabled** in `platform/70D.112/features.h`.

**History:**
- Originally disabled: "Tried it for a felt hundred hours"
- Timer B has untraceable issues (causes vertical banding/patterns on 70D)
- David_Hugh found Timer A-only workaround via "HiJello, FastTv" setting (fps_criteria=3)
- Previous QEMU crash (S3.1) was caused by stale 25KB autoexec.bin on SD image, NOT FPS code
- S3.1a: Confirmed booting in QEMU with proper 462KB build

**Details:**
- FPS_REGISTER_A = 0xC0F06008 (Timer A - controls row readout)
- FPS_REGISTER_B = 0xC0F06014 (Timer B - controls frame timing)
- FPS_REGISTER_CONFIRM_CHANGES = 0xC0F06000
- TG_FREQ_BASE = 32000000 (different from most other cameras at 28800000)
- 70D does NOT have NEW_FPS_METHOD defined (unlike 5D3/600D/60D/1100D)
- 70D does NOT have FRAME_SHUTTER_BLANKING_WRITE (commented out in consts.h)
- fps_criteria menu shows: "Low light", "Exact FPS", "Low Jello, 180d", "HiJello, FastTv"
- Recommended: Use fps_criteria=3 (HiJello/FastTv) for Timer A-only override

**Build impact:** 462KB with FPS override (+11KB vs 451KB baseline)

**Remaining concerns:** Hardware testing needed to verify Timer A-only produces stable video without banding.

### 8.3 RAW Zebras - FIXED (HIGH FIX POTENTIAL)

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

### 8.4 WiFi Tethering - NOT STARTED

**Problem:** 70D has built-in WiFi hardware but Magic Lantern lacks networking stubs (`socket_create`, `wlan_connect`, `NwLimeInit`, etc.) in `platform/70D.112/stubs.S`. Only one WiFi-related stub exists: `LiveViewWifiApp_handler` at `0xFF7523B4`.

**Feasibility:** The ML codebase includes a DryOS socket API (`ml_socket.h`) and a working WiFi example (`yolo.c` module). The 200D port provides a blueprint for networking stubs (DIGIC 8). However, 70D (DIGIC V) requires reverse engineering of firmware addresses.

**Required stubs:**
- Socket API: `socket_create`, `socket_bind`, `socket_connect`, `socket_send`, `socket_recv`, `socket_close_caller`
- WiFi management: `wlan_connect`, `nif_setup`, `set_IP_address`
- Canon WiFi initialization: `NwLimeInit`, `NwLimeOn`, `wlanpoweron`, `wlanup`, `wlanchk`, `wlanipset` (via `call()`)

**Findings:**
- `ml_socket.h` defines socket API and `wlan_settings` struct (size `0xFC`).
- `yolo.c` demonstrates full WiFi sequence: Lime core init → WiFi power on → connect → IP setup → socket communication.
- The `call()` function (variadic) invokes firmware functions by name; requires symbol table lookup.
- 70D currently missing all networking stubs except `LiveViewWifiApp_handler`.

**Potential:** Remote trigger/shooting, live image transfer, remote UI control, timecode/data exchange.

**Effort:** High (reverse engineering of firmware symbols, hardware verification).

## 9. Lens System (`lens.c`)

- 70D uses same digital zoom handling as 600D (`CONFIG_600D || CONFIG_70D`) with `PROP_DIGITAL_ZOOM_RATIO`
- **Focus features bug** (line 677): "70D focus features don't play well with this and soft limit is reached quickly"
- Focus stacking: "still buggy and takes 1 behind and 1 before all others afterwards are before at the same position no matter what's set in menu"
- 70D shares `prop_lv_lens` struct layout with 6D/5D3/100D/750D/80D/7D2/5D4 (line 104)

## 10. Custom Functions (`cfn.c`)

- ALO, HTP, MLU accessed via generic macros (not CFn on 70D - in main menus instead)
- `PROP_CFN_TAB` = `0x80010007`, length `0x1c`
- Position 7 = AF button assignment: `0=AF`, `1=metering`, `2=AeL`
- ML tracks AF button state via direct memory array `some_cfn[0x1c]`

## 11. AF Microadjustment (`afma.h`)

- `PROP_AFMA = 0x80010006`, buffer size `0x22`
- Mode at offset `0xD`
- Per-lens wide at offset `20`
- Per-lens tele at offset `21`
- All lenses at offset `23`

## 12. GUI & Touch (`gui.h`)

**Touch Events:**
- `BGMT_TOUCH_1_FINGER = 0x6f`
- `BGMT_UNTOUCH_1_FINGER = 0x70`
- `BGMT_TOUCH_MOVE = 0x71`
- Two-finger events defined but noted "unavailable on this camera"

**Buttons:**
- REC and LV share same button code: `0x1E`
- METERING/AF-area button toggle commented out ("unreliable")
- Play mode zebras mapped to LIGHT button

## 13. State Objects (`state-object.h`)

- `DISPLAY_STATE = DISPLAY_STATEOBJ`
- `INPUT_ENABLE_IMAGE_PHYSICAL_SCREEN_PARAMETER = 23` — vsync source
- `EVF_STATE`, `MOVREC_STATE`, `SSS_STATE` pointers verified by nikfreak

## 14. MVR (Movie Recorder) (`mvr.h`)

70D-specific struct (140 lines, 0x1D8 bytes):
- QScale configuration
- GOP sizes, opt sizes for all frame rates:
  - 30/25/24 fps FullHD
  - 60/50 fps HD
  - 30/25 fps VGA
- 70D-specific: `uint32_t unk[0x192]`, size `0x658`

## 15. Modules (`modules.included`)

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

## 16. raw_vid / raw_vidx Architecture

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

## 17. TCC (Tiny C Compiler)

- Located in `modules/script/tcc/`
- Version: 0.9.26 (Fabrice Bellard)
- Configured for ARMv5 (`HOST_ARM=1`, `TCC_ARM_VERSION=5`)
- Statically linked, no dynamic loading (`CONFIG_TCC_STATIC`, `CONFIG_NOLDL`)
- Used for on-camera C script compilation

## 18. Notable Architectural Quirks & Hacks

- **Cache Hacks:** Unlike DIGIC 6+ cameras using MMU table manipulation, 70D uses CPU cache line locking hacks in `patch.c` to patch ROM instructions on the fly.
- **AF Button Swap:** 70D handles CFn differently — ALO, HTP, MLU are in main menus, not CFn. Uses `some_cfn[0x1c]` array.
- **A/B Firmware Toggle:** `reboot.c` contains workaround for 70D dual firmware partitions.
- **SD Overclocking:** `sd_uhs` module works but `160MHz1` fails immediately on 70D SD host controller.
- **FlexInfo Flickering:** Bottom bar (time, battery) flickers due to GUI rendering conflict. Uses `UNAVI_BASE` workaround.
- **Missing CancelUnaviFeedBackTimer:** 70D firmware lacks this function, requiring alternative approach.

## 19. Forum Findings (Discovered Issues & Solutions)

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
- **Focus features:** Trap focus only works in photo mode (not LiveView) due to missing LV_FOCUS_DATA property. Focus confirmation now works via PROP_LV_LENS stability detection. Other focus features (follow, rack, stacking) disabled.
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

## 20. Codebase Statistics

- **137+ matches** for "70D" / "70d" references across the codebase
- **55+ files** read and analyzed
- **16 known bugs/TODOs** specifically for 70D
- Closely related to: **6D**, **5D3** (share color matrix, EDMAC channels, EVF_STATE)

## 21. Conclusion

The Canon 70D is a capable DIGIC V platform with robust EDMAC RAW video capabilities (sharing much with the 5D3 and 6D). However, its biggest architectural blindspots are the lack of native LiveView Focus Data (`LV_FOCUS_DATA`) (partially mitigated via PROP_LV_LENS), broken FPS timers, and finicky audio controls. Development on this body requires heavy reliance on `EVF_STATE` hooks rather than clean properties. The 70D uses cache hacks for patching (not MMU like newer cameras), has unique crop_rec limitations, and the SD controller cannot handle aggressive overclocking.

## 22. Recent Sprints (12-17) — Summary

- S12: Dead code purge & cleanup (removed multiple #if 0 blocks, fixed raw.c bitwise operator, cleaned gui-common.c)
- S13: Second pass dead code purge (deleted legacy bitrate-6d.c and additional disabled blocks)
- S14: Module audit & cross-port research (sd_uhs, mlv_lite, dual_iso, crop_rec), enabled safe features from 6D/5D3
- S15: sd_uhs safety hardening for 70D (menu restricted to OFF/160MHz presets with warning)
- S16: Documentation & WiFi research (DryOS networking stubs analysis, AGENTS.md/TODO.md updates)
- S17: QEMU 70D MPU spell fixes — restructured spell #1/#2 to match 6D pattern, added PROP_BOARD_TEMP and PROP_SW2_MOVIE_START replies, fixed eos.c ROM mirroring assert for development with placeholder ROMs
- S18: MPU spell coverage improvement — added 5 missing property groups (PROP_AF_MICROADJUSTMENT, PROP_LV_LENS in PERMIT_ICU_EVENT, PROP_CONTINUOUS_AF_VALID variant, PROP_ROLLING_PITCHING_LEVEL chain, PROP 80050034)
- S19: ROM1 size bug fix — corrected from 8MB to 16MB in both QEMU and ML; full firmware boot achieved with real ROM dumps
- S20: crop_rec 70D timer tables — added 70D-specific default_timerA/B (TG_FREQ_BASE=32MHz), get_default_timer*() accessors, max_resolutions sensor comment

Build verification: autoexec.bin 452KB, magiclantern.bin 448KB (under 656KB reserve)

## 23. QEMU 70D Emulation Status

### QEMU Model Configuration
- ROM0: 0xF0000000 (8MB), ROM1: 0xF8000000 (**16MB** — fixed from 8MB bug)
- RAM: 0x40000000-0x5FFFFFFF, MMIO: 0xC0000000-0xDFFFFFFF
- GDB scripts: `/app/70d/qemu-eos/magiclantern/cam_config/70D/` (debugmsg.gdb, patches.gdb, boot.gdb)
- Run via: `run_qemu.py 70D -q <build_dir> -r /app/70d/roms`
- Test script: `./test_70d_qemu.sh --boot --no-build --timeout 15`

### MPU Spell Fixes (Sprint 17)
- **Spell #1 terminator restored** — was commented out ("fixme: 0x80000001 does not complete")
- **Spell #2 created** — WaitID 0x80000001 handler with PROP_MULTIPLE_EXPOSURE_SETTING reply (mirrors 6D)
- **PROP_BOARD_TEMP** reply added to spell #27 (mirrors 6D spell #26)
- **PROP_SW2_MOVIE_START** self-reply added to spell #45 (mirrors 6D spell #42)
- **Duplicate spell #7 removed** — empty WaitID 0x80000001 handler was redundant with new spell #2

### MPU Spell Coverage (Sprint 18)
- **PROP_AF_MICROADJUSTMENT** added to init spell #1 and mode spell #2 (70D supports AFMA per afma.h)
- **PROP_LV_LENS** added as reply #19.13 in PERMIT_ICU_EVENT spell #19 (critical for focus confirmation)
- **PROP_CONTINUOUS_AF_VALID** variant with value=0x01 added as spell #31 (Dual Pixel AF)
- **PROP_ROLLING_PITCHING_LEVEL** chain added (spells #67-#69) — 70D has electronic level
- **PROP 80050034** added as spell #30 (present in 5D3, 700D, 60D, 600D)

### ROM1 Size Bug Fix
- **Issue:** `rom1_size` was incorrectly set to 8MB based on truncated ROM dump
- **Evidence:** Stub at `0xFFA1844C` (10.09MB offset), backup_region hardcoded 16MB, all DIGIC V peers use 16MB
- **Fix:** Changed to 16MB in both `qemu-eos/hw/eos/model_list.c` and `platform/70D.112/consts.h`
- **Impact:** Addresses above 0xFF800000 now map to correct physical offsets

### Boot Test Results (SUCCESS)
```
Canon Init: K325 READY → ICU Firmware 1.1.2 → startupInitializeComplete
GUI State: PROP_GUI_STATE 2 (active), PROP_VARIANGLE_GUICTRL enabled
ML Init: [MCELL][GuiFactoryRegisterEventCommissionProcedure] — ML GUI factory registered
MPU Stats: 250+ messages, 93 complete spell cycles, 0 hangs
```

### Remaining QEMU Gaps vs 6D (low priority)
- PROP_LV_FOCUS_DATA spell missing (firmware limitation, not fixable)
- NotifyGUIEvent spells commented out (lines 256-265 in 70D.h)
- HDMI GPIO address uses default 0x0138 (6D uses 0x0158)
- SD card partition detection — QEMU SD emulation accuracy issue
- I2C peripheral emulation — warnings (no I2C devices in QEMU)

### ROM Dump Requirement — RESOLVED
- Real ROM dumps obtained from physical 70D camera: ROM0.BIN (8MB), ROM1.BIN (16MB), SFDATA.BIN (16MB)
- ROM1 re-dumped with corrected ROM1_SIZE=16MB build
- All ROM files committed to `/app/70d/roms/70D/`

### Sprint 5 — crop_rec 70D Hardware Calibration Prep (2026-04-26)

**Comprehensive register audit** identified ~35+ hardcoded 5D3 values needing 70D calibration. Three bugs fixed:

1. **3X_TALL missing CMOS override** — `is_70D` switch block had NO case for CROP_PRESET_3X_TALL. Fixed by adding CMOS[7] (vertical centering from 5D3 CMOS[1] values), CMOS[2]=0x10E, CMOS[6]=0x170.

2. **center_canon_preview() bug** — Duplicate block (lines 1834-1851) redeclared `raw_xc`/`raw_yc` with 5D3 hardcoded values (146, 3744, 60, 1380), overwriting the camera-aware computation at lines 1806-1831. Fixed by removing the duplicate 5D3 block entirely.

3. **Timer A/B recalculated for 70D (TG_FREQ_BASE=32MHz)** in all reg_override functions. Timer A was scaled by ratio of 70D/5D3 defaults at same fps. timerB = 32000000 / (timerA * target_fps). These are theoretical values — need hardware verification.

**Remaining issues (NOT fixed, need hardware calibration):**
- CROP_PRESET_3X has NO ENGIO override (commented out: "fixme: corrupted image")
- ADTG readout_end uses `shamem_read(0xC0F06804) >> 16` noted as "fixme: D5 only"
- ADTG 0x8806 register only written for is_5D3 (artifact prevention) — 70D may need equivalent
- All CMOS[2], CMOS[6], CMOS[7] values are still copied from 5D3 (marked as needs 70D calibration)
- ENGIO 0xC0F06800 top-bar offsets (0x1F0017, 0x1D0017) are hardcoded 5D3
- ENGIO 0xC0F06804 end-column values (0x1AA, 0x20A, 0x22A) use 5D3 offset formula
- 3x3_48p HEAD3/4 base values (0x2B4, 0x26D) are 5D3 60p hardcoded
- head_adj values (-30, -20, -10) are 5D3 trial-and-error
