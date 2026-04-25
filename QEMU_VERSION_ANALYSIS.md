# QEMU-EOS Version Analysis

## Current Status

**Base Version:** QEMU 4.2.1 (released December 2019)
**QEMU-EOS Branch:** `qemu-eos-v4.2.1` (custom fork by reticulatedpines)
**Upstream QEMU Latest:** 11.0.50 (development, as of April 2026)

## Version History

### QEMU Mainline Releases
- **QEMU 4.2.0** - November 2019 (base for qemu-eos)
- **QEMU 5.x** - 2020 releases
- **QEMU 6.x** - 2021 releases  
- **QEMU 7.x** - 2022 releases
- **QEMU 8.x** - 2023 releases
- **QEMU 9.x** - 2024 releases
- **QEMU 10.x** - 2025 releases
- **QEMU 11.0.50** - Current development (2026)

### QEMU-EOS Customizations
The `qemu-eos-v4.2.1` branch is based on QEMU 4.2.1 with ~16,600+ additional commits of Canon EOS-specific emulation code added on top.

## What Would an Upgrade Involve?

### Option 1: Upgrade QEMU-EOS Base (HARD - NOT RECOMMENDED)

**Difficulty:** ⚠️ **EXTREME** (Months of work)

**What's involved:**
1. **Merge upstream QEMU changes** from 4.2.1 → 11.0.50 (~7 years of development)
2. **Resolve conflicts** between mainline QEMU changes and EOS-specific patches
3. **Re-test all camera models** (50D, 6D, 70D, 5D3, 100D, etc.)
4. **Update ARM emulation** core (significant changes between 4.x and 11.x)
5. **Port custom EOS hardware** emulation ( MPU, EDMAC, display, sensors)
6. **Re-validate ML bootloader** compatibility

**Risks:**
- Could break working emulation for all cameras
- EOS-specific patches may conflict with upstream changes
- ARM CPU emulation has undergone major refactoring
- Memory management and MMU handling changed significantly
- Interrupt handling model changed

**Benefits:**
- Access to newer ARM features/emulation accuracy
- Performance improvements
- Bug fixes from 7 years of QEMU development
- Better hardware peripheral emulation

### Option 2: Stay on 4.2.1 (RECOMMENDED)

**Difficulty:** ✅ **NONE** (Current stable setup)

**Rationale:**
- QEMU 4.2.1 + qemu-eos patches **works perfectly** for 70D emulation
- Canon firmware boot verified with real ROM dumps
- ML autoexec.bin loading successful
- MPU communication fully functional (250+ messages, 0 hangs)
- No known issues that would be fixed by upgrading QEMU core

**Maintenance Strategy:**
- Backport only critical security fixes if needed
- Focus development on firmware/ML features, not QEMU core
- Use QEMU as-is for firmware testing and development

### Option 3: Selective Backports (MEDIUM - IF NEEDED)

**Difficulty:** ⚠️ **MODERATE** (Weeks of work per feature)

If specific QEMU improvements are needed (e.g., better SD card emulation, ARM bug fixes), selectively backport those patches to the 4.2.1 base.

**Process:**
1. Identify specific QEMU commits that fix relevant issues
2. Test each patch individually on 70D emulation
3. Resolve conflicts with EOS-specific code
4. Validate no regression in boot process

## Recommendation

**DO NOT UPGRADE QEMU BASE VERSION**

**Reasons:**
1. **Current version works** - 70D firmware boots successfully
2. **High risk** - 16,600+ custom commits could break
3. **Low reward** - No identified issues that require QEMU upgrade
4. **Better alternatives** - Focus on firmware features, not emulator internals
5. **Proven stability** - QEMU 4.2.1 is stable and well-tested for Canon cameras

**When to consider upgrade:**
- If a critical security vulnerability is found in QEMU 4.2.1
- If a specific ARM emulation bug affects 70D development
- If new camera requires features only available in newer QEMU
- If upstream qemu-eos project releases a tested 70D-compatible update

## Files to Monitor

If upgrade becomes necessary, these are the key EOS-specific files:
- `hw/eos/eos.c` - Main EOS emulation engine
- `hw/eos/mpu_spells/70D.h` - 70D MPU spell definitions
- `hw/eos/model_list.c` - Camera model configurations
- `hw/arm/digic.c` - DIGIC processor emulation

## Conclusion

**Stay on QEMU 4.2.1** - The current version is stable, tested, and fully functional for 70D development. An upgrade would require months of work with high risk of breaking existing functionality and minimal practical benefit for firmware development.

Focus efforts on:
- ✅ Feature development (focus, FPS, RAW zebras)
- ✅ Testing with QEMU emulation
- ✅ Bug fixes in ML code
- ❌ QEMU core upgrades (not worth the risk/effort)
