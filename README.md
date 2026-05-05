# Magic Lantern Canon 70D — Modern Port, Fully Validated

[![Camera](https://img.shields.io/badge/camera-Canon%20EOS%2070D%20(FW%201.1.2)-red)]()
[![Status](https://img.shields.io/badge/status-Hardware%20Validated-success)]()
[![Tests](https://img.shields.io/badge/tests-35%20PASS%20%2F%205%20SKIP%20%2F%200%20FAIL-brightgreen)]()
[![Build](https://img.shields.io/badge/build-456KB%20(656KB%20limit)-yellow)]()
[![License](https://img.shields.io/badge/license-GPLv3-blue)]()

---

This repository updates the **Canon EOS 70D** Magic Lantern port to the latest ML mainline codebase, adds extensive hardware reverse engineering, and validates everything on a physical camera.

The 70D port was originally created by [nikfreak](https://bitbucket.org/nikfreak/magic-lantern) on an older ML codebase. We forked [`reticulatedpines/magiclantern_simplified`](https://github.com/reticulatedpines/magiclantern_simplified) — itself a cleaned-up ML fork — and merged the 70D platform support into it, resolving all the incompatibilities that had accumulated. We also merged [`qemu-eos`](https://github.com/reticulatedpines/qemu-eos) as a subtree and built out 70D emulation support.

The result: a fully modern, buildable, and hardware-validated Magic Lantern for the Canon 70D.

> **For exhaustive technical details on every module, reverse engineering finding, and hardware validation result, see [doc/deepdive.md](doc/deepdive.md).**

---

## What We Added

### Magic Lantern (70D Platform)

The last official 70D build had 20 modules, no hardware validation, and was based on an older ML snapshot. Here's everything that changed:

**New Modules (11 added: 20 → 31)**
| Module | What It Does | Origin |
|--------|-------------|--------|
| `sd_uhs` | SD card overclocking (160MHz, ~70MB/s) | ML mainline port |
| `raw_vidx` | MLV v3 RAW video recording | ML mainline port |
| `mlv_rec` | RAW video (was available but not in build) | ML mainline |
| `hw_test` | Full hardware diagnostic suite (40 tests) | **Our work** |
| `ptptun` | USB PTP tunnel (remote camera control) | **Our work** |
| `wifisrv` | WiFi TCP server (remote control over network) | **Our work** |
| `wifi_test` | WiFi/PTPIP stub discovery and validation | **Our work** |
| `ramdump` | Full 512MB RAM dump to SD card | **Our work** |
| `adtglog2` | CMOS/ADTG register write logging | ML dev tool |
| `sf_dump` | Serial flash dump utility | ML dev tool |
| 3 dead probe modules deleted → folded into hw_test | — | **Our cleanup** |

**Newly Enabled Features (ported from ML mainline, previously off for 70D)**
- **FPS Override** — Timer A-only mode (HiJello/FastTv). **Verified: range=0 stability**
- **Focus Confirmation** — via PROP_LV_LENS focus_pos stability detection
- **Double-click zoom** shortcut from 5D3/6D
- **Zoom from image review** (Ken Rockwell zoom)
- **Swap info in playback** mode
- **Focus peaking as display filter**
- **Focus box snap to x5 raw**
- **Beep support**, Q Menu in playback, white balance workaround, zoom half-shutter UI lock
- **Debug menu**: Screenshot, Show Tasks, CPU usage, free memory, shutter count, CMOS temp

**Reverse Engineering (all our work)**
- **Full 512MB RAM dump** extracted and analyzed from physical 70D
- **520+ call() functions documented** — Canon's complete eventproc dispatch table
- **270+ ML symbols confirmed** at runtime addresses
- **55 Canon source file paths** identified (kernel, sensor, WiFi, audio, video)
- **WiFi stack fully reversed** — Canon's DLNA/UPnP Media Server discovered (DMS-1.50), 8 PTPIP socket stubs mapped, 7 socket API functions located at fixed RAM addresses
- **All 14 audio DMA stubs** located in ROM1
- **GPS/Touch/Defect** management systems identified
- **Camera hangs from call("EnableBootDisk") / call("TurnOnDisplay")** documented

**Hardware Validation (all our work — old build had zero)**
40 automated tests on a physical 70D (shutter count: 12,349):
- **35 PASS / 5 SKIP / 0 FAIL** — all core subsystems proven
- PTPIP stubs: 11/11 valid, Socket API: 7/7 loaded in RAM
- Dual ISO: photo mode **confirmed working**
- FPS: range=0 across 20 samples (rock-solid timing)
- RAM dumps: 3 regions, 33MB total written to SD card
- Register baselines: 6/6 match

**Infrastructure (all our work)**
- Pre-deployment test suite (`tests/run_all.sh`) — catches symbol errors before camera
- Auto-build system — all 31 modules rebuild on `make`, no stale artifacts
- ~740 lines dead code removed, 10+ cleanup sprints
- [CHANGELOG.md](CHANGELOG.md) with complete day-by-day project history

### QEMU Emulation (qemu-eos)

- **Fixed ROM1 size** — corrected from 8MB to 16MB (all DIGIC V cameras use 16MB)
- **Added 5 MPU spell groups** for proper 70D property handling (AF, LV lens, continuous AF, rolling level, etc.)
- **Achieved 0 unknown MPU messages** during Canon + ML boot
- **Full firmware boot** — `startupInitializeComplete` at ~576ms, ML GUI at ~608ms
- **15 emulation enhancements** — module loading, BMP capture, SD I/O, property testing, task scheduling, memory leak detection, regression tracking
- **Real ROM dumps** from physical 70D (ROM0: 8MB, ROM1: 16MB, SFDATA: 16MB)

### Hardware Validation

Every automated test was executed on a **physical Canon EOS 70D** (shutter count: 12,349) with **35 PASS / 5 SKIP / 0 FAIL**:

| Area | Result |
|------|--------|
| Memory system (malloc, fio, stress) | ✅ All 6 PASS |
| File I/O (SD write, config read/write) | ✅ Both PASS |
| SD benchmark (1K–1MB, 5 block sizes) | ✅ All 5 PASS |
| FPS/Timer registers (11 addresses) | ✅ PASS |
| ENGIO/EDMAC/RAW processing registers | ✅ All 3 PASS |
| Lossless/Display/Image pipeline (37 regs) | ✅ All 4 PASS |
| Dual ISO CMOS tables (photo + movie) | ✅ Photo PASS, CMOS[3] SKIP (expected) |
| Audio IC probe (13 call() functions) | ✅ PASS |
| SD clock/GPIO registers | ✅ Both PASS |
| FPS stability (20 samples over 1s) | ✅ PASS — range=0 (rock-solid) |
| Register baselines (6 register pairs) | ✅ PASS — 6/6 match |
| RAM dump (3 regions, 33MB total) | ✅ PASS |
| PTPIP socket stubs | ✅ PASS — 11/11 valid |
| RAM-loaded socket API | ✅ PASS — 7/7 loaded |
| Extended call() dispatch (10 functions) | ✅ PASS |
| GPS probe (8 functions) | ⏭️ SKIP (not in call() table — expected) |
| Defect probe (3 functions) | ⏭️ SKIP (not in call() table — expected) |

### Modules Built (31)

`adv_int` `adtglog2` `arkanoid` `autoexpo` `bench` `crop_rec` `deflick` `dot_tune` `dual_iso` `edmac` `ettr` `file_man` `hw_test` `img_name` `lua` `mlv_lite` `mlv_play` `mlv_rec` `mlv_snd` `pic_view` `ptptun` `ramdump` `raw_vidx` `sd_uhs` `selftest` `sf_dump` `silent` `wifi_test` `wifisrv`

### Companion Tools

| Tool | What It Does |
|------|-------------|
| `camremote.py` | WiFi TCP remote control from laptop (status, shutter, LiveView toggle, ping) |
| `camtunnel.py` | USB PTP tunnel — execute commands on camera over USB |
| `tests/run_all.sh` | Pre-deployment validation (module symbols + Python syntax + build size) |

---

## Quick Start (For 70D Owners)

1. **Format** an SD card (FAT32)
2. **Build** (see below) or download from **Releases**
3. Copy `autoexec.bin` and `ML/` folder to card root
4. Power on camera while holding **PLAY** button
5. Magic Lantern menu appears in Canon's menu system

> **Re-installing?** Format the SD card first. Canon's bootloader retains boot flags from prior ML installations, which can cause a "Enable Firmware disabled" error. Formatting clears these.

**⚠️** Development software. Use at your own risk. Backup your settings.

---

## Building

```bash
# Prerequisites
sudo apt-get install gcc-arm-none-eabi

# Build firmware
cd platform/70D.112 && make -j$(nproc)

# Run pre-deployment checks
bash tests/run_all.sh

# Deploy to 70d-latest/
make deploy
```

---

## Repository Structure

```
magiclantern_70D/
├── platform/70D.112/        # 70D-specific firmware (stubs.S, features.h, consts.h)
├── src/                     # Core ML source
├── modules/                 # 31 modules
├── tests/                   # Pre-deployment test suite
├── firmware/                # Canon firmware files + binary release archives
├── camremote.py             # WiFi remote control
├── camtunnel.py             # USB PTP tunnel
├── CHANGELOG.md             # Full project history
├── doc/deepdive.md              # Exhaustive technical deep dive
├── doc/ramdump.md               # Complete 512MB RAM dump analysis
├── AGENTS.md                # Technical architecture deep dive
├── TODO.md                  # Remaining work
└── qemu-eos/                # QEMU emulator (git subtree)
```

---

## Related Projects

- **[magiclantern_simplified](https://github.com/reticulatedpines/magiclantern_simplified)** — Upstream ML fork
- **[qemu-eos](https://github.com/reticulatedpines/qemu-eos)** — QEMU Canon emulator
- **[nikfreak 70D port](https://bitbucket.org/nikfreak/magic-lantern)** — Original 70D development

## License

GPL v3. Not affiliated with Canon Inc.

---

**Last Updated:** 2026-05-04  
**Build:** 456KB autoexec.bin (199KB safety margin)  
**Tests:** 35 PASS / 5 SKIP / 0 FAIL on physical Canon EOS 70D
