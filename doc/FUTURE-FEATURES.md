# Magic Lantern: Future Features & Strategic Direction

## A Research-Backed Feature Roadmap for DIGIC V Cameras

**Last Updated:** 2026-05-05
**Author:** Based on 70D firmware RE (95%), RAM dumps, online research, and modern camera analysis
**Applies to:** DIGIC V cameras (70D, 5D3, 6D, 7D, 650D, 700D, 100D, EOS M) + EOS M
**Unique to our work:** 70D/5D3/6D/7D/650D/700D/100D/EOSM

---

## 1. Executive Summary: Why Now?

Magic Lantern development has been dormant since ~2018. The forum is inaccessible. No active developers remain on any camera platform. Meanwhile:

- **Our RE work** has documented ~95% of the 70D firmware — more than any DIGIC V camera has ever had
- **Hardware validation** confirms 35/35 diagnostics pass on real hardware
- **WiFi stack** is proven working on physical 70D — the most advanced ML WiFi effort anywhere
- **We know exactly what's possible** because we've mapped 661 call() functions, 179 registers, the full image pipeline, and every major subsystem

This puts us in a unique position. The question is not "what can we do" — it's "what should we build."

---

## 2. Our Unique Advantages

### What We Have That No Other ML Camera Has

| Advantage | Source | Impact |
|-----------|--------|--------|
| **95% firmware RE** | Our Ghidra work | Can safely patch ANY firmware function |
| **WiFi + socket API validated** | hw_test v15 on hardware | 7 socket funcs RAM-resident, 11 PTPIP stubs valid |
| **DLNA Media Server** | Found in RAM dump | Camera already runs UPnP AV — just needs enabling |
| **DPAF (Dual Pixel CMOS AF)** | 70D sensor hardware | First ML camera with phase-detect AF control potential |
| **Full eventproc table (661)** | Sprint 36 | Every callable function known and addressable |
| **ADTG/CMOS/ENGIO register map** | Sprint 5, hw_test | Complete sensor timing and readout control |
| **PTP tunnel working** | ptptun module, Sprint 26 | Remote register R/W, Lua exec, screenshot over USB |
| **512MB RAM** | Physical hardware | Room for in-camera compositing, stacking, buffering |
| **Only active ML devs in 2026** | Forum research | No competition, no conflicting work, community hungry |

---

## 3. Feature Catalog

Features are ranked by: user impact, feasibility, uniqueness, and development time. Each includes the specific RE finding that makes it possible.

---

### TIER 1: Easy Wins (Days, Not Weeks)

These features are implementable with existing RE data and minimal new code. They use call() functions, property changes, or register writes we've already validated.

#### 1. DLNA Media Server Toggle
- **What:** The 70D's built-in WiFi runs a complete DLNA/UPnP Media Server at boot. It's initialized at ~379ms (`[WFT] InitializeAdapterControl END`). A single property change (`PROP_WIFI_SETTING` → 1) activates the full networking stack. The camera appears as "EOS" on the local network, serving photos via UPnP AV.
- **Why it's new:** No ML camera has ever enabled the built-in DLNA server. Users browsing photos on their TV/phone from the camera without any app.
- **RE evidence:** DLNA strings in RAM dump: `<deviceType>urn:schemas-upnp-org:device:MediaServer:1</deviceType>`, `<presentationURL>/presentation.html</presentationURL>`, IP `192.168.1.20` hardcoded.
- **Effort:** Afternoon project — property change + socket init
- **Risk:** Very low — server is already running, just needs network enable

#### 2. In-Camera Hot Pixel Remapping
- **What:** Uses Canon's OWN defect management pipeline (`FA_LvDetectDefectsFull`, `ExecuteDefectMarge1-5`, `sht_savedefectsproperty`) to detect and map hot pixels at the hardware level. Currently requires Canon service or post-processing to remove hot pixels. This fixes them before RAW.
- **Why it's new:** No ML camera has ever exposed Canon's defect management system. Every ML user with an aging camera develops hot pixels.
- **RE evidence:** Complete defect management chain found: `FA_LvDetectDefectsFull` (detection), `ExecuteDefectMarge1-5` (merging), `FA_DefectsMergeTestImage` (test mode), `sht_savedefectsproperty` (persist). 31 DEF_* registers mapped.
- **Effort:** 2-3 days — call() wrappers exist, need UI and sequencing
- **Risk:** Low — writes to defect map EEPROM, but Canon's own path is safe

