# Canon 70D Magic Lantern — Technical Deep Dive

> Everything we learned reverse-engineering the Canon EOS 70D, porting Magic Lantern to the latest codebase, and validating on hardware.

---

## Table of Contents

1. [Build System & Infrastructure](#1-build-system--infrastructure)
2. [Hardware Test Module (hw_test)](#2-hardware-test-module-hw_test)
3. [WiFi & Networking Stack](#3-wifi--networking-stack)
4. [Full RAM Dump & Analysis](#4-full-ram-dump--analysis)
5. [Dual ISO](#5-dual-iso)
6. [FPS Override](#6-fps-override)
7. [crop_rec Module](#7-crop_rec-module)
8. [Audio Reverse Engineering](#8-audio-reverse-engineering)
9. [SD UHS Overclocking](#9-sd-uhs-overclocking)
10. [PTP Tunnel (ptptun)](#10-ptp-tunnel-ptptun)
11. [GPS, Defect & Touch Probes](#11-gps-defect--touch-probes)
12. [QEMU Emulation](#12-qemu-emulation)
13. [Code Cleanup & Dead Code Removal](#13-code-cleanup--dead-code-removal)
14. [Pre-Deployment Test Suite](#14-pre-deployment-test-suite)
15. [Companion Tools](#15-companion-tools)

---

## 1. Build System & Infrastructure

### The Problem
When we forked the 70D port, modules were built independently and manually copied to `modules/build/`. This meant:
- Changing a module's source code required remembering to rebuild and copy the `.mo` file
- The firmware build would silently use stale `.mo` files from previous builds
- Modules not in the global `ML_MODULES` list (like `hw_test`) required manual intervention

### The Fix
We added all 31 modules to the `ML_MODULES` list in `modules/Makefile`. The build system now:
1. Builds every module automatically when `make` runs
2. Copies fresh `.mo` files to `modules/build/`
3. Filters by `modules.included` to decide which go into the camera's `.zip`

### Stale .mo Prevention
The platform Makefile's `zip_contents` target was modified to clean the modules staging directory before copying:
```makefile
# Before each cam build, remove stale .mo files
rm -f $(CAM_MODULES_DIR)/*.mo $(CAM_MODULES_DIR)/*.sym
```
This prevents a module that was removed from `modules.included` from lingering in the build zip.

### Symbol File (70D_112.sym)
The symbol file maps 270+ ML functions to their runtime addresses. Modules use this to resolve external function calls via TCC at load time. Key entries include memory allocators (`__priv_malloc`, `fio_malloc`), EDMAC functions (`edmac_memcpy`, `edmac_raw_slurp`), ISO conversion (`raw2iso`, `split_iso`), and utility functions (`get_ms_clock`, `msleep`).

**Why this matters:** Without the correct symbol file, modules fail to load with TCC linker errors — exactly the `strncat` undefined symbol bug we hit.

---

## 2. Hardware Test Module (hw_test)

The hw_test module evolved through 27 versions from a simple 6-test proof-of-concept to a comprehensive 40-test diagnostic suite. It's gated behind a flag file (`ML/SETTINGS/HW_TEST.RUN`) and auto-deletes after running (one-shot design).

### Test Categories

#### Firmware Baseline (3 tests)
Verifies the correct camera model (`0x80000325 = Canon EOS 70D`), firmware version (1.1.2), and GUI state.

#### Memory System (6 tests)
Tests `fio_malloc` at 4KB and 64KB sizes, runs a 10× allocation stress test, and reports `GetFreeMemForMalloc` / `GetFreeMemForAllocateMemory`. The 70D has ~313KB free malloc pool and ~2.2MB free allocate memory pool.

#### File I/O (2 tests)
- **sd_write**: Creates, writes, and deletes files in `ML/SETTINGS/` — proves the card is writable
- **config_rw**: Write+read-back verification with 200ms FAT flush delay. The 50ms delay was too short — the SD card's FAT driver needed more time between close and open to flush directory entries

#### SD Benchmark (5 tests)
Measures write speed at 1K, 4K, 64K, 256K (single-block), and 256K×4 (multi-block). Baseline (no overclock): ~30KB/s at 1K, ~11MB/s at 256K×4. Higher presets (160MHz) reach ~70MB/s.

#### FPS/Timer Registers (1 test)
Dumps 11 timer-related registers:
- `FPS_CF` (0xC0F06000): Confirmation register
- `FPS_TA` (0xC0F06008): Timer A — row readout timing
- `FPS_TB` (0xC0F06014): Timer B — frame timing (broken on 70D)
- Various ENGIO mirror registers
- `ENGIO_HD3/HD4` (0xC0F0713C/150): Head register values

#### ENGIO/EDMAC/RAW Registers (3 tests)
Reads sensor interface registers:
- ENGIO top-left (0xC0F06800) and bottom-right (0xC0F06804)
- RAW_PHOTO_EDMAC (0xC0F04008), EDMAC write head (0xC0F04A08), LV RAW EDMAC (0xC0F26200)
- RAW processing: RAW_TYPE (0xC0F37014), SHAD_GAIN (0xC0F08030), PACK32_MODE (0xC0F08094)

#### Lossless/Display/Pipeline Registers (4 tests)
60+ MMIO registers across lossless compression (0xC0F373xx range), display palette (0xC0F140xx range), and image pipeline (0xC0F080xx/0xC0F0Exx ranges).

#### Dual ISO CMOS Tables (4 tests)
Dumps all 7 ISO values at both CMOS[0] and CMOS[3] offsets, for both photo (stride 20) and movie (stride 46) tables. CMOS[3] consistently shows 0x14 with flag bits cleared — confirming CMOS[0] is the active ISO register on 70D.

#### Audio IC Probe (1 test)
Tests 13 `call()` functions against the audio subsystem:
- Working: `SetAudioVolumeOut`, `ResetAudioIC`, `SendDataForAudioIC`, `DumpAudioIcRegister`, `EnableInternalMIC`, `EnableExternalMIC` (returns 1), `EnableHDMIAudio`, `TestSetAudioMic`, `TestSetAudioHeadPhone`
- Not found: `SetAudioVolumeIn`, `PowerMicAmp`, `PowerAudioOutput`, `InitializeAudioIC`

#### SD Clock Registers (2 tests)
Reads UHS registers (0xC0400600-0xC0400628) and GPIO registers (0xC022C634-0xC022C648). All return 0 after initialization — the SD host controller gates these off once the card is configured.

#### FPS Stability (1 test)
Samples FPS_TA 20 times over ~1 second. **Result: range=0, value=699 across all samples.** This confirms the hardware timing is deterministic — the Timer B banding issue is not caused by Timer A jitter.

#### Extended call() Dispatch (1 test)
Tests 10 additional call() functions:
- Working: `GetHDMIInfo` (returns 2010036), `FA_GetProperty`, `FA_GetPropertyAddress`, `SetAudioVolumeOut`, `TurnOffDisplay`
- Not found: `GetBatteryPerformance`, `GetBatteryTimeRemaining`, `GetTaskName`, `GetCurrentAvail`, `GetCFnData`

#### GPS Probe (info-only)
All GPS call() functions return -1 (NOT_FOUND) — confirmed GPS is not exposed via eventproc dispatch on 70D, even though `[GPS] GPS_Initialize` appears in the boot log.

#### Defect Probe (info-only)
All `ExecuteDefectMarge1-3` return -1 — defect management system exists in firmware but isn't accessible via call().

---

## 3. WiFi & Networking Stack

This was the most significant reverse engineering achievement of the project. We discovered that Canon's 70D firmware contains a **complete TCP/IP networking stack** with a DLNA/UPnP Media Server — all hidden in firmware and never exposed to the user.

### Socket API Discovery

The socket API functions are **RAM-loaded** by DryOS at boot, not fixed in ROM. They live at addresses 0x00059xxx in the RAM module space:

| Function | Address | Callers | Purpose |
|----------|---------|---------|---------|
| `socket_create` | 0x00059AF8 | 24 | Create socket (domain=1, type=1, protocol=0) |
| `socket_bind` | 0x00059E94 | 3 | Bind to local address |
| `socket_connect` | 0x00059DDC | 8 | Connect to remote host |
| `socket_listen` | 0x0005A9D0 | 9 | Listen for connections |
| `socket_recv` | 0x00059CE8 | 13 | Receive data |
| `socket_send` | 0x0005A09C | 30 | Send data (most widely used) |
| `socket_setsockopt` | 0x0005A810 | 47 | Set socket options |
| `socket_close` | 0xFF14F74C | 3 | Close socket (ROM1 — confirmed by callers) |

**Validation method:** Each address was checked for a valid ARM PUSH prologue (`0xE92Dxxxx`). All 7 RAM-loaded functions and 1 ROM1 function passed.

### PTPIP ROM1-Safe Wrappers

Canon provides error-handling wrappers around the raw socket API in ROM1 at 0xFF7AEE00-0xFF7AF500. These include logging, parameter validation, and proper cleanup:

**8 PTPIP NSTUBs added to stubs.S:**
- `ptpip_sock_create` (0xFF7AF220): socket_create + setsockopt REUSEADDR
- `ptpip_bind_param` (0xFF7AEE18): socket + bind + socket_close on error
- `ptpip_open_server` (0xFF7AEE80): Full TCP server setup
- `ptpip_create_client` (0xFF7AF2CC): TCP client from sockaddr struct
- `ptpip_listen_close` (0xFF7AEFCC): listen + close
- `ptpip_close_server` (0xFF7AF344): shutdown + listen + close
- `ptpip_set_keepalive` (0xFF7AF38C): setsockopt SO_KEEPALIVE
- `ptpip_errno_handler` (0xFF7AF3B4): error logging

Plus: `socket_close_if_valid` (0xFF7AF380) — safe close that checks fd ≠ -1.

### DLNA/UPnP Media Server Discovery

In the full 512MB RAM dump, we found Canon's hidden UPnP AV Media Server:

```xml
<root xmlns="urn:schemas-upnp-org:device-1-0">
  <deviceType>urn:schemas-upnp-org:device:MediaServer:1</deviceType>
  <friendlyName>EOS</friendlyName>
  <modelName>EOS</modelName>
  <presentationURL>/presentation.html</presentationURL>
</root>
```

Key details:
- DLNA DMS-1.50 certification
- ContentDirectory:1 and ConnectionManager:1 services
- Hardcoded IP: 192.168.1.20 (appears 25+ times in RAM)
- SDIO-based WLAN chip (Broadcom BCM43xx via `./WlanSdcom/WlanSDIODriver.c`)

### wifisrv Module (TCP Server)

The `wifisrv` module implements a TCP command server listening on port 5555:

**Protocol:** Single-byte command → 2-byte length prefix → response payload

**Commands:**
- `P` → "PONG" (ping/keepalive)
- `S` → Status: battery%, shutter count, temperature, LV state, remote shot flag
- `R` → Remote shutter via `call("FA_RemoteRelease")`
- `L` → Toggle LiveView via `call("FA_StartLiveView")/FA_StopLiveView"`
- `B` → Bootdisk mode
- `Q` → Disconnect

**Client: `camremote.py`** — Python3 script with persistent shell mode:
```
$ python3 camremote.py shell
> status
  Battery: 80%
  Shutter cnt: 12349
  ...
> shutter
  REMOTE_OK
```

### wifi_test Module

Discovery and validation module that probes all four WiFi API surfaces:
1. **call() initialization**: NwLimeInit, NwLimeOn, wlanpoweron, etc.
2. **RAM-loaded socket API**: creates socket, tests bind/listen/connect
3. **PTPIP ROM1-safe wrappers**: validates all 8 stubs
4. **NW command interface**: nif_up, nif_start, dhcpc_setup, ipset

**Key finding:** `NwLimeInit`/`NwLimeOn` return -1 on 70D — these are DIGIC 8 function names that don't exist on DIGIC V. Canon's 70D initializes WiFi differently (via WFT and DP subsystems at boot).

---

## 4. Full RAM Dump & Analysis

### The Dump Process
The `ramdump` module dumps 512MB (0x40000000–0x5FFFFFFF) to `ML/LOGS/RAM_FULL.BIN` in 256KB chunks. A safety probe skips unmapped pages (reads 0xFFFFFFFF). The process takes ~30-60 seconds and requires >50% battery.

### Analysis Results
From 509MB of actual data, we extracted:

**520+ callable functions** organized by subsystem:
- **Boot/Power:** EnableBootDisk, DisableBootDisk, EnableFirmware, Reboot, Format
- **LiveView/Sensor:** FA_StartLiveView, FA_StopLiveView, PauseLiveView, ResumeLiveView
- **WiFi/Network:** WLANSDIODRV_InitializeSDIODriver, all 7 socket functions, PTPIP wrappers
- **AF/Lens:** AfCtrl_Act_StartLensDriveRemote, EndLensDriveRemote, SetLensParameterRemote
- **Remote Shot:** schedule_remote_shot (0x0047db24), remote_shot (0x0047e00c)
- **File I/O:** FIO_OpenFile, CloseFile, ReadFile, WriteFile, CreateFile, RemoveFile
- **Factory/Adjustment (FA_*):** FA_SetProperty, FA_GetProperty, FA_ReadEepromData, FA_RemoteRelease, FA_MovieStart, FA_MovieEnd, FA_CreateTestImage, FA_DefectsTestImage
- **EDMAC/DMA:** StartEDmac, StopEDmac, ConnectReadEDmac, ConnectWriteEDmac, AbortEDmac
- **Memory:** fio_malloc, fio_free, AllocateMemoryResource, CreateMemorySuite
- **Property:** prop_register_slave, prop_deliver, prop_request_change
- **Audio:** SetAudioVolumeIn/Out, all ASIF DMA functions
- **GPS:** GPS_Initialize, GPSList, GPSTime
- **Touch:** TCH_CheckTouchICVersion, TCH_SetWaitingTime
- **Defect:** ExecuteDefectMarge1-5, FA_LvDetectDefects*

**270+ ML symbols** confirmed at runtime addresses — complete symbol table including memory allocators, EDMAC, ISO/exposure, menu/GUI, battery, audio, property handlers.

**12,639 unique strings** extracted including Canon's DLNA device descriptor, error messages across all 33 subsystems, source file paths, and property handler strings.

**30+ PROP_ IDs mapped** including WiFi connection targets, LV output devices, lens data, AFMA, HDMI, audio volume.

**55 Canon source file paths** identified:
- `./KernelDry/KerTask.c`, `KerSem.c` — DryOS kernel
- `./Memory/Memory.c`, `./PackMemory/PackHeap.c` — Memory allocator
- `./SensorDrive/SensorDrive.c`, `./Device/TG/TGdriver.c` — Sensor driver
- `./WlanSdcom/WlanSDIODriver.c`, `WlanSdcomDrv.c` — WiFi SDIO driver
- `./LvCommon/LvGainController.c`, `LvDefectController.c` — LiveView processing
- `./Epp/Vram/VramStage.c`, `./Epp/SsDevelop/SsDevelopStage.c` — Video pipeline
- `./Audio/AudioIC.c` — Audio IC driver

### RAM Layout
| Range | Content |
|-------|---------|
| 0x40000000–0x41000000 | ML code + data (~260K strings) |
| 0x41000000–0x44000000 | Canon structs, task data, GUI buffers |
| 0x44000000–0x4E000000 | AllocateMemory pool, ML_OBJS |
| 0x4E000000–0x60000000 | Sparse heap (0x55555555/0xAAAAAAAA) |

---

## 5. Dual ISO

### The Breakthrough
The Dual ISO module had been marked as "addresses wrong" because hw_test was using `shamem_read()` for RAM addresses. `shamem_read()` only works for MMIO registers (0xC0Fxxxxx). For general RAM, `MEM()` (direct pointer dereference) is required.

**3 CMOS ISO register tables found:**

| Location | Stride | Purpose |
|----------|--------|---------|
| 0x404e5664 | 20 bytes | Photo still mode |
| 0x404e5704 | 20 bytes | Photo mirror (alternate table) |
| 0x404e7248 | 20 bytes | LV/movie (base) |
| 0x404e77d6 | 46 bytes | Movie mode (primary) |

### ISO Value Encoding
Each table entry is a 16-bit value encoding two ISO fields and a flag:

```
bits[1:0] = flag (expected: 3 = active)
bits[4:2] = iso1
bits[7:5] = iso2
```

The 7 ISO stops:
| ISO | Value | Binary (flag:iso1:iso2) |
|-----|-------|-------------------------|
| 100 | 0x0003 | 00 0000 0011 |
| 200 | 0x0027 | 00 1001 0111 |
| 400 | 0x004b | 01 0010 1011 |
| 800 | 0x006f | 01 1011 1111 |
| 1600 | 0x0093 | 10 0100 0011 |
| 3200 | 0x00b7 | 10 1101 0111 |
| 6400 | 0x00db | 11 0110 1011 |

The linear pattern (each step adds 0x24 = 36, which is 9 per ISO field after flag removal) confirms proper encoding.

### Module Configuration
```c
// dual_iso.c 70D block
PHOTO_CMOS_ISO_START = 0x404e5664;
PHOTO_CMOS_ISO_COUNT = 7;
PHOTO_CMOS_ISO_SIZE  = 20;

FRAME_CMOS_ISO_START = 0x404e77d6;  // movie mode (was 0 — now enabled)
FRAME_CMOS_ISO_COUNT = 7;
FRAME_CMOS_ISO_SIZE  = 46;

CMOS_ISO_BITS = 3;
CMOS_FLAG_BITS = 2;
CMOS_EXPECTED_FLAG = 3;
```

Photo mode **verified working** on hardware — user reported Dual ISO menu accessible via ML → Expo tab, photos show expected alternating ISO banding pattern.

---

## 6. FPS Override

### Background
The 70D has two FPS timer registers:
- **Timer A** (0xC0F06008): Controls row readout timing
- **Timer B** (0xC0F06014): Controls frame timing

Timer B has a known issue on 70D — it causes vertical banding at non-standard frame rates. David_Hugh from the ML forum discovered that Timer A-only mode via "HiJello/FastTv" (fps_criteria=3) works around this.

### Our Findings
- TG_FREQ_BASE = 32,000,000 Hz (most cameras use 28,800,000)
- FPS_TA baseline = 699 (0x02BB) at 29.976 fps
- FPS_TB baseline = 1525 (0x05F5) at 29.976 fps
- **Range=0** across 20 samples over 1 second — Timer A is perfectly deterministic
- Timer B occasionally jitters ±1 tick (normal)

### The FPS Showdown
The `fps_criteria` menu offers four strategies:
1. Low Light (minimize ISO noise)
2. Exact FPS (precise frame rate)
3. Low Jello, 180d (minimize rolling shutter)
4. **HiJello, FastTv** (Timer A-only — recommended for 70D)

---

## 7. crop_rec Module

### Porting Effort
The crop_rec module was the most complex port. It had no 70D initialization block, no timer tables, and hundreds of hardcoded 5D3-specific register values.

### Timer Tables (TG_FREQ_BASE = 32MHz)
```c
// 70D-specific default timer A/B values
23.976fps: A=700,   B=1907
25fps:     A=800,   B=1600
29.97fps:  A=700,   B=1525
50fps:     A=800,   B=800
59.94fps:  A=672,   B=794
```

### Write Stubs
```c
CMOS_WRITE  = 0x26B54;   // RAM address of CMOS register write function
ADTG_WRITE  = 0x2684C;   // RAM address of ADTG register write function
ENGIO_WRITE = 0xFF2BC6C4; // ROM1 address of ENGIO register write function
```

### Presets Available
- 1080p (1920×1080) — safe default, least aggressive
- 3K (3072×1536) — moderate crop
- UHD (3840×1920) — high resolution
- 4K_HFPS (4096×2160) — maximum resolution
- 1:1 — square crop, no line-skipping
- 3X (3× zoom) — tightest crop
- CENTER_Z — center crop with zoom
- 3X_TALL — 3× crop in tall orientation

### Remaining Calibration
CMOS[2], CMOS[6], CMOS[7] values are all copied from 5D3 trial-and-error. These need hardware verification with visual artifact inspection. The 70D sensor is 5472×3648 vs 5D3's 5796×3870 — all windowing values need scaling.

---

## 8. Audio Reverse Engineering

### ASIF DMA Functions (ROM1)
All core audio DMA stubs located in ROM1:

| Function | Address | Purpose |
|----------|---------|---------|
| `StartASIFDMAADC` | 0xFF1172E0 | Start ADC DMA |
| `StopASIFDMAADC` | 0xFF11758C | Stop ADC DMA |
| `StartASIFDMADAC` | 0xFF1176B4 | Start DAC DMA |
| `StopASIFDMADAC` | 0xFF117934 | Stop DAC DMA |
| `SetNextASIFADCBuffer` | 0xFF117DFC | Queue ADC buffer |
| `SetNextASIFDACBuffer` | 0xFF117FE4 | Queue DAC buffer |
| `sounddev_task` | 0xFF118F5C | Audio task entry |
| `SoundDevActiveIn` | 0xFF11936C | Activate audio input |
| `SetAudioVolumeIn` | 0xFF11970C | Input volume control |

### Audio IC Investigation
We found the `./Audio/AudioIC.c` source path in the RAM strings, confirming Canon has a dedicated audio IC driver. Functions like `ResetAudioIC`, `SendDataForAudioIC`, and `DumpAudioIcRegister` all return 0 (exist). But the audio IC model (AK4646? WM8731? something else?) is never exposed in firmware strings — it requires an I2C register probe to identify.

### CONFIG_AUDIO_CONTROLS
**No DIGIC V camera** has this enabled (70D, 5D3, 6D all commented out). The `audio-ak.c` code assumes AK4646 registers, which may not match the 70D's actual audio IC. Enabling without identification risks hardware damage.

---

## 9. SD UHS Overclocking

### Register Map
The sd_uhs module controls SD clock speed through 9 UHS registers (0xC0400600-0xC0400628):

| Index | Address | Purpose |
|-------|---------|---------|
| [0] | 0xC0400600 | Clock divider/phase |
| [1] | 0xC0400604 | Clock divider/phase |
| [2] | 0xC0400610 | Clock divider/phase |
| [3] | 0xC0400614 | Master config (0x1D000301/0x1D000001) |
| [4] | 0xC0400618 | Clock divider |
| [5] | 0xC0400624 | Timing control |
| [6] | 0xC0400628 | Timing control |
| [7] | 0xC040061C | Timing control |
| [8] | 0xC0400620 | Clock divider |

### 70D-Specific Behavior
- **160MHz1 is broken** — behaves like 192MHz on 70D. Force-mapped to 160MHz2.
- **GPIO registers (0xC022C6xx) are NOT used** by 70D (confirmed by hw_test — all read 0x00000000)
- **Stability registers (0xC0400450/454/46C) are NOT used** by 70D (5D3 only)
- **Menu restricted** to OFF/160MHz only. 192/240MHz documented as unstable.
- Write clock hook uses `sdDMAWriteBlk`, not `sdWriteBlk` (patching sdWriteBlk causes CACHE_COLLISION on 70D)

### Baseline Performance (no overclock)
| Block Size | Write Speed |
|-----------|-------------|
| 1K | ~30 KB/s |
| 4K | ~200 KB/s |
| 64K | ~1.6 MB/s |
| 256K | ~7 MB/s |
| 256K×4 | ~11 MB/s |

At 160MHz, speeds reach ~70 MB/s.

---

## 10. PTP Tunnel (ptptun)

### Architecture
The ptptun module implements a USB remote control tunnel over Canon's PTP protocol at opcode 0xA1E9. It registers a PTP handler via `ptp_register_handler()` and communicates over USB with a host-side Python3 script.

### Tunnel Commands
| ID | Command | Purpose |
|----|---------|---------|
| 0 | CallByName | Execute any firmware function by name |
| 1 | ConsoleCapture | Capture debug console output |
| 2 | Screenshot | Capture LCD screenshot to file |
| 3 | ExecuteLua | Run Lua script from ML/SCRIPTS/ |
| 4 | EngioRead | Read ENGIO register |
| 5 | EngioWrite | Write ENGIO register |
| 6 | ShamemRead | Read MMIO/shamem register |
| 7 | SetPropertyRaw | Set Canon property by ID |

### camtunnel.py
Host-side Python3 script (uses pyusb) that provides both interactive shell and command-line interface. Supports both the PTP tunnel protocol (0xA1E9) and the CHDK protocol (0x9999) for backwards compatibility.

---

## 11. GPS, Defect & Touch Probes

### GPS
The boot log confirms GPS initialization:
```
[FM] [GPS] GPS_Initialize (36, 25)
```
Functions exist in firmware: `GPSList`, `GPSTime`, `GPSCaptureTimeList`, `GPSClearList`, `GPS_RegisterSpaceNotifyCallback`. However, all `call()` attempts return -1 — GPS functions are not exposed via eventproc dispatch on 70D.

### Defect Management
Canon's defect/pixel management system is extensive:
- `ExecuteDefectMarge1-5` — merge different defect maps
- `FA_LvDetectDefectsFull/Magnify/MovieCrop` — LV-specific defect detection
- `FA_DefectsTestImage`, `FA_CreateTestImage` — test image generation
- `sht_savedefectsproperty` — persistent defect storage

These functions exist but are not accessible via `call()` on 70D. They're likely called internally by Canon's LiveView pipeline.

### Touchscreen
Full TCH (touch controller hub) API:
`TCH_CheckTouchICVersion`, `TCH_SetWaitingTime`, `TCH_SetMutualGainValue`, `TCH_SetGainParamForSelfScan`

The 70D touchscreen only supports single-finger events (confirmed by `gui.h`: "NO GUI EVENTS: two finger touch unavailable on this camera").

---

## 12. QEMU Emulation

### ROM1 Size Bug
The 70D's ROM1 was incorrectly set to 8MB in both QEMU and ML. All DIGIC V cameras use 16MB ROM1. Fixing this to 16MB enabled full firmware boot.

### MPU Spells
The 70D's MPU (Main Processing Unit) communication required 5 additional property groups:
- PROP_AF_MICROADJUSTMENT — AF fine-tune data
- PROP_LV_LENS — LiveView lens info (critical for focus confirmation)
- PROP_CONTINUOUS_AF_VALID — Dual Pixel AF state
- PROP_ROLLING_PITCHING_LEVEL — Electronic level
- PROP 80050034 — Unknown (present in 5D3, 700D, 60D, 600D)

### Boot Verification
```
startupInitializeComplete at ~576ms
ML GUI at ~608ms
0 unknown MPU messages
```

### 15 Enhancement Tasks
Module loading, BMP frame capture, SD card I/O tests, property system testing, config file validation, task scheduling, timer callbacks, menu navigation, display visualization, performance benchmarking, memory leak detection, log analysis, regression tracking.

---

## 13. Code Cleanup & Dead Code Removal

### Sprint 12 — First Purge
Removed 318 lines of `#if 0` dead code across the codebase:
- stdio.c: unused streq implementation
- tasks.c: debug stack checking, BMP lock debugging
- focus.c: dead focus stacking menu entries
- menu.c: dead BGMT_PLAY case, bubbles hack
- powersave.c: NotifyBox debug call
- cropmarks.c: draw_cropmark_area tests
- rbf_font.c: tab width fix
- module.c: TCC section debug logging

### Sprint 13 — Second Purge
Deleted `bitrate-6d.c` (656 lines) and removed 420+ more dead lines:
- minimal-d678.c: memory scanning + LiveView experiments
- log-d678.c: MPU message logging
- fio-ml.c: CF card info display
- exmem.c: exmem_test()
- tskmon.c: older camera workarounds
- debug.c: empty test hooks
- raw.c: bad frame DNG debug code
- mem.c: RscMgr allocator entries
- audio-common.c: audio gain display
- zebra-5dc.c: spotmeter erase code

Build size impact: 444KB → 436KB (8KB saved).

### Stale Probe Module Deletion
Three probe modules (`gpsprobe`, `defectprobe`, `diprobe`) were created as separate modules then later integrated into hw_test. The source directories were deleted after the integration was verified.

---

## 14. Pre-Deployment Test Suite

### Symbol Check (`make check-symbols`)
For each `.mo` module, extracts undefined symbols via `readelf` and checks them against:
1. The camera's `70D_112.sym` file (ML core exports)
2. All GLOBAL symbols from ALL built `.mo` files (cross-module resolution)

Catches TCC linker errors before they reach the camera. Detected issues like:
- `strncat` not in the symbol table (ML's minimal vsnprintf doesn't support `%u`)
- `strncpy` not in the symbol table (missing `#include <string.h>`)

### Syntax Check
Validates `camremote.py` and `camtunnel.py` with `python3 -m py_compile`.

### Build Size Check
Ensures `autoexec.bin` is under 656KB (current: 456KB, safety margin: 199KB).

---

## 15. Companion Tools

### camremote.py
WiFi TCP remote control for the wifisrv module:
- One-shot commands: `python3 camremote.py status`, `shutter`, `lv`, `ping`
- Persistent shell: `python3 camremote.py shell`
- Connection: TCP to camera IP:5555

### camtunnel.py
USB PTP tunnel for the ptptun module:
- Uses pyusb to communicate over PTP protocol
- Supports both tunnel (0xA1E9) and CHDK (0x9999) protocols
- Commands: CallByName, Screenshot, ExecuteLua, EngioRead/Write, ShamemRead
- Configurable camera VID/PID (default: 0x04A9/0x3277)

---

## Appendix: Boot Log Analysis

The 70D boot sequence revealed in the serial log:

```
[STARTUP] startupEntry → startupPrepareProperty
[RTC] RTC_Permit → SND Seq LPC fin
[RSC] MemoryStatusMasterResultCBR
[FM] FM_RegisterSpaceNotifyCallback (×2)
[MRK] MRK_RegisterSpaceNotifyCallback
[GPS] GPS_RegisterSpaceNotifyCallback
[JOB] InitializeJobClass (13 job classes)
[ENG] ENGIO mapping at 0x41700000
[MC] PROP_GUI_STATE 0
[MC] PROP_VARIANGLE_GUICTRL : Enable
[MAC] MAC_Initialize (board ID validation)
[SHTP] RegRPCHandler Master
[MR] TimeCode Mode:1
[WFT] InitializeAdapterControl END
[RMT] QuickTransmissionEnable / PROP_CONNECT_TARGET
[PTPCOM] SetPtpTransportResources:0,3253
[SD] sdSendOCR → Set Hi-Speed Mode (96MHz)
[DP] DP_Initialize() → DpsTerminate() (WiFi not connected)
[HDMI] DisconnectHDMI : Not Connected
[H264E] InitializeH264EncodeFor1080pDZoom
[TCH] Active_Mode SetEnd
[CAPD] LvdsCalclkCtrlMaster / ImagePowerOn
[LVEVF] ShootingMode(2), FrameRate(5)
[STARTUP] startupInitializeComplete (~600ms)
```

Key observations:
- WiFi initialization (WFT/DP/RMT/PTPCOM) happens automatically even without WiFi enabled
- `PROP_WIFI_SETTING` is 0 (off) at boot, only changes to 1 when user enables WiFi
- SD card runs at 96MHz Hi-Speed mode (UHS-I not SDR50)
- H.264 encoder initializes for 1080p and 1080p25fps DZoom simultaneously

---

*For build instructions, quick start, and current status, see [README.md](README.md).*  
*For the complete commit-by-commit project history, see [CHANGELOG.md](CHANGELOG.md).*  
*For sprint planning and remaining work, see [TODO.md](TODO.md).*
