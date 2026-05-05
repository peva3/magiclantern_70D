# Changelog — Magic Lantern Canon 70D Port

All notable changes since forking from `reticulatedpines/magiclantern_simplified`
and `reticulatedpines/qemu-eos`.

## 2026-04-30 — hw_test v27: Final Automated Test Suite

### Added
- **Pre-deployment test suite** (`tests/run_all.sh`): 3 automated checks
  - Module symbol validation (catches TCC `strncat`/`strncpy` errors before camera)
  - Companion script syntax check (`camremote.py`, `camtunnel.py`)
  - Build size verification (under 656KB limit)
- **FPS stability test** (hw_test S3.2): samples FPS_TA 20× over 1 second
  - Result: range=0, value=699 across all samples — rock-solid timing
- **Extended call() dispatch test** (hw_test S23.x): 10 new functions
  - Working: `GetHDMIInfo`=2010036, `FA_GetProperty`, `FA_GetPropertyAddress`,
    `SetAudioVolumeOut`, `TurnOffDisplay`
- **ADTG/crop_rec registers** (hw_test S5.8): CMOS write addr, ADTG 8172/8173/8178/8179,
  ENGIO HEAD3/4
- **SD card mode** (hw_test S9.x): reads SD_MASTER register, interprets idle/active state

### Fixed
- `config_rw` test: increased FAT flush delay from 50ms to 200ms — SD card needed
  more time between close and open for read-back to work
- `strncat` undefined symbol in hw_test: ML's minimal `vsnprintf` doesn't support
  `%u` format — changed to `%d` (used across all printf in firmware)

### Changed
- All 28 70D modules now auto-build via `ML_MODULES` list in `modules/Makefile`
  (no more stale `.mo` artifacts from manual copy)
- Dead probe modules deleted: `gpsprobe`, `defectprobe`, `diprobe`
  (all integrated into hw_test)
- `yolo` module removed from `ML_MODULES` (200D-only, runtime-locked)
- `ramdump`, `ptptun`, `wifi_test`, `wifisrv` added to `ML_MODULES` auto-build
- Build system: zip modules dir cleaned before `copy_included_modules.py` to
  prevent stale `.mo` accumulation

### Hardware Verification (hw_test v27)
- **35 PASS / 5 SKIP / 0 FAIL** on physical Canon 70D
- All register baselines match (6/6), FPS TA/TB stable
- Dual ISO tables confirmed (3 photo + 1 movie, stride 20/46)
- PTPIP stubs: 11/11 valid, socket API: 7/7 loaded
- RAM dumps: LOWER 16MB, ISO 1MB, UPPER 16MB

---

## 2026-04-30 — S30: All Software Tasks Built Out

### WiFi TCP Server (S30.1)
- **wifisrv v2**: TCP server with persistent read-eval loop on port 5555
  - Commands: P(ping), S(status: battery/shutter/temp/LV/remote_shot_flag),
    R(remote shutter via `FA_RemoteRelease`), L(toggle LiveView),
    B(bootdisk), Q(disconnect)
  - Uses RAM-loaded socket functions (validated by hw_test on hardware)
  - Config from `ML/SETTINGS/WIFISRV.CFG`
- **camremote.py**: laptop companion with persistent shell mode
  - `python3 camremote.py shell` for interactive session
  - `python3 camremote.py status` for one-shot status

### GPS/Defect Probes (S30.2-3)
- All GPS call() functions return -1 (NOT_FOUND) on 70D — GPS functions exist
  in firmware (`[GPS] GPS_Initialize` in boot log) but not exposed via eventproc
- `ExecuteDefectMarge1-3` also return -1 — defect API not in call() table
- All integrated into hw_test as info-only SKIP sections

### PTP Tunnel (S28.3)
- `ptptun` module + `camtunnel.py` companion — USB remote access
- 8 PTP commands: CallByName, ConsoleCapture, Screenshot, ExecuteLua,
  EngioRead/Write, ShamemRead, SetPropertyRaw
- `PTP_CHDK_ExecuteScript` fix in `src/ptp-chdk.c`: saves Lua to
  `ML/SCRIPTS/CHDK_RUN.LUA` instead of "not implemented" stub

### Latent Build Bugs Fixed
- `crop_rec.c`: Extra `}` closing `engio_write_hook()` prematurely
- `mlv_3.c`: Wrong variable name `session.image_data_alignment` →
  `session_data.image_data_alignment`; RTCI case had `break` instead of `return 0`
