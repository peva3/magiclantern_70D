# EOS M (M1) Hardware Test Plan

**Camera:** Canon EOS M (firmware 2.0.2)  
**Status:** 🔲 Pre-hardware | **Est. time:** 2-3 hours  

---

## Prerequisites

- [ ] Canon EOS M with firmware 2.0.2
- [ ] SD card (UHS-I, 16GB+)
- [ ] Fully charged battery (LP-E12)
- [ ] USB cable (mini USB)
- [ ] Lens (EF-M 22mm f/2 or EF-M 18-55mm f/3.5-5.6 IS)
- [ ] HDMI cable + monitor (optional)
- [ ] External microphone (optional)

---

## Phase 1: ROM Dump & Installation

### 1.1 Dump Firmware ROMs
```bash
# Using ML-SETUP.FIR installer (place on SD card root)
# Camera will run the FIR file and dump ROMs
# Copy dumps to:
cp /mnt/sdcard/ROM0.BIN   /app/70d/roms/EOSM/
cp /mnt/sdcard/ROM1.BIN   /app/70d/roms/EOSM/
cp /mnt/sdcard/SFDATA.BIN /app/70d/roms/EOSM/
```

### 1.2 Install Magic Lantern
```bash
cp /app/70d/platform/EOSM.202/build/autoexec.bin /mnt/sdcard/
```

### 1.3 Boot Test
- [ ] Insert SD card, power on
- [ ] Verify ML splash screen
- [ ] Check firmware version in ML menu
- [ ] Verify no crashes or hangs

---

## Phase 2: Basic Diagnostics (15 min)

### 2.1 Camera Info
- [ ] Model ID shown correctly (0x80000331)
- [ ] Firmware version string
- [ ] Shutter count
- [ ] Battery level

### 2.2 Display
- [ ] LCD display works
- [ ] Touchscreen responds (tap, swipe, pinch)
- [ ] ML overlays visible (zebras, histogram)
- [ ] No flickering or artifacts

### 2.3 Buttons
- [ ] Power button
- [ ] Shutter button (half/full press)
- [ ] REC/LV button
- [ ] PLAY button
- [ ] MENU button
- [ ] INFO button
- [ ] Trash/DOWN button
- [ ] SET button
- [ ] Wheel (up/down/left/right)
- [ ] Movie record button (if separate)

### 2.4 Audio
- [ ] Internal microphone recording
- [ ] External microphone (if available)
- [ ] Playback audio
- [ ] Check for audio sync

---

## Phase 3: ML Feature Testing (45 min)

### 3.1 Modules
- [ ] All 21 modules listed in ML → Modules menu
- [ ] Enable/disable each module without crash
- [ ] Module descriptions display correctly

### 3.2 Dual ISO
- [ ] Photo mode: Dual ISO images capture
- [ ] Check for banding artifacts
- [ ] Movie mode: Dual ISO test

### 3.3 crop_rec
- [ ] OFF mode (no crop)
- [ ] 3x3 720p mode
- [ ] Verify crop frame display
- [ ] Record crop mode video
- [ ] Check for frame corruption

### 3.4 SD UHS Overclock
- [ ] Measure baseline write speed (no overclock)
- [ ] Enable 160MHz preset
- [ ] Enable 240MHz hybrid preset (EOSM best)
- [ ] Test stability with continuous shooting
- [ ] Test with video recording
- [ ] Check for corruption

### 3.5 Focus Features
- [ ] Focus confirmation (if LV_FOCUS_DATA enabled)
- [ ] Focus peaking
- [ ] Trap focus (photo mode)
- [ ] Magic Zoom
- [ ] Follow/rack focus (if enabled)

### 3.6 Exposure Tools
- [ ] Zebras (over/under)
- [ ] Histogram
- [ ] Spotmeter
- [ ] False color
- [ ] Waveform
- [ ] Vectorscope

### 3.7 Audio Controls (if CONFIG_AUDIO_CONTROLS enabled)
- [ ] Analog gain adjustment
- [ ] Digital gain
- [ ] AGC toggle
- [ ] Wind filter
- [ ] Input source switching
- [ ] Mic power control