#### 3. FA_ White Balance Calibration Tool
- **What:** Wraps `FA_AdjustWhiteBalance` in a user-friendly ML menu. Point camera at gray card, press OK, camera computes and saves per-channel color correction to EEPROM.
- **Why it's new:** Currently requires Canon Service Software (CS-S) or hacky workarounds. Professional calibration in a menu.
- **RE evidence:** `FA_AdjustWhiteBalance` returns 0 (registered in eventproc table). `FA_SetWbCoef`, `FA_GetWbCoef`, `FA_SetSsWbCoef` all present.
- **Effort:** Half-day — call() wrapper + menu entry
- **Risk:** Very low — read-only feedback, user-initiated

#### 4. SD Card Health Diagnostic
- **What:** Wraps `FA_CheckSD`, `FA_CheckMediumError` to display: write speed, read speed, bad sectors, error count, S.M.A.R.T.-like health status.
- **Why it's new:** No ML camera has ever called FA_CheckSD. Users currently have no idea if their card is failing.
- **RE evidence:** `FA_CheckSD` and `FA_CheckMediumError` in eventproc table.
- **Effort:** Half-day
- **Risk:** Very low — read-only

#### 5. Hardware Self-Test
- **What:** Runs Canon's factory assembly check (`FA_ChkAssembly`). Verifies flex cables, sensor connections, button contacts, LCD ribbon, microphone module. Displays pass/fail per subsystem.
- **Why it's new:** Useful after camera repair, drops, or buying used. Currently requires Canon service center.
- **RE evidence:** `FA_ChkAssembly` in eventproc table.
- **Effort:** Half-day
- **Risk:** Very low — read-only

#### 6. LCD Test Patterns
- **What:** Displays solid test patterns via `FA_DISP_Start100White`, `FA_DISP_Start50Gray`, `FA_DISP_StartColor`. Useful for checking LCD uniformity, dead pixels, brightness calibration.
- **Why it's new:** Currently requires Canon service software.
- **RE evidence:** FA_DISP_Start* functions in eventproc table.
- **Effort:** Half-day
- **Risk:** Very low — temporary display, no writes

#### 7. Color Blind Assist Palette
- **What:** Changes the 16-entry BMP overlay palette (PAL_0-15 at 0xC0F14080-0xBC) to a colorblind-accessible scheme. Red/green zebras become blue/yellow. Focus peaking colors shift to luminance-based.
- **Why it's new:** No camera has ever offered this. Makes ML usable for ~8% of male photographers.
- **RE evidence:** PAL registers confirmed R/W in hw_test. `CRAZY_COLORS` register at 0xC0F14040 known.
- **Effort:** Hours — table of color values
- **Risk:** Very low — pure display change, no writes

#### 8. Audio IC Live Register Dump
- **What:** Displays all AK4646 audio codec registers in real-time. Shows current gain, routing, power state, ALC status. `call("DumpAudioIcRegister")` already works.
- **Why it's new:** Useful for debugging audio issues, verifying CONFIG_AUDIO_CONTROLS changes took effect.
- **RE evidence:** `call("DumpAudioIcRegister")` returns 0. Confirmed in hw_test.
- **Effort:** Hours — display the register dump output
- **Risk:** Very low — read-only

#### 9. Overlay Brightness/Transparency Control
- **What:** Uses display registers (BRIGHTNESS=0xC0F141B8, SATURATION=0xC0F140C4) to dim or desaturate ML overlays. "Night mode" with red-only palette.
- **Why it's new:** No overlay brightness control exists. Currently overlays are always full-brightness white/green which ruins night vision for astro.
- **RE evidence:** Register addresses confirmed in hw_test.
- **Effort:** Hours
- **Risk:** Very low