- `worker.c`: Missing `#include <string.h>` for `strncpy`

---

## 2026-04-30 — Full 512MB RAM Dump & Analysis

### RAM Dump Module
- `ramdump` module: Debug menu entry gated by `ML/SETTINGS/RAMDUMP.RUN` flag file
- Dumps 512MB (0x40000000-0x5FFFFFFF) to `ML/LOGS/RAM_FULL.BIN` in 256KB chunks
- Safety probe skips 0xFFFFFFFF unmapped pages, reports final size + skipped count
- On-screen progress every 4MB, battery check before starting

### Analysis Results (509MB data, 12,639 unique strings)
- **~520 callable functions** extracted: boot/power, LV/sensor, WiFi/network,
  AF/lens remote, FIO, FA_* factory adjustment, EDMAC/DMA, memory, property,
  audio, GPS, touchscreen TCH_*, defect management, H.264 encoding
- **Complete WiFi stack**: DLNA/UPnP MediaServer 1.50, WlanSdcomDrv SDIO driver,
  192.168.1.20 hardcoded IP, 8 PTPIP ROM1 stubs + 7 RAM-loaded socket functions
- **270+ ML symbols** at runtime addresses: memory allocators, EDMAC, ISO/exposure,
  menu/GUI, remote shot, battery, audio, property, task, lens, movie, debug
- **30+ PROP_ IDs**: Connection/WiFi, Display/LV, Lens/Focus, Audio, Card
- **55 Canon source file paths**: Kernel, Memory, Sensor, Video, WiFi (all subsystems)
- **Memory allocator hierarchy**: AllocateMemoryResource → SRM → PackHeap/RingHeap →
  fio_malloc → __priv_malloc

### New Systems Discovered
- **GPS**: `GPS_Initialize`, `GPSList`, `GPSTime` functions exist in firmware
- **Touchscreen**: `TCH_CheckTouchICVersion`, full capacitive touch API
- **Defect/pixel management**: `ExecuteDefectMarge1-5`, `FA_LvDetectDefects*`,
  `FA_DefectsTestImage`, `sht_savedefectsproperty`

### RAM Layout
| Range | Density | Content |
|-------|---------|---------|
| 0x40000000–0x41000000 | 65% | ML code + data (~260K strings) |
| 0x41000000–0x44000000 | 55–70% | Canon structs, task data, GUI buffers |
| 0x44000000–0x4E000000 | 45% | AllocateMemory pool, ML_OBJS |
| 0x4E000000–0x60000000 | 15–30% | Sparse heap (0x55555555/0xAAAAAAAA) |

---

## 2026-04-29 — PTP Tunnel & Cleanup

### Sprint 26: PTP Tunnel
- Created `modules/ptptun/` — USB PTP tunnel at opcode 0xA1E9
- `camtunnel.py` (410 lines, pure Python3/pyusb)
- Module name truncated to `ptptun` (≤8 chars for FAT 8.3 compliance)

### Sprint 27: Dead Code Cleanup
- Removed 14 `#if 0` blocks (~630 lines) from `src/`
- Removed ~33 commented-out code lines across 15 files
- Deleted `bitrate-6d.c` (656 lines, entirely dead)
- Removed various dead GDB stubs, io_trace fallbacks, AutoISO experiments

### hw_test v15 Hardware Verification
- **23 PASS / 2 SKIP / 0 FAIL** on physical 70D
- Subsystems proven: model detection, fio_malloc, shamem_read (MMIO), get_ms_clock,
  msleep, bmp_vram, semaphores, FIO file I/O
- PTPIP stubs: 11/11 VALID on hardware (all have ARM PUSH prologues)
- Socket API: 7/7 LOADED in RAM
- NW command interface: valid at 0xFF46CCD8
- `call("dumpf")` returns 0

### Camera Hang Fixes
- `EnableBootDisk` and `TurnOnDisplay` identified as freeze-causing on 70D
  (requires battery pull) — excluded from call() dispatch tests
- SD read-back test causing hang on physical camera — removed from hw_test

---

## 2026-04-28 — WiFi Reverse Engineering & Audio