### 3.8 PTP Tunnel (if ptptun module loaded)
- [ ] Connect via USB
- [ ] `./camtunnel.py --camera EOSM` connects
- [ ] Screenshot command works
- [ ] ShamemRead works
- [ ] CallByName works

---

## Phase 4: Video Recording (30 min)

### 4.1 Standard Recording
- [ ] 1080p30 recording (30 seconds)
- [ ] 1080p24 recording
- [ ] 720p60 recording
- [ ] Check for dropped frames

### 4.2 MLV Recording (mlv_lite)
- [ ] Start MLV recording
- [ ] Verify MLV files created
- [ ] Check file integrity (mlv_dump)
- [ ] Audio sync in MLV files

### 4.3 Raw Video
- [ ] Raw video recording (if raw_vidx ported)
- [ ] Check raw frame integrity
- [ ] Measure write speed

---

## Phase 5: Stress Testing (30 min)

### 5.1 Temperature
- [ ] 10 min continuous LiveView
- [ ] Video recording until auto-stop
- [ ] Check for overheating warnings

### 5.2 Battery Life
- [ ] Note battery drain during ML usage
- [ ] Compare with Canon-only usage

### 5.3 Stability
- [ ] Rapid button presses
- [ ] Mode switching (photo ↔ movie ↔ playback)
- [ ] Module enable/disable cycling
- [ ] Continuous shooting burst

---

## Phase 6: QEMU Validation (if real ROMs dumped)

### 6.1 Update ROM Files
```bash
cp roms/EOSM/ROM1.BIN /app/70d/roms/EOSM/
cp roms/EOSM/ROM0.BIN /app/70d/roms/EOSM/
cp roms/EOSM/SFDATA.BIN /app/70d/roms/EOSM/
```

### 6.2 Test QEMU Boot
```bash
./test_qemu.sh EOSM --boot --timeout 60
```

### 6.3 Verify in QEMU
- [ ] "K325 READY" (or EOSM equivalent)
- [ ] ICU firmware boot
- [ ] ML GUI factory registered
- [ ] All MPU spells resolve
- [ ] No hangs or crashes

---

## Test Results Template

```
# EOSM Hardware Test — <DATE>
## Build: platform/EOSM.202/build/autoexec.bin
## Modules: <count>

### Phase 1: ROM Dump & Installation
- [ ] ROMs dumped: ROM0=<size> ROM1=<size> SFDATA=<size>
- [ ] ML installed: autoexec.bin <size>

### Phase 2: Basic Diagnostics
- Model ID: <value> | PASS/FAIL
- Firmware: <string> | PASS/FAIL
- Shutter count: <value>
- Display: PASS/FAIL
- Touchscreen: PASS/FAIL
- Buttons: <working> <not working>

### Phase 3: ML Features
- Dual ISO: PASS/FAIL (notes)
- crop_rec: PASS/FAIL (notes)
- SD UHS <speed>: PASS/FAIL
- Focus: PASS/FAIL
- PTP tunnel: PASS/FAIL

### Phase 4: Video
- Standard rec: PASS/FAIL
- MLV rec: PASS/FAIL
- Audio sync: PASS/FAIL

### Phase 5: Stress
- Overheating: YES/NO
- Crashes: <count>
- Hangs: <count>

### Phase 6: QEMU (if applicable)
- Boot: PASS/FAIL
- MPU: <spell count>
- ML init: PASS/FAIL
```

---

## Success Criteria

### Must Pass
- [ ] Camera boots with ML installed
- [ ] No crashes during normal operation
- [ ] Video recording works
- [ ] All 21 modules load without error
- [ ] SD card reads/writes correctly

### Should Pass
- [ ] Dual ISO photo mode
- [ ] crop_rec 3x3 720p
- [ ] SD overclock 240MHz hybrid
- [ ] PTP tunnel USB connection
- [ ] Focus confirmation
- [ ] Audio recording

### Nice to Have
- [ ] Touchscreen fully working
- [ ] Trap focus working
- [ ] Higher crop_rec resolutions
- [ ] 96kHz audio recording