#### 10. Debug Monitor Overlay
- **What:** Permanent on-screen display showing: FPS, CMOS temp, battery voltage, shutter count, card speed, ISO, aperture. `call("EnableDebugMon")` already exists.
- **Why it's new:** Useful for hardware testing, benchmarking, and power users.
- **RE evidence:** `EnableDebugMon` returns 0 in eventproc table. Known 70D property externs.
- **Effort:** Hours
- **Risk:** Very low

---

### TIER 2: Medium Effort (1-2 Weeks)

These require new module code but leverage existing RE data and validated APIs.

#### 11. WiFi Remote Trigger & File Browsing
- **What:** TCP server on the 70D's built-in WiFi. Connect from phone/laptop browser. See photo gallery (thumbnails generated in-camera), download full-res JPEG/RAW, trigger shutter remotely. Companion web app or simple REST API.
- **Why it's new:** The ONLY functioning ML WiFi feature. No other DIGIC V camera has WiFi. The socket API is proven on hardware.
- **RE evidence:** All 7 socket functions RAM-resident and validated (hw_test v15). `socket_create` at 0x00059AF8, `socket_bind` at 0x00059E94, `socket_send` at 0x0005A09C. `schedule_remote_shot` at 0x0047db24.
- **Effort:** 1-2 weeks
- **Risk:** Low — socket APIs validated, just need application code

#### 12. Olympus-Style Live Composite (Astro)
- **What:** Takes repeated exposures, applies per-pixel max() blending, shows composite building on screen. Same as Olympus Live Composite — star trails appear without overexposing the background. Background stays at base exposure, only bright pixels (stars) accumulate.
- **Why it's new:** No ML camera has ever implemented this algorithm. Requires ~120MB RAM for the composite buffer (20MP × 3 channels × 16-bit). 70D has 512MB — plenty of room.
- **Algorithm:** Base exposure → for each new frame: `composite[pixel] = max(composite[pixel], new_frame[pixel])`
- **RE evidence:** RAW capture via EDMAC slurp works. FPS override controls exposure timing. Event pusher schedules frame capture.
- **Effort:** 1 week
- **Risk:** Low — simple algorithm, no register writes

#### 13. Intervalometer with Exposure Ramping ("Holy Grail" Timelapse)
- **What:** Day-to-night timelapse with automatic exposure ramping between frames. Uses ISO override, shutter control, and aperture control to smoothly transition from day to night exposure. Existing ML components: bulb ramping, ISO override, intervalometer.
- **Why it's new:** ML has bulb ramping and intervalometer, but never combined with smooth ISO+shutter interpolation across a sequence. Requires frame-to-frame exposure smoothing.
- **RE evidence:** `FEATURE_FPS_OVERRIDE` enabled. Frame ISO and shutter override documented. Bulb ramping API exists.
- **Effort:** 1 week
- **Risk:** Low-Moderate — existing components, just need coordination

#### 14. Anamorphic Desqueeze Preview
- **What:** When using anamorphic lenses, the LV preview is vertically squeezed. This applies a real-time desqueeze (1.33x, 1.5x, 2x) to the display. Cropmarks and framing guides adjust accordingly. Existing cropmarks framework makes this straightforward.
- **Why it's new:** No ML camera has anamorphic desqueeze. Popular with anamorphic shooters using vintage lenses on Canon DSLRs.
- **RE evidence:** Display pipeline documented. YUV→VRAM stage supports coordinate transforms. Cropmarks framework exists.
- **Effort:** 3-5 days
- **Risk:** Low — display-only transformation

#### 15. Custom Color Science / LUT Loading
- **What:** Load custom 33x33x33 3D LUTs from SD card and apply them to the LiveView display. Create film simulation looks (Fuji, Kodak, Leica) or technical LUTs (C-Log to Rec709). Separate from recording — affects monitoring only.
- **Why it's new:** No ML camera has ever loaded user LUTs. Modern cameras charge thousands for this feature.
- **RE evidence:** Display filter registers (FILTER_EN=0xC0F14140) control display processing. EekoSsrawToYuvPathCore shows the conversion pipeline. YUV→VRAM path supports transformations.
- **Effort:** 1-2 weeks
- **Risk:** Moderate — need to understand the display LUT format and hardware path