### PTPIP Stubs (Sprint 23)
8 ROM1-safe NSTUBs added to `stubs.S` at 0xFF7AEE00–0xFF7AF500:
- `ptpip_sock_create`, `ptpip_bind_param`, `ptpip_open_server`,
  `ptpip_create_client`, `ptpip_listen_close`, `ptpip_close_server`,
  `ptpip_set_keepalive`, `ptpip_errno_handler`
- Plus `socket_close_caller` (0xFF14F74C), `socket_close_if_valid` (0xFF7AF380)
- Later (S30): `ptpip_sock_accept` (0xFF7AF160)

### Socket API Addresses (RAM-loaded 0x0005xxxx)
| Function | Address | Callers |
|----------|---------|---------|
| `socket_create` | 0x00059AF8 | 24 |
| `socket_bind` | 0x00059E94 | 3 |
| `socket_connect` | 0x00059DDC | 8 |
| `socket_listen` | 0x0005A9D0 | 9 |
| `socket_recv` | 0x00059CE8 | 13 |
| `socket_send` | 0x0005A09C | 30 |
| `socket_setsockopt` | 0x0005A810 | 47 |

### Audio Reverse Engineering (Sprint 8)
- `SetAudioVolumeIn` verified at 0xFF11970C (valid ARM PUSH prologue)
- All core ASIF stubs present: StartASIFDMAADC, StopASIFDMAADC,
  StartASIFDMADAC, StopASIFDMADAC, SetNextASIFADCBuffer, SetNextASIFDACBuffer
- **NO DIGIC V camera** has `CONFIG_AUDIO_CONTROLS` enabled
- Audio IC type unknown — AK4646 registers in `audio-ak.c` may not work on 70D
- I2C helpers call RAM-loaded code (0x10000xxxx range), similar to socket library

### RAW Zebras Root Cause (Sprint 4)
- **Dual Pixel CMOS AF pixels** (70D unique) have different photometric response
- Causes false overexposure/underexposure when zebra algorithm scans every pixel
- EDMAC RAW SLURP single-buffer race condition is a secondary contributor
- Fix requires Focus Pixel Map (FPM) for 70D or double-buffering

---

## 2026-04-27 — QEMU 70D Emulation

### MPU Spell Improvements (Sprint 17-18)
- Restructured spell #1/#2 to match 6D pattern
- Added 5 missing property groups: PROP_AF_MICROADJUSTMENT,
  PROP_LV_LENS in PERMIT_ICU_EVENT, PROP_CONTINUOUS_AF_VALID variant,
  PROP_ROLLING_PITCHING_LEVEL chain, PROP 80050034
- **0 unknown MPU messages** during Canon boot + ML boot

### ROM1 Size Bug Fix
- `rom1_size` corrected from 8MB to **16MB** in both QEMU and ML
  (all DIGIC V cameras use 16MB ROM1)
- Real ROM dumps obtained from physical 70D:
  ROM0.BIN (8MB), ROM1.BIN (16MB), SFDATA.BIN (16MB)
- Full firmware boot achieved: `startupInitializeComplete` at ~576ms,
  ML GUI registered at ~608ms

### Boot Log Analysis
- Confirmed K325 READY → ICU Firmware 1.1.2 → Full 4-phase startup
- Canon WiFi initialization traced: WFT, DP, RMT, PTPCOM all initialize
- `PROP_WIFI_SETTING [0]` and `PROP_NETWORK_SYSTEM [0]` at boot (WiFi off by default)

### QEMU Enhancement (Sprint 24)
- 15 tasks completed: module loading fix, BMP capture, boot marker, SD I/O tests,
  property system testing, config validation, task scheduling, timer callbacks,
  menu navigation, display visualization, performance benchmarking, memory leak
  detection, log analysis tools, regression tracking

---

## 2026-04-27 — SD UHS Analysis & Module Audit (Sprints 12-15)

### Dead Code Purges (Sprints 12-13)
- **S12**: Removed 318 lines of `#if 0` dead code across the codebase (debug
  blocks, unused implementations, stale function prototypes). Fixed bitwise vs
  logical operator in `raw.c`. Cleaned `gui-common.c` redundant logic.
  Build: 444KB.
- **S13**: Deleted `bitrate-6d.c` (656 lines, entirely dead). Removed 420+ more
  lines of dead code. Build: 436KB (8KB saved).

