# Canon EOS M (Original) — Magic Lantern Platform Analysis

**Firmware:** 2.0.2 | **Codename:** EOSM | **DIGIC:** V | **Release:** 2012 (First Canon mirrorless)

---

## 1. Hardware Overview

| Spec | Value |
|------|-------|
| **Sensor** | 18.0 MP APS-C CMOS (5184 × 3456) — same as 650D/Rebel T4i |
| **Processor** | DIGIC V |
| **RAM** | 256MB (half of 70D's 512MB) |
| **ROM0** | 8MB (assets) |
| **ROM1** | 16MB (code) |
| **Display** | 3.0" 1,040K-dot LCD, fixed (non-vari-angle), 3:2 ratio at 720×480 |
| **Touchscreen** | Capacitive (partially supported, needs "more hacking") |
| **Viewfinder** | None (mirrorless — always in LiveView) |
| **WiFi** | **None** (no built-in WiFi unlike 70D) |
| **GPS** | **None** (no internal GPS, external GP-E2 via accessory terminal) |
| **Audio** | Built-in stereo mic, 3.5mm external mic input, **no headphone jack** |
| **HDMI** | Mini HDMI output |
| **Storage** | Single SD card slot (no CF slot) |
| **Battery** | LP-E12 (smaller than 70D's LP-E6) |
| **Weight** | 298g body only |
| **Sensor AF** | Hybrid CMOS AF (phase-detect pixels embedded, NOT Dual Pixel like 70D) |

### Sensor Family: 650D/700D/100D/EOSM

The EOS M shares the same 18MP sensor as the 650D (Rebel T4i), 700D (Rebel T5i), and 100D (Rebel SL1). Key shared constants:

| Parameter | EOS M Value | vs 70D |
|-----------|-------------|--------|
| Color matrix | `6602,-841,-939,-4472,12458,2247,-975,2039,6148` | Different (70D uses 6D matrix) |
| Dynamic range (ISO100-12800) | `1060,1063,1037,982,901,831,718,622,536` | Slightly lower than 70D |
| LV skip_left | 72 | 144 |
| LV skip_top | 28 | 28 (same) |
| Photo skip_left | 72 | 142 |
| Photo skip_top | 52 | 52 (same) |
| column_factor | 4 | 8 |
| Binning | 3x horizontal, y-skip varies (4 for 720p, 2 for 1080p) | Similar but y-skip different |
| RAW_PHOTO_EDMAC | `0xC0F04008` | Same group |
| TG_FREQ_BASE | 32MHz | 32MHz (same) |
| EDMAC raw_write_chan | 0x12 | 0x12 (same) |
| EDMAC read_chan (memcpy) | 0x19 | 0x19 (same) |
| EDMAC write_chan (memcpy) | **0x13** | **0x11** (different!) |
| EDMAC bytes/transfer | **2** | **8** (different!) |

---

## 2. Platform Configuration

### Build System (`Makefile`)

```makefile
MODEL = EOSM
FW_VERSION = 202
AUTOEXEC_BASE = 0x800000
FIR_BASE = 0x800120
ML_BOOT_OBJ = boot-d45.o       # Classic boot (not AllocateMemory pool)
RESTARTSTART = 0x9E1E0
MAIN_FIRMWARE_ADDR = 0xFF0C0000
PLATFORM_ARCH = armv5te
ML_AF_PATTERNS_OBJ = n          # No regular AF (mirrorless)
```

**Boot method:** Classic `boot-d45.o` — copies firmware from MAIN_FIRMWARE_ADDR to relocation address, patches BSS, injects ML's `my_init_task`. The 70D uses `CONFIG_ALLOCATE_MEMORY_POOL` (boot-d45-am.c) which shrinks Canon's AllocateMemory pool. EOS M does NOT use this method.

### Key internals.h Differences from 70D

| Feature | EOS M | 70D | Notes |
|---------|-------|-----|-------|
| `CONFIG_ALLOCATE_MEMORY_POOL` | **No** | Yes | EOSM uses classic boot |
| `CONFIG_NEW_DRYOS_TASK_HOOKS` | **No** | Yes | EOSM uses old task hooks |
| `CONFIG_VARIANGLE_DISPLAY` | **No** | Yes | EOSM fixed screen |
| `CONFIG_TOUCHSCREEN` | **Yes** (FIXME) | Yes | *"Needs more hacking"* |
| `CONFIG_AFMA` | **No** | Yes | EOSM has no AF microadjustment |
| `CONFIG_LV_FOCUS_INFO` | **No** | Yes (via PROP_LV_LENS) | EOSM has PROP_LV_FOCUS_DATA in QEMU spells |
| `CONFIG_AUDIO_CONTROLS` | **No** | Yes (enabled 2026-05-05) | EOSM audio controls disabled |
| `CONFIG_BEEP` | **No** (ASIF hangs) | Yes | *"Asif hangs camera"* |
| `CONFIG_ELECTRONIC_LEVEL` | **No** | Yes | No level gauge |
| `CONFIG_EXPSIM` | **No** | Yes | Exposure simulation not configurable |
| `CONFIG_FPS_UPDATE_FROM_EVF_STATE` | **Yes** | No (broken on 70D) | EOSM works, 70D doesn't |
| `CONFIG_FPS_AGGRESSIVE_UPDATE` | **Yes** | No | EOSM needs aggressive updates |
| `CONFIG_EDMAC_MEMCPY` | Yes (both) | Yes (both) | Same |
| `CONFIG_EDMAC_RAW_SLURP` | Yes (both) | Yes (both) | Same |
| `CONFIG_EVF_STATE_SYNC` | Yes (both) | Yes (both) | Same |
| `CONFIG_ZOOM_X1` | **Yes** | No | EOSM has intermediate 1x zoom |
| `CONFIG_CAN_REDIRECT_DISPLAY_BUFFER` | Yes (easily) | Yes | Display filtering "easily" works |
| `CONFIG_WB_WORKAROUND` | Yes | Yes | Both |
| `CONFIG_ZOOM_BTN_NOT_WORKING_WHILE_RECORDING` | Yes | Not set | EOSM only |
| `CONFIG_BATTERY_INFO` | **No** | Yes (LP-E6) | EOSM doesn't report percentage |
| `CONFIG_NO_DEDICATED_MOVIE_MODE` | Yes | Yes | Both |
| MLU / Mirror lockup | **No** (mirrorless) | Yes | Fundamental difference |
| `CONFIG_PHOTO_MODE_INFO_DISPLAY` | **No** | Yes | |
| `CONFIG_SEPARATE_BULB_MODE` | **No** | Yes | |

---

## 3. Memory Map

### EOSM vs 70D RAM Layout

| Region | EOS M | 70D |
|--------|-------|-----|
| Total RAM | 256MB (0x10000000) | 512MB (0x20000000) |
| RAM range | 0x10000000–0x1FFFFFFF | 0x40000000–0x5FFFFFFF |
| AUTOEXEC_BASE | 0x800000 | 0x44C100 |
| RESTARTSTART | 0x9E1E0 | 0x44C100 |

### Key Buffer Addresses

| Buffer | EOS M | 70D |
|--------|-------|-----|
| YUV422_LV_BUFFER_1 | `0x4F1D7800` | `0x5F227800` |
| YUV422_LV_BUFFER_2 | `0x4F5E7800` | `0x5F637800` |
| YUV422_LV_BUFFER_3 | `0x4F9F7800` | `0x5EE17800` |
| YUV422_HD_BUFFER_1 | `0x44000080` | `0x53FFF780` |
| YUV422_HD_BUFFER_2 | `0x46000080` | `0x4EFFF780` |
| DEFAULT_RAW_BUFFER | `MEM(0x404E4 + 0x44)` | `MEM(0x7CFEC + 0x30)` |
| DEFAULT_RAW_BUFFER_SIZE | `0x47F00000 - 0x46798080` | `0x4CFF0000 - 0x4B328000` |
| SRM_BUFFER_SIZE | `0x1F24000` | `0x2314000` |
| EVF_STATE | `0x404E4` | `0x7CFEC` |
| SSS_STATE | `0x4018C` | `0x91BD8` |
| MOVREC_STATE | `0x42254` | `0x7CE48` |
| MVR struct ptr | `0x4C124` | `0x7AEA4` |
| Display state obj | `0x45C3C` | `0x7B4C8` |
| YUV display addr ptr | `0x44C28 + 0xEC` | `0x7B3EC + 0xEC` |

### State Object Parameters

```c
INPUT_SET_IMAGE_VRAM_PARAMETER_MUTE_FLIP_CBR: 26  // (70D: 23)
INPUT_ENABLE_IMAGE_PHYSICAL_SCREEN_PARAMETER: 27  // (70D: 23)
```

### Register Definitions

| Register | EOS M | 70D |
|----------|-------|-----|
| CARD_LED | `0xC022C188` (LEDON=0x138800, LEDOFF=0x838C00) | `0xC022C06C` |
| RTC_CS | `0xC022C0C4` | `0xC02201D4` |
| RTC_TIME_CORRECT | `0x98` | Different |
| FPS_REGISTER_A | `0xC0F06008` | Same |
| FPS_REGISTER_B | `0xC0F06014` | Same |
| FPS_CONFIRM_CHANGES | `0xC0F06000` | Same |
| RAW_PHOTO_EDMAC | `0xC0F04008` | Same |
| LV EDMAC channel base | `0xC0F26200` | Same |

---

## 4. Stubs

**Total: ~139 active NSTUB lines** (vs 70D's 277 — EOSM has significantly fewer, reflecting the simpler hardware).

### EOSM HAS, 70D does NOT

| Stub | EOSM Address | Notes |
|------|-------------|-------|
| `dma_memcpy` | RAM `0xAC40` | EOSM has a hardware DMA memcpy; 70D lacks this |
| `vsnprintf` | RAM `0x205CC` | Available |
| RemoveFile stubs | Present | More FIO stubs |

### 70D HAS, EOSM does NOT

EOSM is missing entire categories of stubs that the 70D has:

**Missing PTPIP/WiFi stubs (11 on 70D):**
- No `ptpip_sock_create`, `ptpip_bind_param`, `ptpip_open_server`
- No `ptpip_create_client`, `ptpip_listen_close`, `ptpip_close_server`
- No `socket_close_caller`, `socket_close_if_valid`
- No PTPIP socket accept or keepalive stubs
- No `LiveViewWifiApp_handler`

**Missing Socket API stubs (7 on 70D):**
- No `socket_create`, `socket_bind`, `socket_connect`
- No `socket_listen`, `socket_setsockopt`
- No `socket_recv`, `socket_send`

**Missing Audio stubs (partial):**
- No `SetASIFMode` (70D doesn't have it either)
- No `SetAudioVolumeIn`
- No `_audio_ic_write_bulk`

**Missing Property stubs:**
- No `PROPAD_GetPropertyData`

**Missing stubs that are present on 70D:**
- No `mvrSetFullHDOptSize`
- No `mvrSetGopOptSizeFULLHD`
- No `dma_memcpy` (actually EOSM HAS this, 70D doesn't)
- No `sounddev_active_in`

### EOSM Stub Address Categories

| Type | EOSM | 70D |
|------|------|-----|
| RAM (0x000xxxxx) | Many EDMAC/FIO stubs | Similar |
| ROM1 (0xFFxxxxxx) | ASIF, engio, memory stubs | Same pattern |
| Total count | ~139 | ~277 |

---

## 5. Features Status

### Working Features (Enabled or Not Undef'd)
- `FEATURE_CROP_MODE_HACK` — enabled
- `FEATURE_AUDIO_REMOTE_SHOT` — enabled  
- `FEATURE_EYEFI_TRICKS` — enabled (Eye-Fi card support)
- Shutter fine-tuning — *"It works! Timer values not applied until record (normal for EOSM)"*
- Video modes: 1080p30/25/24, 720p60/50, VGA30/25
- MLV lite, MLV play, MLV sound
- Dual ISO (photo mode) — table at `0x4048124C` (6 entries, SIZE=16)
- SD UHS overclock — stable at **240MHz hybrid** (best of any DIGIC V!)
- crop_rec — basic modes (OFF, 3x3 720p) — **no 3K/UHD/4K presets** (grouped as `is_basic` with 650D/700D/100D)

### Disabled Features (Undef'd — Cannot Be Implemented)
| Feature | Reason |
|---------|--------|
| `FEATURE_FORCE_LIVEVIEW` | *"Already Live View"* (mirrorless) |
| `FEATURE_MLU` / `FEATURE_MLU_HANDHELD` | *"No Mirror"* |
| `FEATURE_STICKY_DOF` | *"No DOF button"* (no DoF preview) |
| `FEATURE_IMAGE_EFFECTS` | *"DigicV new effects check Art Filter"* |
| `FEATURE_INTERMEDIATE_ISO_PHOTO_DISPLAY` | *"Will work in 1 mode"* |
| `FEATURE_AF_PATTERNS` | *"No regular AF"* (Hybrid CMOS AF only) |
| `FEATURE_VOICE_TAGS` | *"Just to be sure"* |
| `FEATURE_GHOST_IMAGE` | *"No way to pick image but works"* (inconsistent) |
| `FEATURE_SET_MAINDIAL` | *"Set taken over by Q"* (no main dial) |
| `FEATURE_QUICK_ZOOM` | No zoom buttons |
| `FEATURE_QUICK_ERASE` | Unknown |
| `FEATURE_LV_FOCUS_BOX_FAST` / `SNAP` | Focus system limitations |
| `FEATURE_ARROW_SHORTCUTS` | No arrow key shortcuts |
| `FEATURE_MAGIC_ZOOM_FULL_SCREEN` | *"Full-screen magic zoom is garbled"* |
| `FEATURE_PLAY_EXPOSURE_FUSION` / `COMPARE_IMAGES` / `TIMELAPSE` / `EXPOSURE_ADJUST` | Playback mode limitations |

### Commented-Out Features (Could Potentially Work)
| Feature | Status |
|---------|--------|
| `FEATURE_TRAP_FOCUS` | Undef'd — might work with LV_FOCUS_DATA |
| `FEATURE_FOLLOW_FOCUS` | Undef'd |
| `FEATURE_RACK_FOCUS` | Commented out (not undef'd) |
| `FEATURE_FOCUS_STACKING` | Commented out (not undef'd) |
| `FEATURE_VIDEO_HACKS` | Commented out — *"unclean patching"* |

---

## 6. Known Issues & Workarounds

### 6.1 ASIF/Beep Hangs Camera (CRITICAL)
- **File:** `platform/EOSM.202/internals.h`
- `CONFIG_BEEP` commented out with note: *"Asif hangs camera.. will look for a fix"*
- **Root cause:** The ASIF (Audio Serial Interface) DMA controller locks up when beep is attempted. This is a hardware/firmware bug in Canon's ASIF implementation on the EOS M.
- **Impact:** No ML beep/audio feedback for UI actions. Audio recording still works (different code path).
- **Status:** UNFIXED. Would require ASIF register-level debugging to determine the cause. Audio AK4646 init may be missing a hardware-specific step that the 70D does correctly.

### 6.2 Touchscreen Partial Support (FIXME)
- **File:** `platform/EOSM.202/internals.h`
- Note from nanomad: *"Needs more hacking, I'll fix it once I get the EOSM"*
- EOSM touch events are defined in `gui.h` (BGMT_TOUCH_1_FINGER, BGMT_TOUCH_2_FINGER, pinch events) but the touch driver integration is incomplete.
- **Status:** PARTIAL. Touch events exist in GUI but may not work reliably. Needs someone with a physical EOSM to debug.

### 6.3 Frame ISO/Shutter is "Stubborn"
- **File:** `src/fps-engio.c:2263,2301`
- `can_set_frame_iso()` returns 0 unless `RECORDING_H264`
- `can_set_frame_shutter_timer()` same behavior
- **Impact:** ISO and shutter speed overrides only work during movie recording, not in LV standby.
- **Status:** KNOWN LIMITATION. Referenced forum topic 5200. Affects exposure tools (ETTR, etc.) in photo mode.

### 6.4 Magic Zoom Uses Busy-Waiting (High CPU)
- **File:** `src/zebra.c:3356`
- EOS M is in the slow path with 5D2/50D: `#if defined(CONFIG_5D2) || defined(CONFIG_EOSM) || defined(CONFIG_50D)`
- Explicitly EXCLUDED from DIGIC V clean vsync: `#if defined(CONFIG_DIGIC_V) && !defined(CONFIG_EOSM)`
- **Impact:** Magic Zoom uses busy-waiting for vsync, causing higher CPU usage and potential overheating.
- **Status:** UNFIXED. The DIGIC V cacheable LV buffer method doesn't work on EOS M.

### 6.5 No LV_FOCUS_INFO, But PROP_LV_FOCUS_DATA Exists in Firmware
- **Critical difference from 70D:** EOSM's QEMU spells have complete PROP_LV_FOCUS_DATA replies with actual focus point arrays (70D's firmware lacks this entirely).
- Spells at QEMU lines 232-431 are **all commented out** — they exist in the boot logs but are not active.
- **Potential:** If uncommented, EOS M could have native focus confirmation bars, focus peaking, and possibly trap focus.
- Focus data format: 36-byte payload with focus point coordinates at different zoom levels.
- **Status:** NOT IMPLEMENTED. QEMU spells exist but are disabled. Phoenix could enable them.

### 6.6 No Zoom Buttons (Fake Codes)
- **File:** `platform/EOSM.202/gui.h`
- EOSM has hardware zoom buttons (left/right side of lens barrel on EF-M lenses). These send button codes as `BGMT_PRESS_ZOOM_IN = -0x112`.
- But `FEATURE_QUICK_ZOOM` is disabled.
- Fake negative codes ensure they fail read-only tests and don't pass to Canon firmware.

### 6.7 No Top Wheel (Fake Negative Codes)
- **File:** `platform/EOSM.202/gui.h`
- `BGMT_WHEEL_UP = -12345`, `BGMT_WHEEL_DOWN = -123456`
- No physical top wheel. Negative codes prevent passing to Canon firmware.

### 6.8 REC and LV Same Button
- **File:** `platform/EOSM.202/gui.h`
- `BGMT_REC = 0x1E`, `BGMT_LV = 0x1E` (same code)
- Same behavior as 70D. ML handles the distinction by tracking GUI state.

### 6.9 Menu Opens via Trash Button
- **File:** `src/menu.c:6248-6256`
- EOSM-specific `erase_longpress` struct (lines 6248-6256): long-press on DOWN/TRASH opens ML menu
- **Different from 70D:** 70D uses Q button long-press

### 6.10 Exit LV Uses SetGUIRequestMode(21)
- **File:** `src/movtweaks.c:256`
- `#ifdef CONFIG_EOSM` → `SetGUIRequestMode(21)` to shut off LiveView by switching to info screen
- Different from all other cameras which use different methods.

### 6.11 Lens struct is Unique
- **File:** `src/lens.h:161-185`
- EOS M has its own `prop_lv_lens` struct layout: 61 bytes total
- `focus_pos` at offset 0x22 (int16) — described as *"guess (not tested)"*
- `focal_len` at offset 0x2e
- `focus_dist` at offset 0x30 (expects `focus_dist2` — two distances)
- Different from 70D/5D3 group (which is 64 bytes with focus_pos at 0x23)

### 6.12 RAW Height Exception
- **File:** `src/raw.c:761-767`
- When `lv_dispsize == 1 && !video_mode_crop && !RECORDING_H264`:
  `height = 727` (forced constant due to forum-reported issue, topic 16608)
- **Impact:** RAW height calculation has a special case for EOSM in 720p non-crop non-recording mode.

### 6.13 QEMU Requires I2C Patch
- **File:** `qemu-eos/magiclantern/cam_config/EOSM/patches.gdb`
- Two patches: (1) Fix DL error at `0xFF1BE4AC`, (2) Patch `localI2C_Write` to always return success at `0xFF3460C4`
- The I2C patch is needed because EOSM firmware queries I2C devices that QEMU doesn't emulate.

### 6.14 CFN System — Simplified
- **File:** `platform/EOSM.202/cfn.c`
- Simple `GetCFnData/SetCFnData` calls (no mirror AF button, no MLU)
- `get_mlu()` returns 0, `set_mlu()` does nothing

### 6.15 Shoot.c — Direct Release
- **File:** `src/shoot.c:1064-1066`
- EOSM skips MLU check before release — directly calls `call("Release")`
- Because there's no mirror, the full release sequence is simpler.

### 6.16 Dynamic Range Values
- From `src/raw.c:658`:
```c
#ifdef CONFIG_EOSM
  {1060, 1063, 1037, 982, 901, 831, 718, 622, 536},  // ISO100-12800
#endif
```

### 6.17 Dual ISO Tables
- **Photo table:** `0x4048124C`, 6 entries (ISO 100-3200), SIZE=16
- **Frame table:** `0x40482516`, 6 entries, SIZE=34
- Same pattern as 650D/700D (18MP sensor family)

---

## 7. Modules

### Included in Magic Lantern ZIP (20 modules)
adv_int, arkanoid, autoexpo, bench, **crop_rec**, deflick, **dual_iso**, edmac, ettr, file_man, img_name, lua, **mlv_lite**, mlv_play, mlv_snd, pic_view, **sd_uhs**, selftest, **silent**

### NOT Included (not in modules.included, built but not distributed)
dot_tune, hw_test, ptptun, ramdump, raw_vidx, sf_dump, wifi_test, wifisrv, adtglog2

### Key Module Differences from 70D

| Module | 70D Status | EOSM Status | Notes |
|--------|-----------|-------------|-------|
| `crop_rec` | Full (8 presets) | Basic (OFF/3x3 720p) | EOSM is `is_basic` group |
| `dual_iso` | Works (photo) | Works (photo) | Different table addresses |
| `sd_uhs` | 160MHz max | **240MHz hybrid** | EOSM is best DIGIC V for SD overclock |
| `ptptun` | Included | Not included | No PTP tunnel on EOSM |
| `wifisrv` | Included | Not included | No WiFi hardware |
| `hw_test` | Included | Not included | |
| `mlv_rec` | Optional | Not included | |
| `raw_vidx` | Optional | Not included | |

---

## 8. QEMU Support

### QEMU Model (`model_list.c:386-395`)
```c
.name = MODEL_NAME_EOSM,
.digic_version = 5,
.ram_size = 0x10000000,      // 256MB
.current_task_addr = 0x3DE78,
.card_led_address = 0xC022C188,
.serial_flash_size = 0x800000,
.rtc_cs_register = 0xC022C0C4,
.rtc_time_correct = 0x98,
.dedicated_movie_mode = 0,
```

**Notable:** No `rom0_size`, `rom1_size`, `mpu_request_register`, `mpu_status_register`, `serial_flash_cs_register`, or `imgpowdet_register` — EOSM uses QEMU DIGIC V defaults.

### MPU Spells (`EOSM.h`)
- 45 active spells in init sequence
- PLUS extensive commented-out LV_FOCUS_DATA frames (the EOSM firmware DOES support focus data — unlike 70D)
- PROP_LV_LENS also present (commented out)
- Lens name hardcoded: "EF-S17-55mm f/2.8 IS USM"

### GDB Scripts
- `debugmsg.gdb` — Standard: DebugMsg, task_create, register_interrupt, register_func, CreateStateObject
- `patches.gdb` — Only 2 patches (I2C + DL fix)

### Test Script

```bash
./test_qemu.sh EOSM --boot        # Quick EOSM test
./test_qemu.sh EOSM --gdb          # With GDB
./test_qemu.sh EOSM --boot-trace   # With boot trace
```

Placeholder ROMs required at `roms/EOSM/` (ROM0.BIN, ROM1.BIN, SFDATA.BIN).

### QEMU-Specific Menu QEMU Fix
- `src/menu.c:6289`: *"delete QEMU thing"* — EOSM has a menu hack for QEMU mode:
  ```c
  #if defined(CONFIG_QEMU) && (defined(CONFIG_EOSM) || defined(CONFIG_EOSM2))
  ```

---

## 9. Build System Integration

| Aspect | Info |
|--------|------|
| Build target | `make -C platform/EOSM.202` |
| Build size | ~448KB (autoexec.bin) |
| Platform Makefile entry | Line 94 of `platform/Makefile` |
| modules.hidden | Empty (no hidden modules) |
| ML-SETUP.FIR | Present (binary installer) |
| Boot method | Classic boot-d45.o |
| DryOS version | 2.3, release #0051 |

---

## 10. Key Differences Summary: EOS M vs 70D

| Aspect | EOS M | 70D | Impact |
|--------|-------|-----|--------|
| RAM | 256MB | 512MB | Less buffer space for MLV |
| Boot | Classic | AllocateMemory pool | Different patching approach |
| Task hooks | Old | New (`CONFIG_NEW_DRYOS_TASK_HOOKS`) | Different hooking mechanism |
| Mirror | None | Yes | Simpler capture path |
| WiFi | None | Built-in 802.11n | No WiFi modules |
| Sensor | 18MP Hybrid CMOS AF | 20.2MP Dual Pixel AF | Different color/dyn range |
| LV_FOCUS_DATA | Supported in firmware | Not supported | EOSM could get focus features |
| FPS update | From EVF state | Manual | EOSM has smoother FPS control |
| ASIF/Beep | Hangs camera | Works | No beep on EOSM |
| Touchscreen | Partial FIXME | Working | |
| EDMAC write_chan | 0x13 | 0x11 | Different DMA channel |
| EDMAC bytes/transfer | 2 | 8 | Different DMA config |
| column_factor | 4 | 8 | Different RAW layout |
| SD overclock | 240MHz hybrid | 160MHz max | EOSM has better SD timing |
| AFMA | No | Yes (extended) | EOSM lacks AF adjust |
| Stubs count | ~139 | ~277 | Fewer capabilities |
| crop_rec | Basic (OFF/3x3 720p) | Full (8 presets) | EOSM is is_basic group |
| Menu open | Trash long-press | Q button long-press | Different UI |
| LV exit | SetGUIRequestMode(21) | Other methods | Unique exit path |
| Shutter count | Not tracked separately | Tracked | Minor |

---

## 11. Online Research Findings

### Forum Development Thread (Topic 9741, 75 pages, 2013-2023)
- Wayback Machine: `https://web.archive.org/web/2023/https://www.magiclantern.fm/forum/index.php?topic=9741`

### Developers & Contributors
| Person | Role | Notes |
|--------|------|-------|
| **nanomad** | Primary port author | Also 650D/700D/100D (all 18MP sensor family). Forum admin. |
| **jordancolburn** | Initial ML 2.0.2 port | Ported from Tragic Lantern fork (Dec 2013) |
| **1%** | Tragic Lantern maintainer | His EOSM work was the foundation for ML port |
| **dfort** | Major contributor | Shutter bug investigation, boot method experiments, focus pixel maps, build tutorials |
| **Danne** | Experimental builds | crop_rec_4k + mv1080p (most feature-rich options) |
| **theBilalFakhouri** | crop_rec development | 1x3 binning experiments |
| **garry23** | Lua scripts | Focus bar, toggler, landscape tools |
| **a1ex** | ML lead | Architectural guidance on shutter/FPS/ISO issues |

### CRITICAL BUGS (Show-Stoppers)

#### 1. Shutter Bug (CRITICAL, UNFIXED)
Camera refuses to shoot with EF-M zoom lenses (especially 18-55mm kit) when using SDXC cards >32GB.
- **Workarounds**: (a) Use ≤32GB SanDisk Extreme PRO cards, (b) twist lens off/on while camera is ON to break electrical contact, (c) use prime lenses (22mm works fine), (d) remove battery then mount kit lens
- **Root cause**: Unknown. Not firmware-related (affects stock Canon too). dfort tried AllocateMemory boot method — didn't help.
- **References**: Multiple forum pages (2014-2022), dfort's extensive investigation

#### 2. ASIF/Beep Hang (CRITICAL, UNFIXED)
Camera hard locks when beep attempted. Audio recording works fine (different code path).
- nanomad: *"will look for a fix"* — never found one
- Across entire 650D/700D/100D/EOSM family — no known solution exists
- Root cause: ASIF DMA controller lockup during beep playback only

### MAJOR BUGS

#### Red LED Boot Hang (Intermittent)
- Powers on but black screen + blinking green LED
- Multiple battery pulls needed to recover
- Was worse pre-Mar 2014, not fully resolved

#### MLV Playback
- RAW video won't play back in-camera (slow/doesn't work)
- Requires computer for playback

#### Shutter Display Nonsense
- Shows `0.-1, -1818` instead of `1/48, 1/64`
- a1ex: FRAME_SHUTTER_TIMER bug — suggests undefining to fall back to APEX

#### Pink Dots in RAW (Hardware Issue)
- Fixed pattern noise in sensor
- Workaround: Pink Dot Remover (PDR) in post-processing

#### Magic Zoom Locks UI + Overheating
- High CPU busy-wait causes overheating
- EOSM is in slowest code path (with 5D2/50D) — explicitly excluded from DIGIC V clean vsync
- nanomad investigated on 650D: turned off vsync completely — no improvement
- **UNFIXED** — always-on MZ will cause CPU overheating

#### Frame ISO/Shutter Stubborn
- Settings ignored in LV standby (only work during H.264 recording)
- a1ex: *"EOS-M does not configure LiveView with real shutter speed while not recording"*
- Workaround: undefine `FRAME_SHUTTER_TIMER` → fall back to APEX method

#### H.264 Proxy + RAW (Single SD Bottleneck)
- Corrupted frames on both streams when writing RAW + H.264 simultaneously
- Unlike 5D3 (CF+SD), EOSM has single SD card — can't handle dual writes

#### 3x Crop + HDMI Broken
- Regression in later builds (~2022)
- Users reverted to Jun 2021 build

### WORKING FEATURES (Confirmed by Community)
- ✅ 1080p H.264 with 3x crop — excellent moire/aliasing reduction
- ✅ crop_rec basic (3x3 720p), Danne's mv1080p (1736x976 16:9 @ 24fps 14bit lossless)
- ✅ 10/12/14-bit lossless RAW video
- ✅ SD UHS overclock: **240MHz @ 73MB/s** — BEST of any DIGIC V! (confirmed Feb 2023 by loknar)
- ✅ Dual ISO photo mode (fixed by nanomad Apr 2014 — was `"ISOLess PH Err (15)"`)
- ✅ Silent pictures, focus peaking, Lua scripting, audio meters
- ✅ Spotmeter, RAW zebras, ETTR, bulb mode, intervalometer
- ✅ MLV lite/play/sound modules, audio recording in crop_rec
- ✅ Audio Remote Shot

### Feature Details

#### LV_FOCUS_DATA Support
**Critical difference from 70D:** EOSM firmware DOES expose PROP_LV_FOCUS_DATA.
- QEMU spells (EOSM.h:232-431) have complete 36-byte focus point arrays with coordinates at multiple zoom levels
- All spells are **commented out** — need activation + hardware testing
- CONFIG_LV_FOCUS_INFO already enabled in current build

#### crop_rec Status
- **Basic**: OFF / 3x3 720p only (EOSM is `is_basic` group with 650D/700D/100D)
- **Danne's mv1080p**: 1736x976 continuous @ 24fps with 14bit lossless + sd_uhs. Requires manual focus (AF freezes LCD)
- **1x3 binning** (theBilalFakhouri): Works on 700D but corrupted frames on EOSM above 1150px height
- **3x crop + HDMI**: Broken in later builds (~2022)

#### Dual ISO
- Fixed by nanomad Apr 2014 (wrong ISO register address per camera)
- Photo mode confirmed working by garry23: *"Even the humble EOSM can exploit dual-ISO"*
- Tables at 0x4048124C (6 entries, SIZE=16) — same pattern as 650D/700D

#### SD Overclock
- **Best DIGIC V camera**: 240MHz hybrid @ 73MB/s SDR50 (confirmed Feb 2023 by loknar)
- 70D maxes at 160MHz
- Caution: higher presets may ruin cards
- SanDisk Extreme PRO ≤32GB recommended (also avoids shutter bug)

### Touchscreen Status
- CONFIG_TOUCHSCREEN defined but nanomad: *"Needs more hacking, I'll fix it once I get the EOSM"*
- Suggesting nanomad never had physical hardware to debug touch
- User reports: less responsive in later 2014 builds
- Physical scroll wheel on EOSM is fragile (some reports of actual hardware failure)
- Basic touch-focus works but not fully reliable

### Port Activity Timeline
- **Dec 2013**: jordancolburn ports Tragic Lantern to EOSM 2.0.2
- **2014**: nanomad takes over, fixes dual ISO, shutter bug investigations
- **~2016**: Declared stable/main build
- **~2017-2018**: Last official nightly builds
- **~2018**: Danne's experimental builds (mv1080p, crop_rec)
- **2022-2023**: Thread continues but only user Q&A, no new development
- **Current**: Maintenance mode. No active developer. Thread at 75+ pages.

### Camera Popularity & Ecoystem
- Extremely popular for RAW video due to: $100-150 used price + 240MHz SD overclock + 14bit lossless at 1736x976
- Strong community: Lua scripts (garry23), focus pixel maps (dfort), processing tools (Danne), crop_rec experiments (theBilalFakhouri)
- Often recommended as "cheapest RAW video camera" on ML forum

### Related Cameras
- **650D/Rebel T4i** — Same sensor, nearly identical firmware structure
- **700D/Rebel T5i** — Same sensor, minor improvements
- **100D/Rebel SL1** — Same sensor, even smaller body
- **EOS M2** — Successor model (Japan-only), different firmware
- **EOS M3** — Major redesign with better sensor

### Build Availability
- Stable "main build" available at `builds.magiclantern.fm` (via Wayback Machine)
- Last update: ~2018 (same era as nikfreak's last 70D updates)
- EOSM2 also has a build but is less stable

---

## 12. Development Recommendations

### High Priority (Easy Wins)
1. **Uncomment LV_FOCUS_DATA QEMU spells** — EOSM firmware supports it, just needs spell activation
2. **Enable CONFIG_ZOOM_X1 properly** — Already in internals.h, may just need testing
3. **Add 96kHz to mlv_snd** — Same change as 70D (trivial)
4. **Port hw_test module** — Copy from 70D, adjust register addresses
5. **Enable prop_lv_lens focus_pos testing** — Current offsets are *"guess (not tested)"*

### Medium Priority
6. **Touchscreen debugging** — Need physical EOSM, test BGMT_TOUCH events
7. **Fix ASIF beep** — Register-level debugging of ASIF DMA controller
8. **Add crop_rec higher resolutions** — Need 18MP sensor-specific calibration
9. **Port ptptun** — Add PTP tunnel USB control (useful for mirrorless)
10. **Enable more QEMU LV spells** — Uncomment PROP_LV_LENS, LV_BV, etc.

### Low Priority (Hardware Limitation)
11. **WiFi modules** — EOSM has no WiFi hardware, skip these entirely
12. **GPS integration** — External GP-E2 only, low interest for small body
13. **4K crop modes** — 18MP sensor can't do UHD/4K with enough resolution
14. **Headphone monitoring** — No hardware headphone jack

### Code Patterns to Follow
When writing EOSM-specific code, use these guard patterns:
```c
#if defined(CONFIG_EOSM)
    // EOSM-specific code here
#endif
```

For shared 18MP sensor code (with 650D/700D/100D):
```c
#if defined(CONFIG_650D) || defined(CONFIG_EOSM) || defined(CONFIG_700D) || defined(CONFIG_100D)
    // 18MP sensor family code
#endif
```

---

## 13. Key Source Files Reference

| File | Purpose |
|------|---------|
| `platform/EOSM.202/Makefile` | Build configuration |
| `platform/EOSM.202/internals.h` | Feature flags, memory addresses |
| `platform/EOSM.202/features.h` | Feature toggles |
| `platform/EOSM.202/consts.h` | Hardware constants, buffer addresses |
| `platform/EOSM.202/stubs.S` | Firmware entry points (139 NSTUBs) |
| `platform/EOSM.202/gui.h` | Button codes (no zoom buttons, fake wheel) |
| `platform/EOSM.202/cfn.c` | Custom functions (simplified) |
| `platform/EOSM.202/fps-engio_per_cam.h` | FPS timer registers |
| `platform/EOSM.202/fps-engio_per_cam.c` | FPS timer values |
| `platform/EOSM.202/mvr.h` | Movie recorder struct |
| `platform/EOSM.202/modules.included` | 20 built-in modules |
| `qemu-eos/hw/eos/mpu_spells/EOSM.h` | MPU spells (focus data exists!) |
| `qemu-eos/magiclantern/cam_config/EOSM/patches.gdb` | QEMU patches (I2C) |
| `qemu-eos/magiclantern/cam_config/EOSM/debugmsg.gdb` | QEMU debug logging |