#### 16. FA_ Dark Frame Calibration Tool
- **What:** Runs Canon's dark current calibration (`FA_DarkAdjAutoExecute`, `FA_DarkCheckAutoExecuteCH`). User caps lens, presses start, camera captures dark frames and recomputes the dark current subtraction map. Reduces noise in long exposures and high ISO.
- **Why it's new:** Currently requires Canon service center. Dark frame calibration degrades over time as the sensor ages.
- **RE evidence:** FA_DarkAdjAutoExecute and FA_DarkCheckAutoExecuteCH in eventproc table.
- **Effort:** 1-2 days
- **Risk:** Low — Canon's own safe path

#### 17. Custom False Color IRE Ranges
- **What:** User-configurable false color thresholds. Currently hardcoded to specific IRE levels. Let users set FB_LOW (0xC0F140D0) and FB_HIGH (0xC0F140D4) to custom values. Add presets for skin tone (70% IRE), white (90%), black (10%).
- **Why it's new:** Every other false color implementation is hardcoded. This extends ML's false color to be more useful for video.
- **RE evidence:** FB_LOW/HIGH registers confirmed R/W in hw_test. ZEBRA_REG at 0xC0F140CC understood.
- **Effort:** 1-2 days
- **Risk:** Low — register writes, preview visible immediately

---

### TIER 3: Hard (2-6 Weeks)

These require deeper code, new algorithms, or hardware calibration.

#### 18. DPAF RAW Zebra Fix
- **What:** RAW zebras are disabled on 70D because DPAF pixels interspersed in the sensor array cause false overexposure readings. A pixel mask that skips DPAF phase-detect pixels when computing RAW zebras would fix this. Requires mapping DPAF pixel positions and modifying raw_zebra computation to exclude them.
- **Why it's new:** Would ENABLE RAW zebras on 70D for the first time. Currently disabled (CONFIG_NO_RAW_ZEBRAS). Also fixes RAW spotmeter.
- **RE evidence:** Root cause identified (Sprint 4, 2026-04-28). DPAF pixel map format unknown but inferable from sensor layout. DPAF covers 80% of sensor width.
- **Effort:** 1-2 weeks
- **Risk:** Moderate — wrong mask could cause other artifacts

#### 19. Focus Bracketing Bugfix
- **What:** The existing `stack_focus` module has a bug on 70D — "takes 1 behind and 1 before all others afterwards are before at the same position." This is a step-counting bug where the focus range halving works incorrectly. The lens control APIs are all mapped.
- **Why it's new:** Would fix focus stacking for 70D. Currently the feature is unusable.
- **RE evidence:** `PROP_LV_LENS` provides focus_pos at offset 0x23. `AdjustFocusLens`, `set_motor_position`, `LvLensDrive` all NSTUB'd. The bug is in step sequence logic, not hardware.
- **Effort:** 2-5 days
- **Risk:** Low — bug fix in existing code

#### 20. Remote AF via WiFi
- **What:** Focus pulling from a phone/tablet. Uses `AfCtrl_Act_StartLensDriveRemote` to initiate focus via WiFi, on-screen slider adjusts focus distance, `EndLensDriveRemote` confirms final position. Enables rack focus shots without touching the camera.
- **Why it's new:** The RE document found AfCtrl remote functions in RAM. No ML camera has ever exposed them. Enables video focus pulling which normally requires a follow focus rig.
- **RE evidence:** Complete AF remote state machine in ROM: `AfCtrl_Act_Ready` → `StartLensDriveRemote` → `SetLensParameterRemote` → `EndLensDriveRemote`. `schedule_remote_shot` at 0x0047db24.
- **Effort:** 2-3 weeks
- **Risk:** Moderate — AF functions are validated as existing but their exact calling convention needs testing

