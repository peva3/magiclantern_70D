# Magic Lantern 70D - Canon EOS 70D Development

[![Status](https://img.shields.io/badge/status-Week%201%20Foundation-blue)](https://github.com/peva3/magiclantern_70D)
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

**Phase:** Week 1 - Foundation Setup  
**Build Status:** Reference build available, source build in progress  
**Testing:** QEMU emulation setup in progress  

### What Works (Reference Build - June 2025)
✅ RAW video recording (MLV Lite)  
✅ Dual ISO photography (still mode only)  
✅ Crop recording (basic 3x3_1X preset)  
✅ ETTR (Expose To The Right)  
✅ Lua scripting (11 scripts included)  
✅ 19 modules loaded and functional  

### Known Issues
🔴 **No LV_FOCUS_DATA** - Focus confirmation disabled  
🔴 **FPS Override broken** - Cannot change frame rates  
🔴 **RAW Zebras disabled** - Causes display corruption  
🔴 **Dual ISO movie mode broken** - Photo mode only  
🟡 **Audio controls missing** - Cannot adjust gain  
🟡 **FlexInfo flickering** - Bottom bar unstable  

## Development Progress

### Week 1 Tasks (Current Week: 2026-04-22)
- [x] Analyze reference build structure
- [x] Create comprehensive documentation (AGENTS.md, FUTURE-WORK.md, TODO.md)
- [x] Set up testing framework documentation
- [ ] Install ARM toolchain and verify build
- [ ] Configure QEMU for 70D emulation
- [ ] Create first host-side test suite

### Completed
- ✅ Repository created and cloned
- ✅ Reference build analyzed (19 modules, 11 scripts)
- ✅ Documentation complete (4 technical docs)
- ✅ Testing framework documented

### In Progress
- 🔧 Build environment setup
- 🔧 QEMU 70D configuration
- 🔧 Host-side test framework

### Next Up
- Build autoexec.bin from source
- Run first QEMU emulation
- Start LV_FOCUS_DATA investigation

## Repository Structure

```
magiclantern_70D/
├── AGENTS.md              # Technical architecture documentation
├── FUTURE-WORK.md         # Improvement roadmap (20 items)
├── TODO.md                # Development sprint planning
├── TESTING_FRAMEWORK.md   # Testing without physical camera
├── NEXT_STEPS.md          # Immediate action plan
├── 70d-dev/               # Reference build (June 2025)
│   ├── autoexec.bin
│   ├── ML-SETUP.FIR
│   └── ML/
│       ├── modules/       # 19 .mo modules
│       └── scripts/       # 11 Lua scripts
├── platform/70D.112/      # 70D-specific code
├── src/                   # Core Magic Lantern source
├── modules/               # Module source code
└── qemu-eos/              # QEMU emulator submodule
```

## Testing Approach

Since we don't have physical 70D access, we use a multi-layered testing strategy:

1. **QEMU Emulation** - ARM emulation of DIGIC V hardware
2. **Host-Side Compilation** - Test logic on x86/Linux with stubs
3. **Simulation Framework** - Mock camera state for algorithms
4. **Remote Testing** - Collaboration with 70D owners for field validation

See [TESTING_FRAMEWORK.md](TESTING_FRAMEWORK.md) for details.

## Getting Involved

### For Developers
1. Read [AGENTS.md](AGENTS.md) for architecture
2. Check [TODO.md](TODO.md) for current tasks
3. Follow development requirements in AGENTS.md
4. Test changes via QEMU or host-side before commit

### For 70D Owners
- Test builds will be posted as releases
- Feedback needed on focus features, FPS override, RAW zebras
- See [NEXT_STEPS.md](NEXT_STEPS.md) for testing opportunities

## Documentation

| Document | Description |
|----------|-------------|
| [AGENTS.md](AGENTS.md) | Technical architecture, registers, memory maps |
| [FUTURE-WORK.md](FUTURE-WORK.md) | 20 improvement items with severity ratings |
| [TODO.md](TODO.md) | 10-sprint development roadmap |
| [TESTING_FRAMEWORK.md](TESTING_FRAMEWORK.md) | Testing without physical camera |
| [NEXT_STEPS.md](NEXT_STEPS.md) | Immediate action plan (weeks 1-16) |
| [70d-dev/README.md](70d-dev/README.md) | Reference build analysis |

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
cd tests
make
./run_all_tests.sh
```

## Reference Build

The `70d-dev/` directory contains a working build from June 20, 2025:
- **autoexec.bin** (437 KB)
- **19 modules** (dual_iso, mlv_lite, crop_rec, etc.)
- **11 Lua scripts** (api_test, pong, sokoban, etc.)
- **Known working** on Canon 70D firmware 1.1.2

## Roadmap

### Phase 1: Foundation (Weeks 1-2)
- Build environment setup
- QEMU 70D configuration
- Host-side test suite

### Phase 2: Core Features (Weeks 3-8)
- LV_FOCUS_DATA discovery
- FPS override implementation
- RAW zebras fix

### Phase 3: Advanced (Weeks 9-16)
- Crop recording extension (1:1, 3K, 4K)
- Dual ISO movie mode
- MLV v3 port (raw_vidx)

See [TODO.md](TODO.md) for detailed sprint planning.

## Related Projects

- **Upstream:** [magiclantern_simplified](https://github.com/reticulatedpines/magiclantern_simplified)
- **QEMU:** [qemu-eos](https://github.com/reticulatedpines/qemu-eos)
- **Main ML:** [magiclantern.fm](http://www.magiclantern.fm/)

## License

Magic Lantern is released under the GPL v3 license. See [COPYING](COPYING) for details.

## Disclaimer

Magic Lantern is not affiliated with Canon Inc. Use at your own risk. We are not responsible for any damage to your camera or images.

---

**Last Updated:** 2026-04-22  
**Current Sprint:** Week 1 - Foundation Setup  
**Next Milestone:** Build autoexec.bin from source
