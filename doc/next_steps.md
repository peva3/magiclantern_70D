# Magic Lantern 70D - Immediate Next Steps

## Current Status (April 2026)

**Repository:** https://github.com/peva3/magiclantern_70D  
**Reference Build:** `/app/70d/70d-dev/` (June 20, 2025 build)  
**Development Status:** Existing working build with 19 modules, but key features broken/missing

## What We Have

✅ Working Magic Lantern build for 70D (firmware 1.1.2)  
✅ 19 functional modules (dual_iso, mlv_lite, crop_rec, etc.)  
✅ Complete development environment in `/app/70d/`  
✅ QEMU-EOS emulator cloned  
✅ Comprehensive documentation (AGENTS.md, FUTURE-WORK.md, TODO.md)  
✅ Testing framework for host-side development  

## What We Don't Have

❌ No physical 70D camera for testing  
❌ QEMU not configured for 70D specifically  
❌ Build system not verified (can we reproduce the autoexec.bin?)  
❌ No remote testing partners lined up  

## Phase 1: Foundation (Week 1-2)

### Step 1: Verify Build System
**Goal:** Ensure we can build from source

```bash
cd /app/70d
make -j4 70D
```

**Expected Output:** `build/70D/autoexec.bin`

**Action Items:**
- [ ] Install ARM cross-compiler toolchain
- [ ] Build autoexec.bin successfully
- [ ] Compare size/hash with reference build
- [ ] Document any build errors

**Testing:** Host-side only (no camera needed)

---

### Step 2: Set Up QEMU for 70D
**Goal:** Run ML builds in emulation

**Current State:**
- QEMU-EOS cloned at `/app/70d/qemu-eos`
- 70D support status: UNKNOWN (need to verify)

**Action Items:**
- [ ] Check if 70D is in QEMU-EOS supported list
- [ ] If not, adapt from 6D/5D3 (closest DIGIC V relatives)
- [ ] Build QEMU-EOS
- [ ] Run minimal test (qemu-fio) with 70D config

**Testing:** QEMU emulation

**Expected Result:** Can boot ML in QEMU and see console output

---

### Step 3: Create Host-Side Test Suite
**Goal:** Test logic without camera/QEMU

**Action Items:**
- [ ] Create `tests/` directory structure
- [ ] Write stub implementations for camera functions
- [ ] Create first test: focus data parsing
- [ ] Create second test: FPS register calculations
- [ ] Write Makefile for host tests

**Testing:** x86/Linux compilation

**Expected Result:** Can run `make test` and see tests pass

---

## Phase 2: Feature Development (Week 3-8)

### Priority 1: Focus Features (Sprint 2)

**Problem:** No LV_FOCUS_DATA on 70D breaks focus confirmation

**Development Path:**
1. **Week 3-4:** Memory mapping (host-side)
   - Analyze focus.c to understand what data is needed
   - Create mock focus data structures
   - Test focus graph algorithm with synthetic data

2. **Week 5-6:** Memory spy implementation
   - Implement safe memory read hooks
   - Create candidate address list (from 6D/5D3 patterns)
   - Test in QEMU if available

3. **Remote Testing (if possible):**
   - Deploy test build with logging
   - Capture memory during AF operations
   - Analyze logs for focus data patterns

**Deliverable:** Working focus confirmation in Magic Zoom

---

### Priority 2: FPS Override (Sprint 3)

**Problem:** Timer B has "untraceable problems", Timer A causes banding

**Development Path:**
1. **Week 3:** Register analysis (host-side)
   - Document FPS_REGISTER_A/B behavior
   - Calculate expected values for 24/30/60 fps
   - Create test program to verify math

2. **Week 4-5:** Safe register testing
   - Implement register read/write in QEMU
   - Test formula with mock registers
   - Identify banding source (if possible in QEMU)

3. **Remote Testing:**
   - Deploy with 24fps preset (safest)
   - Capture register states
   - Measure actual frame rates from video

**Deliverable:** 24fps and 30fps override working

---

### Priority 3: RAW Zebras (Sprint 4)

**Problem:** Causes QuickReview/LV corruption

**Development Path:**
1. **Week 5:** Algorithm testing (host-side)
   - Extract zebra threshold algorithm
   - Test on sample RAW files
   - Verify algorithm correctness

2. **Week 6:** Double-buffer implementation
   - Implement vsync-safe capture
   - Add semaphore protection
   - Test logic in host simulation

3. **Remote Testing:**
   - Deploy with logging
   - Verify no corruption
   - Tune thresholds

**Deliverable:** RAW zebras enabled without corruption

---

## Phase 3: Advanced Features (Week 9-16)

### Crop Recording Extension
- Map CMOS/ADTG registers for 1:1, 3K, 4K modes
- Add presets to menu
- Test stability

### Dual ISO Movie
- Investigate H.264 pipeline differences
- Adjust ADTG injection timing
- Test with various frame rates

### MLV v3 Port
- Enable raw_vidx module
- Map crop dimensions for 70D
- Tune worker priorities

---

## Immediate Action Items (This Week)

### Day 1-2: Environment Setup
- [ ] Install ARM toolchain (`arm-none-eabi-gcc`)
- [ ] Install QEMU build dependencies
- [ ] Verify git repository is accessible

### Day 3-4: Initial Build
- [ ] Run `make -j4 70D`
- [ ] Document any errors
- [ ] Compare output with reference build

### Day 5: QEMU Investigation
- [ ] Check QEMU-EOS for 70D support
- [ ] If missing, create basic 70D config from 6D template
- [ ] Attempt first QEMU boot

### Day 6-7: Test Framework
- [ ] Create `tests/` directory
- [ ] Write first host-side test
- [ ] Document the process

---

## Success Metrics

### Week 1 Goals
- ✅ Can build autoexec.bin from source
- ✅ QEMU boots 70D config (even if minimal)
- ✅ At least one host-side test running

### Week 2 Goals
- ✅ Full test suite compiling
- ✅ QEMU running ML modules
- ✅ First feature investigation started (focus or FPS)

### Month 1 Goals
- ✅ One high-priority feature working in QEMU
- ✅ Remote testing protocol established (if possible)
- ✅ CI/CD pipeline running automated tests

---

## Risk Mitigation

### Risk: Cannot reproduce reference build
**Mitigation:** 
- Compare commit hashes
- Check for local patches in reference build
- Document differences

### Risk: QEMU doesn't support 70D
**Mitigation:**
- Focus on host-side testing
- Prioritize finding remote testing partners
- Use simulation framework

### Risk: Cannot test on real hardware
**Mitigation:**
- Maximize host-side testable code
- Create detailed test plans for remote testers
- Partner with 70D owner community

---

## Resources Needed

### Already Available
- ✅ Source code repository
- ✅ Reference build
- ✅ Documentation
- ✅ QEMU-EOS base

### Need to Acquire/Setup
- 🔲 ARM toolchain installation
- 🔲 QEMU build environment
- 🔲 Remote testing partnerships (optional but helpful)

---

## Contact Points

For collaboration or testing partnerships:
- GitHub: https://github.com/peva3/magiclantern_70D
- Reference: magiclantern_simplified upstream

---

## Summary

**Current Phase:** Foundation (Week 1)  
**Next Milestone:** Working build environment with QEMU emulation  
**Timeline:** 1-2 weeks to foundation, 2-3 months for first major features  
**Confidence:** High (strong documentation, reference build available, clear roadmap)

The path forward is clear: establish the development environment, verify we can build and emulate, then systematically tackle features starting with focus data discovery.