#### 21. WiFi Live View Streaming
- **What:** Streams LiveView frames over UDP/TCP at ~5-10fps to a laptop/phone. Uses EDMAC LV buffer capture + socket_send. Lower resolution than HDMI but requires no cables.
- **Why it's new:** The most requested ML WiFi feature that never materialized.
- **RE evidence:** LV buffer addresses known (0x5F227800). EDMAC LV channel at 0xC0F26200. Socket send validated in RAM.
- **Effort:** 2-4 weeks
- **Risk:** Moderate — streaming at useful frame rate needs optimization

#### 22. Lossless Compression Parameter Tuning
- **What:** ML menu to tune the hardware lossless compression engine: slice size (SLICE_SIZE=0xC0F37300), compression mode (LOSSLESS_MODE=0xC0F373B4), config registers (0xC0F37628-48). Trade-off between compression ratio and CPU usage for RAW video.
- **Why it's new:** These registers are documented (15 registers mapped) but never exposed to users.
- **RE evidence:** All 15 lossless registers mapped in hw_test. ALT_FIX=0xC0F373F4, SLICE_SIZE=0xC0F37300.
- **Effort:** 1-2 weeks experimental
- **Risk:** Moderate — wrong values could corrupt silent pictures

#### 23. Shadow Lift / ISO Push Hardware Controls
- **What:** Direct control of hardware shadow lifting and ISO push registers. SHDW_LIFT_1-4 (0xC0F0E094, 0xC0F0E0F0, 0xC0F0F1C4, 0xC0F0F43C) raise shadow detail at the hardware level — before any processing. ISO_PUSH_D4 (0xC0F0E0F8) and ISO_PUSH_D5 (0xC0F42744) control push-ISO gain per RGGB channel.
- **Why it's new:** Shadow lifting in post-processing amplifies noise. Hardware shadow lifting preserves bit depth.
- **RE evidence:** All registers confirmed readable in hw_test. Effects documented: "raises shadows" (type 1), "brings back shadow detail" (type 2), "has discontinuity" (type 3), "ugly" (type 4). D5 push: "per RGGB byte, 0-7".
- **Effort:** 1-2 weeks experimental
- **Risk:** Moderate — register write experiments needed, effects may be undesirable

#### 24. In-Camera Star Trail Compositing
- **What:** During a multi-hour bulb session, incrementally composite frames into a star trail image. Background task runs on the ARM CPU, accumulating maximum pixel values into a 16-bit buffer. Result saved as DNG at the end.
- **Why it's new:** No ML camera has done this. Requires ~120MB RAM buffer, which 70D has (512MB total).
- **RE evidence:** 512MB RAM available. RAW capture via EDMAC slurp works. Event pusher can schedule frames.
- **Effort:** 1-2 weeks
- **Risk:** Low-Moderate — CPU-bound during compositing

#### 25. HDMI Timecode Output
- **What:** Sets `PROP_TIMECODE_HDMI_OUTPUT` to embed SMPTE timecode in the HDMI output. External recorders (Atomos, Blackmagic) sync timecode automatically. Also `PROP_TIMECODE_HDMI_REC_COMMAND` for HDMI record start/stop trigger.
- **Why it's new:** Timecode properties discovered in 70D RE. No ML camera has ever used them.
- **RE evidence:** `PROP_TIMECODE_HDMI_OUTPUT (0x80040056)`, `PROP_TIMECODE_HDMI_REC_COMMAND (0x80040057)` found in property system. HDMI output is active during LV.
- **Effort:** 1-2 days initial, 1 week for full integration
- **Risk:** Moderate — property may need HDMI to be active, may not work without HDMI connected

#### 26. PTP Tunnel Camera Control App
- **What:** Desktop companion app (extends camtunnel.py) for the ptptun module. GUI for remote register read/write, Lua execution, screenshot capture, property change. Cross-platform (Python/PyQt or web-based).
- **Why it's new:** ptptun module works but has no user-friendly interface.
- **RE evidence:** ptptun module validated: 8 PTP commands working (CallByName, ConsoleCapture, Screenshot, ExecuteLua, EngioRead/Write, ShamemRead, SetPropertyRaw).
- **Effort:** 1-2 weeks
- **Risk:** Low — ptptun already works, just needs UI

---

### TIER 4: Research/Experimental (Ongoing)

These are speculative — they may work or may hit hardware limits.

