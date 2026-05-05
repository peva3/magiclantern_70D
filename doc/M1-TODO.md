# EOS M (M1) — Development Roadmap & TODO

**Firmware:** 2.0.2 | **Status:** Pre-hardware (camera en route) | **Last Updated:** 2026-05-05

---

## Phase 0: Pre-Hardware Preparation (Before Camera Arrives)

| # | Task | Status | Notes |
|---|------|--------|-------|
| 0.1 | Create doc/M1-TODO.md | ✅ DONE | This file |
| 0.2 | Add EOSM section to AGENTS.md | 🔲 | Document platform status |
| 0.3 | Uncomment LV_FOCUS_DATA QEMU spells | 🔲 | Biggest potential win for focus features |
| 0.4 | Port hw_test module for EOSM | 🔲 | Hardware diagnostics ready day one |
| 0.5 | Port ptptun module for EOSM | ✅ DONE | PTP tunnel USB remote control |
| 0.6 | Verify EOSM.202 build compiles cleanly | ✅ DONE | 448KB, 21 modules |
| 0.7 | Add 96kHz sample rate to mlv_snd | ✅ DONE | Already in mlv_snd_rates[] |
| 0.8 | Write EOSM hardware test plan | ✅ DONE | doc/eosm_test_plan.md |
| 0.9 | Set up EOSM GDB scripts for QEMU | ✅ DONE | boot.gdb + existing debugmsg/patches |
| 0.10 | Fix test_qemu.sh EOSM ROM loading | ✅ DONE | Added QEMU_EOS_WORKDIR |
| 0.11 | Port hw_test module for EOSM and 70D | ✅ DONE | #ifdef CONFIG_70D/CONFIG_EOSM guards; EOSM SKIPs WiFi tests, has own RAM regions |
| 0.12 | Enable CONFIG_LV_FOCUS_INFO for EOSM | ✅ DONE | Focus features work natively (PROP_LV_FOCUS_DATA) |
| 0.13 | Enable FEATURE_SHUTTER_FINE_TUNING | ✅ DONE | Confirmed working per comment |
| 0.14 | Enable FEATURE_RACK_FOCUS/FOCUS_STACKING | ✅ DONE | Both compile-tested (focus.c build fix) |
| 0.15 | Add hw_test.mo to modules.included | ✅ DONE | 22 modules total |
| 0.16 | Fix focus.c trap_focus_autoscaling build bug | ✅ DONE | Variable moved outside FEATURE_TRAP_FOCUS guard |

## Phase 1: Initial Hardware Bring-up (Camera Arrives)

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1.1 | Dump real ROMs (ROM0, ROM1, SFDATA) | 🔲 | Essential for full QEMU boot |
| 1.2 | Verify ML splash screen on boot | 🔲 | Basic ML installation check |
| 1.3 | Run hw_test diagnostics | 🔲 | Validate all subsystems |
| 1.4 | Test touchscreen functionality | 🔲 | nanomad's FIXME needs physical testing |
| 1.5 | Test focus confirmation (LV_FOCUS_DATA) | 🔲 | If QEMU spells are enabled |
| 1.6 | Test SD overclock at 240MHz | 🔲 | EOSM is best DIGIC V for this |
| 1.7 | Test Dual ISO photo mode | 🔲 | Validate unique table addresses |
| 1.8 | Test crop_rec basic modes | 🔲 | OFF/3x3 720p |
| 1.9 | Test PTP tunnel via USB | 🔲 | If ptptun module is ported |

## Phase 2: Feature Development

| # | Task | Status | Notes |
|---|------|--------|-------|
| 2.1 | Enable CONFIG_AUDIO_CONTROLS for EOSM | 🔲 | If ASIF beep bug is fixed |
| 2.2 | Add higher crop_rec resolutions | 🔲 | 18MP sensor calibration needed |
| 2.3 | Fix ASIF/beep hang | 🔲 | Register-level debugging |
| 2.4 | Fix touchscreen fully | 🔲 | Physical camera required |
| 2.5 | Enable Q_MENU_PLAYBACK | 🔲 | Depends on touchscreen |
| 2.6 | Add focus confirmation bars | 🔲 | Via LV_FOCUS_DATA spells |
| 2.7 | Add focus peaking | 🔲 | Via LV_FOCUS_DATA spells |
| 2.8 | Enable trap focus | 🔲 | Via LV_FOCUS_DATA spells |
| 2.9 | Add 96kHz audio recording | 🔲 | Already done on 70D, port to EOSM |
| 2.10 | Write SELFTEST for EOSM hardware | 🔲 | Automated testing framework |

## Phase 3: Advanced Features & Polish

| # | Task | Status | Notes |
|---|------|--------|-------|
| 3.1 | Port raw_vidx module | 🔲 | MLV v3 recording |
| 3.2 | Port mlv_rec module | 🔲 | Alternative raw recording |
| 3.3 | Port wifisrv module | 🔲 | N/A — EOSM has no WiFi |
| 3.4 | Enable EXPOSURE_FUSION | 🔲 | May work now |
| 3.5 | Enable GHOST_IMAGE | 🔲 | *"No way to pick image but works"* |
| 3.6 | Port adtglog2 module | 🔲 | ADTG register logging |
| 3.7 | Update QEMU spells for full LV simulation | 🔲 | Uncomment more spells |
| 3.8 | Add EOSM to test_all_cams.sh | 🔲 | Multi-camera test suite |

## Known Issues Tracker

| Issue | File | Status | Notes |
|-------|------|--------|-------|
| ASIF/Beep hangs camera | internals.h | UNFIXED | `CONFIG_BEEP` commented out |
| Touchscreen partial | internals.h | FIXME | *"Needs more hacking - nanomad"* |
| Frame ISO stubborn | fps-engio.c:2263 | KNOWN LIMIT | Only works during RECORDING_H264 |
| Magic Zoom busy-wait | zebra.c:3356 | UNFIXED | Excluded from DIGIC V clean path |
| LV exit unique path | movtweaks.c:256 | PARTIAL | Uses SetGUIRequestMode(21) |
| RAW height exception | raw.c:761 | WORKAROUND | Forced height=727 in 720p non-crop |
| prop_lv_lens guessed | lens.h:161 | UNVERIFIED | Offsets are *"guess (not tested)"* |
| CF init in QEMU | test_qemu.sh | PARTIAL | Placeholder ROMs, real dumps needed |

## Build Tracking

| Date | autoexec.bin | magiclantern.bin | Changes |
|------|-------------|------------------|---------|
| 2026-05-05 | ~448KB | ~445KB | Baseline (from existing build artifacts) |