### Module Audit (Sprint 14)
- **sd_uhs**: 160MHz1 explicitly broken on 70D (acts like 192MHz). No GPIO
  register overrides for 70D. SDR50 baseline borrowed from 700D — may not match.
  Menu shows unstable presets (192/240MHz) without warning — potential data
  corruption risk.
- **dual_iso**: Photo mode works (user-confirmed). Movie mode deliberately
  disabled (FRAME_CMOS_ISO_START = 0). CMOS bit parameters (BITS=3, FLAG_BITS=2,
  EXPECTED_FLAG=3) copied from 7D, unverified for 70D.
- **mlv_lite**: Well-supported. Only 70D-specific: `dialog_refresh_timer_addr`.
  Lossless compression works with proper 70D register handling.
- **crop_rec**: No 70D initialization block existed — added in S20.
  Skip offsets, timer tables, and presets all needed per-camera calibration.

### sd_uhs Safety Hardening (Sprint 15)
- 70D-only menu now exposes only OFF/160MHz with warning text about unstable
  higher presets. 160MHz2 preset used (~70MB/s stable), 160MHz1 blocked.
  Build: 439KB.

### Cross-Port Features Enabled
- FEATURE_ZOOM_TRICK_5D3, FEATURE_KEN_ROCKWELL_ZOOM_5D3 (double-click zoom)
- FEATURE_SWAP_INFO_PLAY (info display in playback)
- FEATURE_LV_FOCUS_BOX_SNAP_TO_X5_RAW
- FEATURE_FOCUS_PEAK_DISP_FILTER
- Fixed `arrow_key_mode_toggle` guard in `tweaks.c`

---

## 2026-04-26 — Dual ISO Breakthrough & FPS Override

### Dual ISO Addresses CONFIRMED Correct
- Earlier "wrong addresses" was a hw_test bug: used `shamem_read()` for RAM
  addresses instead of `MEM()`. `shamem_read()` only works for MMIO (0xC0Fxxxxx).
- **3 ISO tables found:**
  - 0x404e5664 — photo still (stride 0x14)
  - 0x404e5704 — photo mirror (stride 0x14)
  - 0x404e7248 — LV/movie (stride 0x14, alternate at 0x404e77d6 stride 0x2E)
- All 7 ISO values match expected linear pattern (0x0003, 0x0027, 0x004b, ...)
- `FRAME_CMOS_ISO_START` uncommented for movie mode in `dual_iso.c`

### FPS Override Enabled
- `FEATURE_FPS_OVERRIDE` enabled — Timer A-only via HiJello/FastTv (fps_criteria=3)
- Timer B has untraceable banding issues on 70D (confirmed by David_Hugh)
- FPS_REGISTER_A = 0xC0F06008, FPS_REGISTER_B = 0xC0F06014
- TG_FREQ_BASE = 32MHz (different from most cameras at 28.8MHz)
- Build: 462KB (+11KB baseline)

### crop_rec 70D Timer Tables
- 70D-specific `default_timerA/B` (TG_FREQ_BASE=32MHz):
  - 23.976fps: A=700, B=1907
  - 25fps: A=800, B=1600
  - 29.97fps: A=700, B=1525
  - 50fps: A=800, B=800
  - 59.94fps: A=672, B=794
- CMOS_WRITE=0x26B54, ADTG_WRITE=0x2684C, ENGIO_WRITE=0xFF2BC6C4

---

## 2026-04-25 — MLV v3 Port & Build Baseline

### raw_vidx (MLV v3) for 70D
- Set crop dimensions: 1280×720 (~38.7MB/s at 24fps, under 40MB/s SD limit)
- MLV v3 session API: binary compatible with MLV 2.0, enum-based block types
- Worker priorities (0x11/0x9) kept as 200D defaults
- Added 70D crop offset placeholder in `event_pusher.c`

### L2: MLV v3 Global Dependency Cleanup
- Removed `#include` dependencies on `raw.h`, `lens.h`, `fps.h` from `mlv_3.c`
- Expanded `mlv_session` struct to carry camera-specific state
- Resolved 15 FIXMEs related to global variable access

### FPS Override QEMU Boot Confirmed
- Previous QEMU crash was INVALID — stale 25KB autoexec.bin on SD image
- Proper 462KB build boots cleanly in QEMU

---

## 2026-04-22 — Initial Fork & Foundation