#### 27. DPAF Depth Map Overlay
- **What:** Extract phase-detect data from the 70D's DPAF sensor to generate a real-time depth map overlay. Warm colors = near, cool colors = far. 80% of sensor covered.
- **Why it's new:** Genuinely unique — no ML camera, no Canon camera has ever displayed a depth map. Only possible on DPAF cameras.
- **Challenges:** DPAF data structure location unknown. Phase difference computation in real-time. Overlay rendering.
- **Effort:** Unknown — requires finding DPAF data in sensor pipeline
- **Risk:** High — may not be accessible from main CPU

#### 28. EEPROM Explorer
- **What:** Hex viewer/editor for camera EEPROM. Read-only mode for inspection (shutter count, serial, calibration tables). Write mode with explicit confirmation for repair.
- **Why it's new:** Currently requires Canon service software.
- **RE evidence:** `FA_ReadEepromData`, `FA_WriteEepromData`, `FA_GetPropertyAddress`, `FA_GetPropertyDataSize` all in eventproc table.
- **Effort:** 1-2 weeks
- **Risk:** HIGH — EEPROM writes can brick camera permanently
- **Warning:** Only for advanced users. Read-only by default.

#### 29. Lens Profile Extractor
- **What:** Reads lens-specific distortion, vignetting, and CA correction profiles from the lens ROM via Canon's `GetLensCorrectionData.c`. Saves as Adobe Lens Profile (.lcp) format on SD card.
- **Why it's new:** No more searching for lens profiles online. Extract from your actual lens.
- **RE evidence:** Source file paths confirmed: `GetLensCorrectionData.c`, `GetLensCorrectionData_Fmt2.c`. Lens correction data structures found in RAM strings.
- **Effort:** 2-4 weeks
- **Risk:** Moderate — requires calling correction data functions and parsing lens ROM format

#### 30. AF Scripting Engine
- **What:** Program complex AF sequences via Lua: track subject for 5s, rack focus to background, return. For video effects and automated focus bracketing.
- **RE evidence:** AfCtrl state machine mapped: ContinuousAfStart/Stop, TvAfStart, CompleteAfResult.
- **Effort:** 3-6 weeks
- **Risk:** High — AF control during recording may cause brightness changes

#### 31. Always-On HDR Video (Analog Dual ISO)
- **What:** True per-frame HDR by switching sensor gain on alternating scan lines via ADTG column gain registers (0x8882-0x8888). Different from Canon's frame-alternating dual ISO — this is per-line HDR within a single frame.
- **Why it's new:** Current dual ISO alternates between frames. Per-line ADTG gain switching would give HDR in EVERY frame — true HDR video.
- **RE evidence:** ADTG column gain registers at 0x8882-0x8888 documented. ADTG 8172/8173/8178/8179 for power timing. 30 ADTG registers total.
- **Effort:** 2-4 weeks experimental
- **Risk:** HIGH — ADTG register writes could damage sensor if wrong values used

---

## 4. Strategic Recommendations

### Phase 1: Build Credibility (Month 1)
Ship the easy wins to demonstrate momentum. These are low-risk, high-visibility features:
1. DLNA Media Server toggle — unique WiFi capability
2. Hot Pixel Remapping — solves real user problem
3. WB Calibration tool — professional feature
4. SD Card Diagnostic — useful for everyone
5. Anamorphic desqueeze — attracts video shooters
6. PTP tunnel companion app — makes infrastructure visible

### Phase 2: Establish Uniqueness (Month 2-3)
Build features no other ML camera has:
1. WiFi Remote Trigger + Gallery — first working ML WiFi tethering
2. Live Composite (astro) — first ML implementation
3. Focus bracketing fix — makes existing feature work
4. Color Blind palette — accessibility-first thinking
5. Overlay brightness/transparency — usability improvement

### Phase 3: Dominance (Month 3-6)
Features that make ML relevant again:
1. WiFi LV streaming — the killer app for studio/video
2. Custom LUT loading — attracts color graders
3. In-camera star trail stacking — attracts astro community
4. HDMI timecode — attracts video professionals
5. Holy Grail timelapse assistant — attracts timelapse community

