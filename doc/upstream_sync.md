# Upstream Sync Strategy

## Overview

This repo has two upstream sources that may receive new changes:

| Upstream | URL | What We Track |
|----------|-----|---------------|
| **magiclantern_simplified** (ML mainline) | `https://github.com/reticulatedpines/magiclantern_simplified.git` | The core ML source (`src/`, `modules/`, `platform/`) |
| **qemu-eos** (QEMU Canon emulator) | `https://github.com/reticulatedpines/qemu-eos.git` | The QEMU emulation layer (subtree at `qemu-eos/`) |

**Status** (as of 2026-05-05): The upstream ML mainline (`dev` branch) was last updated October 30, 2025. It is relatively stable with occasional commits. Our fork has diverged significantly with 90,000+ commits (including the full upstream history) and extensive 70D-specific work on top.

## Sync Strategy

### ML Mainline (`magiclantern_simplified`)

The upstream ML repo uses a `dev` branch for ongoing development. Since our repo already contains the full upstream history, syncing is a standard `git merge`:

```
git fetch upstream dev
git merge upstream/dev
```

This creates a merge commit that pulls in any upstream changes since our last sync. Conflicts are possible in areas where we have 70D-specific modifications (e.g., `platform/70D.112/`, `src/audio-ak.c`).

**When to sync:**
- When upstream releases a new feature relevant to 70D (e.g., improved crop_rec, new RAW formats, audio improvements)
- When an upstream bug fix affects code we use
- Every 3-6 months as a maintenance check

**What NOT to merge from upstream:**
- Changes to other camera platforms (`platform/*/`) — unless the change affects shared code
- Changes to the build system that break our 70D configuration
- Unstable experimental commits (check the `edmac_unify` or `m6ii_raw_video` branches separately)

### QEMU-eOS Subtree

`qemu-eos` is a **git subtree**. This means upstream changes come in as subtree merges:

```
git fetch https://github.com/reticulatedpines/qemu-eos.git master
git merge -s subtree FETCH_HEAD
```

Subtree merges preserve the subdirectory mapping — the QEMU code stays in `qemu-eos/`.

**When to sync QEMU:**
- When upstream adds new camera model support or fixes 70D emulation bugs
- When upstream improves MPU spell accuracy for DIGIC V cameras
- When upstream fixes QEMU infrastructure issues (ARM emulation bugs, SD/CF emulation)

### Conflicts to Expect

| Area | Likelihood | Notes |
|------|-----------|-------|
| `platform/70D.112/` | High | We have extensive 70D-specific changes (internals.h, stubs.S, consts.h) |
| `src/audio-ak.c` | Medium | We modified for CONFIG_AUDIO_CONTROLS — check for upstream AK4646 changes |
| `src/raw.c` | Medium | 70D-specific skip values and RAW buffers |
| `modules/crop_rec/` | Medium | 70D timer tables and CMOS/ENGIO register values |
| `qemu-eos/` | Low-Medium | Subtree merges are usually clean unless QEMU restructured |
| `modules/raw_video/` | Low | Upstream changes to mlv_rec/mlv_snd are rare |
| `tools/` | Low | Our scripts are 70D-specific |

## Quick Reference

### Check Upstream Status

```
./tools/sync_upstream.sh --status
```

Shows how many commits we're behind/ahead of upstream.

### Merge Upstream ML Changes

```
./tools/sync_upstream.sh --merge
```

Fetches, shows changelog, prompts for confirmation, then merges.

### Merge Upstream QEMU Changes

```
./tools/sync_upstream.sh --merge-qemu
```

Same flow but for the QEMU subtree.

### Manual Sync (if script fails)

```bash
# Add upstream remote (one-time)
git remote add upstream https://github.com/reticulatedpines/magiclantern_simplified.git

# Fetch and merge
git fetch upstream dev
git log HEAD..upstream/dev                  # Review changes
git merge upstream/dev                      # Create merge commit

# On conflict: resolve, then:
git add <resolved-files>
git commit

# Verify
./validate_build.sh
./test_70d_qemu.sh --boot --no-build --timeout 60
tests/run_all.sh

# Push
git push origin main
```

### Manual QEMU Subtree Sync

```bash
git fetch https://github.com/reticulatedpines/qemu-eos.git master
git log HEAD..FETCH_HEAD                    # Review changes
git merge -s subtree FETCH_HEAD             # Subtree merge

# Rebuild QEMU
cd qemu-eos/build && make -j$(nproc)

# Test
cd /app/70d && ./test_70d_qemu.sh --boot --no-build --timeout 60
```

## After Merging

After any upstream merge, always run:

1. **Build**: `make -C platform/70D.112 -j$(nproc)`
2. **Validate**: `./validate_build.sh`
3. **Pre-deployment**: `tests/run_all.sh`
4. **QEMU test**: `./test_70d_qemu.sh --boot --no-build --timeout 60`
5. **Check size**: autoexec.bin must be under 656KB (target: <600KB)

If the build breaks or size exceeds limits, review the upstream changes and adapt our 70D configuration as needed.
