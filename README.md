# Magic Lantern 70D - Canon EOS 70D Development

[![Status](https://img.shields.io/badge/status-Software%20Complete-brightgreen)](https://github.com/peva3/magiclantern_70D)
[![Camera](https://img.shields.io/badge/camera-Canon%2070D-red)](https://github.com/peva3/magiclantern_70D)
[![Firmware](https://img.shields.io/badge/firmware-1.1.2-green)](https://github.com/peva3/magiclantern_70D)

Magic Lantern is a firmware enhancement for Canon DSLR cameras that adds video and photo features not included by the manufacturer. This repository is specifically for the **Canon 70D (DIGIC V, firmware 1.1.2)**.

## Quick Start

### Installation (For 70D Owners)
1. Format SD card (FAT32, 32GB or smaller recommended)
2. Copy `autoexec.bin` and `ML/` folder to card root
3. Boot camera while holding PLAY button
4. Magic Lantern menu will appear in Canon's menu system

**⚠️ Warning:** This is development software. Use at your own risk. Always backup your settings.

## Current Status

**Phase:** All non-hardware software complete — awaiting physical 70D testing  
**Build:** 452KB autoexec.bin (656KB limit), 25+ modules  
**QEMU:** Full boot verified — `startupInitializeComplete` at ~595ms, 0 unknown MPU messages  
**Last updated:** 2026-04-28

### What's Implemented (Software Complete)
- ✅ **QEMU 70D emulation** — Full boot, ML GUI, 0 MPU errors, auto SD image generation
- ✅ **crop_rec** — 70D timer tables (TG_FREQ_BASE=32MHz), 8 crop presets, CMOS/ADTG/ENGIO hooks
- ✅ **MLV v3 port** — raw_vidx enabled, 1280x720 crop for 40MB/s SD limit
- ✅ **WiFi discovery** — All socket addresses found (RAM-loaded 0x0005xxxx), 8 PTPIP ROM1-safe NSTUBs
- ✅ **FPS override** — Timer A-only (HiJello/FastTv mode), QEMU boot confirmed
- ✅ **Focus confirmation** — Via PROP_LV_LENS focus_pos with stability detection
- ✅ **Audio RE** — All 14 ASIF stubs active, SetAudioVolumeIn verified at 0xFF11970C
- ✅ **RAW zebras root cause** — Dual Pixel CMOS AF pixels identified as cause
- ✅ **Dual ISO** — Photo mode working; movie mode pipeline documented
- ✅ **sd_uhs** — 160MHz preset for 70D with safety warnings
- ✅ **hw_test module** — Automated hardware diagnostic framework (3.2KB)
- ✅ **wifi_test module** — Socket/PTPIP discovery framework (4.4KB)
- ✅ **Code cleanup** — 12+ sprints of dead code removal, config consolidation

### Known Issues (Software Verified)
- 🔴 **RAW Zebras** — Intentionally disabled; Dual Pixel AF pixels cause false readings
- 🔴 **Audio Controls** — CONFIG_AUDIO_CONTROLS commented out; codec type unknown (not AK4646?)
- 🟡 **FPS Timer B** — Causes vertical banding; Timer A-only workaround recommended
- 🟡 **Level indicator** — Freezes after ~1 min in LV; press INFO to reset
- 🟡 **FlexInfo** — Bottom bar flickers

### Hardware Calibration Still Needed
| Component | Status | Why |
|-----------|--------|-----|
| CMOS registers | 🔲 Needs hardware | All values copied from 5D3 |
| ENGIO registers | 🔲 Needs hardware | Top-bar/end-column values uncalibrated |
| CROP_PRESET_3X | 🔲 Needs hardware | ENGIO override commented out (corrupted image) |
| Wireless | 🔲 Needs hardware | Socket/PTPIP stubs need runtime verification |
| Audio codec | 🔲 Needs hardware | AK4646 register map may not match 70D |

## Repository Structure

```
magiclantern_70D/
├── AGENTS.md                # Technical architecture (60+ pages)
├── TODO.md                  # Development sprint planning
├── 70d-latest/              # Deployment folder (latest autoexec.bin)
├── platform/70D.112/        # 70D-specific code
├── src/                     # Core Magic Lantern source
├── modules/                 # Module source code (25+ modules)
└── qemu-eos/                # QEMU emulator submodule
```

## Building

### Prerequisites
```bash
# ARM cross-compiler
sudo apt-get install gcc-arm-none-eabi

# QEMU dependencies (for emulation)
sudo apt-get install qemu-system-arm
```

### Build 70D
```bash
make -j4 70D
# Output: build/70D/autoexec.bin
```

### Run Tests
```bash
./test_70d_qemu.sh --boot --no-build --timeout 30
```

## Getting Involved

### For 70D Owners
- Your camera is needed for final calibration!
- See `HARDWARE-TESTING.md` for the complete testing checklist
- Test builds in `70d-latest/autoexec.bin`

### For Developers
1. Read [AGENTS.md](AGENTS.md) for architecture
2. Check [TODO.md](TODO.md) for remaining tasks (hardware-required)
3. All non-hardware work is complete — next steps need physical 70D

## Roadmap

### Completed (All Software Work)
- Sprints 0-23 covering: Foundation, Focus, FPS, Zebras, crop_rec, Audio, WiFi, MLV v3
- QEMU emulation with full MPU spell coverage
- All socket/WiFi addresses discovered and documented

### Pending (Hardware Required)
1. **CMOS/ENGIO calibration** for crop_rec (S5.5-S5.9)
2. **WiFi verification** of PTPIP/socket stubs (S23.16+)
3. **Audio codec identification** + CONFIG_AUDIO_CONTROLS (S8.2+)
4. **Dual ISO movie mode** investigation (S6)
5. Focus stacking fix (S2.4), FPS banding (S3), SD UHS tuning (S9)

## Related Projects

- **Upstream:** [magiclantern_simplified](https://github.com/reticulatedpines/magiclantern_simplified)
- **QEMU:** [qemu-eos](https://github.com/reticulatedpines/qemu-eos)
- **Main ML:** [magiclantern.fm](http://www.magiclantern.fm/)

## License

Magic Lantern is released under the GPL v3 license. See [COPYING](COPYING) for details.

## Disclaimer

Magic Lantern is not affiliated with Canon Inc. Use at your own risk. We are not responsible for any damage to your camera or images.

---

**Last Updated:** 2026-04-28  
**Current Phase:** All non-hardware work complete — awaiting physical 70D  
**Next Milestone:** Hardware calibration of crop_rec CMOS/ENGIO registers