### Phase 4: Research (Ongoing)
1. DPAF depth map — genuinely new, could be transformative
2. ADTG per-line HDR — hardware-level innovation
3. AF scripting — opens creative possibilities
4. EEPROM explorer — power user tool

---

## 5. Beyond Features: Community & Ecosystem

### What ML really needs
New features are great, but the REAL value we can provide:

**Keep the infrastructure alive.** The forum is down. The builds site is down. QEMU upstream removed EOS code. Nobody is maintaining the toolchain. Just keeping things running is valuable.

**Provide reproducible builds.** Our current setup: 172 commits, clean build, 461KB, 30 modules, hardware-verified. That's more than any DIGIC V camera has right now.

**Document everything.** Our AGENTS.md is the most comprehensive ML documentation for any single camera. The RE work (661 call() functions, image pipeline, 179 registers) is useful to ML developers even if they don't use 70D code.

**Support new developers.** If someone wants to pick up an old camera port (100D, 650D, 7D), our RE methodology applies directly. The same Ghidra scripts, the same call() discovery, the same hw_test framework — all portable.

### What gets people excited
- **"I can browse photos on my TV wirelessly from my 70D"** — that's a demo moment
- **"I can remove hot pixels without sending my camera to Canon"** — that's a solved problem
- **"I can do Olympus Live Composite on my Canon for astro"** — that's a community feature
- **"I can control my camera from a web browser over WiFi"** — that's a flagship capability
- **"I can load custom LUTs on my 10-year-old DSLR"** — that's a headline

---

## 6. What's NOT Possible (From RE Data)

Let's be honest about limits (from a1ex's "NOT possible" list + our RE):

| Feature | Why not |
|---------|---------|
| 4K video | H.264 encoder throughput and sensor readout speed limits |
| 1080p 60fps | Same — sensor reads 5472 lines at ~30fps max |
| H.265/ProRes | Codecs are fixed-function hardware blocks, not programmable |
| Continuous AF during movie | DPAF driver is proprietary; ML can't call it during recording without brightness changes |
| Full electronic shutter | DIGIC V can do EFCS (silent pictures) but not global shutter |
| Internal GPS logging | 70D has NO GPS chip — external GP-E2 accessory only |
| Gyro data for stabilization | 70D has NO internal gyroscope |
| Simultaneous LCD+HDMI output | Single display engine on DIGIC V (PoC on D6+ was experimental) |
| Clean HDMI with overlays removed | 70D HDMI is a mirror of LCD, not independent |

---

## 7. The Competitive Landscape

**Magic Lantern's position in 2026:**
- Zero active developers (except us)
- Forum inaccessible
- Upstream repo dormant since 2018
- QEMU upstream removed EOS code
- Community is scattered but still exists (Reddit r/MagicLantern, DPReview threads)

**What our competitors offer:**
- **CHDK** (Canon PowerShot): Still actively maintained. Point-and-shoot only.
- **Stock Canon firmware**: No RAW video, no focus peaking, no intervalometer, no WiFi tethering
- **Modern cameras**: Have the features ML pioneered, but cost $1,000-6,000

**Our unique selling points:**
- ML runs on $100-300 used cameras
- RAW video on 10-year-old hardware
- WiFi on cameras that Canon never enabled it for
- DPAF features no Canon camera ever had
- Hardware diagnostic capabilities that normally cost thousands at a service center

---

## 8. Conclusion

The traditional ML roadmap was "port existing features to new cameras." That era is over — there are no new cameras to port to.

The new roadmap: **Build features no camera has ever had, using the RE data nobody else has collected.**

The 70D is uniquely positioned: DIGIC V (fast enough), WiFi (built-in and proven), DPAF sensor (unique among ML cameras), 512MB RAM (room for complex operations), and 95% firmware RE (safe to patch anything).

Three features define the vision:
1. **WiFi tethering** — first fully working ML WiFi implementation
2. **Live Composite / Star Trails** — brings Olympus' best feature to Canon
3. **DPAF exploitation** — depth maps, RAW zebras, enhanced AF — genuinely new

That's a roadmap worth building.
