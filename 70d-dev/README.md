# Canon 70D Development Build Analysis

This directory contains a working Magic Lantern development build for the Canon 70D (firmware 1.1.2), dated June 20, 2025.

## Build Contents

### Core Files
- **autoexec.bin** (437 KB) - Main Magic Lantern bootloader
- **ML-SETUP.FIR** (33 KB) - Firmware installer
- **magiclantern.2025-06-20.70D.112.zip** - Complete distribution package

### Included Modules (19 total)
1. `adv_int.mo` - Advanced intervalometer
2. `arkanoid.mo` - Arkanoid game
3. `autoexpo.mo` - Auto exposure bracketing
4. `bench.mo` - Benchmarking tools
5. `crop_rec.mo` - Crop recording (basic 3x3_1X only)
6. `deflick.mo` - Deflicker
7. `dot_tune.mo` - DotTune AF microadjustment
8. `dual_iso.mo` - Dual ISO photography (photo mode working, movie mode broken)
9. `edmac.mo` - EDMAC tools
10. `ettr.mo` - ETTR (Expose To The Right)
11. `file_man.mo` - File manager
12. `img_name.mo` - Image naming
13. `lua.mo` - Lua scripting support
14. `mlv_lite.mo` - MLV Lite video recording
15. `mlv_play.mo` - MLV playback
16. `mlv_snd.mo` - MLV sound metadata
17. `pic_view.mo` - Picture viewer
18. `selftest.mo` - Self-test utilities
19. `silent.mo` - Silent picture (lossless)

**Notable Missing Modules:**
- `raw_vid.mo` - Not included (newer architecture)
- `raw_vidx.mo` - Not included (MLV v3 architecture)
- `script.mo` - Not included (TCC script compilation)
- `sd_uhs.mo` - Not included (SD overclocking module)

### Lua Scripts (11 total)
- `api_test.lua` - API testing
- `hello.lua` - Hello world example
- `pong.lua` - Pong game
- `menutest.lua` - Menu testing
- `sokoban.lua` - Sokoban game
- `scrnshot.lua` - Screenshot utility
- `copy2m.lua` - Copy utility
- Plus library modules in `lib/`

### Data Files
- **Crop marks:** 5 BMP files for crop mode overlays
- **LUT files:** 4 color LUTs (apsc8r, apsc8p, ff8r, ff8p)
- **Fonts:** 6 RBF font files

## Known Limitations (from codebase analysis)

### High Priority Issues
1. **No LV_FOCUS_DATA** - Focus confirmation in Magic Zoom disabled
2. **FPS Override Broken** - Cannot change frame rates (Timer B issues, Timer A causes banding)
3. **RAW Zebras Disabled** - Causes QuickReview/LV corruption
4. **Dual ISO Movie Mode Broken** - Only photo mode works

### Medium Priority Issues
5. **Audio Controls Missing** - Cannot adjust audio gain from ML
6. **FlexInfo Flickering** - Bottom bar display unstable
7. **Focus Stacking Buggy** - "Takes 1 behind and 1 before"
8. **SD 160MHz Overclock Fails** - Aggressive overclock doesn't work

### Low Priority Issues
9. **No Beep Support** - CONFIG_BEEP disabled
10. **METERING/AF-Area Toggle Unreliable** - Button handling issues

## Comparison: This Build vs Repository

### What's Working in This Build
✅ Basic ML functionality (menus, config, modules)
✅ RAW video recording (mlv_lite)
✅ Dual ISO photography
✅ Crop recording (basic mode only)
✅ ETTR
✅ Lua scripting
✅ Deflicker
✅ Intervalometer

### What Needs Development (per FUTURE-WORK.md)
🔲 Focus confirmation and stacking
🔲 FPS override for slow-mo/timelapse
🔲 RAW zebras
🔲 Advanced crop modes (1:1, 3K, 4K)
🔲 Dual ISO movie mode
🔲 Audio controls
🔲 SD UHS overclocking
🔲 MLV v3 port (raw_vidx)

## Testing This Build

### Installation (On Physical 70D)
1. Format SD card (FAT32)
2. Copy `ML-SETUP.FIR` to card
3. Install via firmware update menu
4. Copy `autoexec.bin` and `ML/` folder to card
5. Boot camera (hold PLAY button)

### Verification Steps
```bash
# Check autoexec.bin is ARM code
file autoexec.bin
# Should show: ARM executable

# Verify module count
ls -1 ML/modules/*.mo | wc -l
# Should show: 19
```

## Development Next Steps

### Immediate (Sprint 0)
1. ✅ Analyze existing build structure
2. 🔲 Build from source to match this build
3. 🔲 Verify build process produces identical output
4. 🔲 Set up QEMU emulation environment

### Short Term (Sprints 1-3)
1. 🔲 Implement LV_FOCUS_DATA memory spy
2. 🔲 Test FPS override register calculations
3. 🔲 Enable RAW zebras with double-buffering
4. 🔲 Add crop_rec advanced modes

### Medium Term (Sprints 4-7)
1. 🔲 Fix dual_iso movie mode
2. 🔲 Port to MLV v3 (raw_vidx)
3. 🔲 Implement audio controls
4. 🔲 SD UHS tuning

## Build Information

**Source:** https://github.com/peva3/magiclantern_70D  
**Forked From:** https://github.com/reticulatedpines/magiclantern_simplified  
**Camera:** Canon 70D  
**Firmware:** 1.1.2  
**Build Date:** 2025-06-20  
**DIGIC Version:** DIGIC V  

## File Structure
```
70d-dev/
├── autoexec.bin              # Main ML binary
├── ML-SETUP.FIR              # Installer
├── magiclantern.*.zip        # Distribution package
└── ML/
    ├── cropmks/              # Crop mark overlays
    ├── data/                 # LUT files
    ├── doc/                  # Documentation (empty)
    ├── fonts/                # Font files
    ├── modules/              # Compiled modules
    │   ├── 70D_112.sym      # Symbol file
    │   └── *.mo             # Module files
    └── scripts/              # Lua scripts
        ├── lib/             # Lua libraries
        └── *.lua            # Script files
```

## Useful Commands

### Extract Build Info
```bash
# List modules
ls -1 ML/modules/*.mo

# List scripts
ls -1 ML/scripts/*.lua

# Check symbol file
head -20 ML/modules/70D_112.sym
```

### Compare with Repository
```bash
# Check if repository builds match
cd /app/70d
make -j4 70D
cmp build/70D/autoexec.bin 70d-dev/autoexec.bin
```

## References
- AGENTS.md - Full technical architecture
- FUTURE-WORK.md - Improvement roadmap
- TODO.md - Development sprints
- TESTING_FRAMEWORK.md - Testing without physical camera