### Repository Setup
- Forked from `reticulatedpines/magiclantern_simplified`
- Forked `qemu-eos` as git subtree
- Set up ARM cross-compilation toolchain
- Verified first build: autoexec.bin 435KB

### Initial Feature Enablement
- `CONFIG_ALLOCATE_MEMORY_POOL`: primary boot mechanism
- `CONFIG_NEW_DRYOS_TASK_HOOKS`: modern task hooking
- `CONFIG_VARIANGLE_DISPLAY`: flip-out screen
- `CONFIG_TOUCHSCREEN`: capacitive touch
- `CONFIG_EVF_STATE_SYNC`: VSync via EVF_STATE
- `CONFIG_NO_DEDICATED_MOVIE_MODE`
- `CONFIG_FRAME_ISO_OVERRIDE_ANALOG_ONLY`

### Focus via PROP_LV_LENS (CONFIG_LV_FOCUS_INFO)
- 70D firmware does not expose `PROP_LV_FOCUS_DATA` (0x80050026)
- Alternative: `PROP_LV_LENS` (0x80050000) provides `focus_pos` at offset 0x23
- Focus confirmation uses stability detection (4 consecutive identical samples)
- Slower update than native LV_FOCUS_DATA but functional

### Documentation
- AGENTS.md: comprehensive architectural analysis (22 sections, 50+ files analyzed)
- TODO.md: sprint planning, hardware vs non-hardware task categorization
- doc/hardware_testing.md: crop_rec calibration checklist
- README.md: current status and progress tracking

## 2026-05-05 — Sprint 33: Eventproc Address Mapping Complete (263 entries, 40 tables)

### Added
- **Eventproc table scanning**: Comprehensive ROM1 scan for 8-byte (name_ptr, func_ptr) pairs
- **40 distinct eventproc tables found** across 5 major categories:
  - FA_* (Factory Adjustment): 4 tables, ~140 entries — calibration, display, LV, defects, dark
  - Display/Filter/TFT: 3 tables, ~40 entries — power save, HDMI, display control, backlight
  - Touch: 1 table, 26 entries — TCH_* touchscreen API
  - WiFi/Protocol: 1 table, 9 entries — PrepareCommunication, SetWiFiEnable, etc.
  - Debug/Test/Mon: 7 tables — memory nav, monitor, test images, ISO speed, capture unit
  - Grid/Guides: 6 tables — gmtwaku_* overlay functions
  - PTP: 1 table, 4 entries — PTP framework control
  - Others: 17 tables — shutter, pixel shift, power, GPU graphics, etc.
- **263 unique functions mapped** from name to runtime address
- **315 new eventproc names discovered** beyond original doc/ramdump.md list
- **457 Ghidra labels applied** via analyzeHeadless/ApplyLabels.java

### Fixed
- **Clarified non-eventproc dispatch mechanisms**: AfCtrl_* (C++ vtables), FIO_* (DryOS SWI syscalls),
  PROP_* (runtime property registration), Eeko_* (image pipeline descriptors),
  WLANSDCOMDRV_* (dynamic/debug-only names)

### Changed
- `eventproc_full_map.csv`: 264 rows (header + 263 entries)
- `all_symbols.csv`: 579 total symbols (166 NSTUB + 81 ML + 332 eventproc + existing)
- `eventproc_labels.csv`: Ghidra-compatible label import file
- TODO.md: Sprint 33 completion with verification of unmapped function categories

## 2026-05-05 — wifi_enable Module: WiFi Init + TCP Server for 70D

### Added
- **New module: wifi_enable** (`modules/wifi_enable/`) — initializes 70D WiFi stack
  using RE-discovered eventproc sequence (DisablePowerSave → InitializePTPFrameworkController → LughPowerOn → LughConnect → OpenSession → EnablePowerSave)
- **Companion script** (`companion/camremote.py`) — Python TCP client for camera remote control
- **Module registers**: wifi_enable added to ML_MODULES in modules/Makefile and platform modules.included
- All 7 RAM-loaded socket functions (0x0005xxxx) verified and used for TCP server
- Build: 457KB autoexec.bin, 454KB magiclantern.bin (4.9KB module)

## 2026-05-05 — Sprint 35: DRYOS Kernel Dispatch Architecture RE

### Added
- **Discovered: DIGIC V uses NO SWI/SVC instructions for system calls** (unlike typical ARM CPUs)
- **RAM-loaded kernel modules**: firmware functions at 0x000xxxxx are loaded by `cstart` at boot
- **Jump table at ROM1+0xBA4BB0**: `LDR PC,[PC,#-4]!` trampolines → RAM module addresses
- **Wrap-around BL mechanism**: firmware at 0xFF0Cxxxx calls RAM via 32-bit address wrap
- **cstart analysis**: 85 BL calls to RAM with 36 unique targets during boot
- **Descriptor table**: at 0xFF0Axxxx for boot-time NSTUB fixup
- **FIO dispatch fully documented**: filesystem driver struct with function pointer table
- **Full analysis**: `doc/dryos_dispatch.md`

### Changed
- **0xFF000000 corrected**: previously modeled as ROM1 mirror in QEMU (real HW has RAM here)
- **AGENTS.md**: update discovered 7 RAM jump tables, 41 entries total
- **TODO.md**: Sprint 35 completion

### Fixed
- SWI dispatch gap: resolved via QEMU `-d int` trace — zero SWI exceptions during firmware boot
- Audio codec: AKM AK4646 confirmed via build system (ML_AUDIO_OBJ default, no 70D override)
- GPS: 70D has no internal GPS (external GP-E2 only); call() returns -1 is expected
- MMIO registers expanded from ~45 to ~179 across 15+ categories

### Build
- autoexec.bin: 457KB, magiclantern.bin: 454KB, 28 modules

## 2026-05-05 — Documentation Reorganization

### Changed
- **Analysis docs moved to `doc/` directory**: RAMDUMP.md → doc/ramdump.md, DEEPDIVE.md → doc/deepdive.md,
  WIFI_INIT.md → doc/wifi_init.md, HARDWARE-TESTING.md → doc/hardware_testing.md,
  QEMU_VERSION_ANALYSIS.md → doc/qemu_version_analysis.md, NEXT_STEPS.md → doc/next_steps.md,
  FIRMWARE_ANALYSIS.md → doc/firmware_analysis.md, ROM-DUMP.md → doc/rom_dump.md
- All cross-references updated across AGENTS.md, CHANGELOG.md, README.md, TODO.md
- Core dump file (core.1593555) removed
- `70d-dev/` directory removed from repo (stale build deployment)

## 2026-05-05 — CONFIG_AUDIO_CONTROLS Enabled + 96kHz Sample Rate

### Added
- **CONFIG_AUDIO_CONTROLS enabled** (`platform/70D.112/internals.h`): unlocks 8 audio features
  - Analog gain (0-32dB in 8 steps), digital gain (L/R independent)
  - AGC toggle (Canon's auto-level permanently disabled via `my_sounddev_task` override)
  - Wind filter (HPF), input source (internal/ext/balanced/auto)
  - Mic power (Low Z / High Z), headphone monitoring, headphone volume
- **96kHz sample rate in mlv_snd**: added to `mlv_snd_rates[]` selector — AK4646 supports it
- **sounddev_active_in NSTUB**: added at same address as SoundDevActiveIn (0xFF11936C)

### Fixed
- **Null dereference in audio-ak.c:191**: guarded `HOTPLUG_VIDEO_OUT_STATUS_ADDR` write
  (value is 0 on 70D, causing `-Wnull-dereference` error with -Werror)

### Changed
- 24-bit mode documented as blocked by missing `SetASIFMode` address (needs ROM search;
  present on 6D.116 at 0xFF11CD44, 700D.113 at 0xFF109510)
- mlv_snd_prepare_audio(): falls back to 16-bit if 24-bit is selected (infrastructure ready)

### Build
- autoexec.bin: 461KB (+4KB from 457KB baseline, safe at 656KB limit)

## 2026-05-05 — Script Reorganization

### Changed
- **Root directory cleaned**: moved test/tool scripts to appropriate subdirectories
  - `test_module_load.sh` → `tests/test_module_load.sh`
  - `run_hw_test.sh` → `tests/run_hw_test.sh`
  - `extract_hw_logs.sh` → `tools/extract_hw_logs.sh`
- **Kept in root**: `test_70d_qemu.sh`, `deploy.sh`, `validate_build.sh`
- Fixed internal path references in moved scripts (SCRIPT_DIR resolution)
- Build verification test (`tests/run_all.sh`) already in `tests/`
