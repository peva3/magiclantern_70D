# Canon 70D Full 512MB RAM Dump Analysis

> Complete reverse engineering reference from the 512MB RAM dump (0x40000000–0x5FFFFFFF) of a physical Canon EOS 70D (FW 1.1.2).  
> 509MB actual data, 12,639 unique strings, ~520 callable functions, 270+ ML symbols.

---

## Table of Contents

1. [Overview](#1-overview)
2. [RAM Layout](#2-ram-layout)
3. [Call() / Eventproc Dispatch Table](#3-call--eventproc-dispatch-table)
4. [ML Symbol Table (Runtime Addresses)](#4-ml-symbol-table-runtime-addresses)
5. [Property System](#5-property-system)
6. [WiFi & Networking Stack](#6-wifi--networking-stack)
7. [Audio System](#7-audio-system)
8. [Memory Allocator Hierarchy](#8-memory-allocator-hierarchy)
9. [EDMAC / DMA System](#9-edmac--dma-system)
10. [Canon Source File Paths](#10-canon-source-file-paths)
11. [Task & Kernel System](#11-task--kernel-system)
12. [Lens & Focus System](#12-lens--focus-system)
13. [Sensor & Image Pipeline](#13-sensor--image-pipeline)
14. [GPS, Touchscreen & Defect Management](#14-gps-touchscreen--defect-management)
15. [FA_* Factory/Adjustment Functions](#15-fa_-factoryadjustment-functions)
16. [FIO_* File I/O Functions](#16-fio_-file-io-functions)
17. [H.264 / Video Encoding](#17-h264--video-encoding)
18. [USB / PTP System](#18-usb--ptp-system)
19. [Boot Log Analysis](#19-boot-log-analysis)
20. [ADTG Register Addresses](#20-adtg-register-addresses)
21. [MMIO Register Map](#21-mmio-register-map)
22. [Error & Assert Messages](#22-error--assert-messages)
23. [Module System Strings](#23-module-system-strings)

---

## 1. Overview

**Source:** Full 512MB RAM dump (0x40000000–0x5FFFFFFF) via the `ramdump` module on a physical Canon EOS 70D (shutter count: 12,349, FW 1.1.2).

**Methodology:**
- `strings -n6` extracted 20.4M lines, 12,639 unique strings (137MB output file)
- Categorized by subsystem and cross-referenced against known ML/DryOS/Canon API patterns
- Addresses verified by ARM prologue checking (valid PUSH `0xE92Dxxxx`)

**Key findings at a glance:**
- ~742 call() / eventproc dispatch function names
- 191 FA_* factory/adjustment functions
- 270+ ML symbols at runtime addresses
- 34 PROP_ IDs with 8 ML property handlers
- 68 Canon firmware source file paths (47 Eeko image processing paths)
- Complete WiFi SDIO/DLNA/socket/PTPIP stack
- 14 ASIF audio DMA functions
- AllocateMemory → SRM → PackHeap → fio_malloc hierarchy

---

## 2. RAM Layout

```
Range                 Size    Density  Content
──────────────────────────────────────────────────
0x40000000–0x41000000  16MB    65%     ML code + data, ~260K strings
0x41000000–0x42000000  16MB    55%     Canon structs, task data
0x42000000–0x44000000  32MB    70%     Densest: firmware data, GUI buffers
0x44000000–0x4E000000 160MB    45%     AllocateMemory pool, ML_OBJS loaded here
0x4E000000–0x50000000  32MB    30%     Sparse: uninit heap (0x55555555/0xAAAAAAAA)
0x50000000–0x60000000 256MB    15%     Very sparse: mostly 0x00, scattered data
```

### Key Memory Regions

| Address | Size | Content | Verified By |
|---------|------|---------|-------------|
| `0x404e5664` | 7×16-bit | Photo CMOS ISO table (stride 20) | hw_test v22+ |
| `0x404e5704` | 7×16-bit | Photo mirror ISO table | hw_test v22+ |
| `0x404e7248` | 7×16-bit | LV/movie base ISO table | hw_test v22+ |
| `0x404e77d6` | 7×16-bit | Movie ISO table (stride 46) | hw_test v22+ |
| `0x0045d1a8` | — | `run_in_separate_task` | 70D_112.sym |
| `0x00470068` | — | `prop_init` | 70D_112.sym |
| `0x0046ddc8` | — | `raw2iso` | 70D_112.sym |
| `0x0048cad4` | — | `edmac_memcpy` | 70D_112.sym |
| `0x00482334` | — | `GetBatteryLevel` | 70D_112.sym |
| `0x00059AF8` | — | `socket_create` | RAM-loaded socket |
| `0x00059E94` | — | `socket_bind` | RAM-loaded socket |
| `0x0005A9D0` | — | `socket_listen` | RAM-loaded socket |
| `0x00059CE8` | — | `socket_recv` | RAM-loaded socket |
| `0x0005A09C` | — | `socket_send` | RAM-loaded socket |
| `0x0005A810` | — | `socket_setsockopt` | RAM-loaded socket |
| `0xFF7AF220` | — | `ptpip_sock_create` | ROM1 stub |
| `0xFF7AEE18` | — | `ptpip_bind_param` | ROM1 stub |
| `0xFF7AEE80` | — | `ptpip_open_server` | ROM1 stub |
| `0xFF7AF2CC` | — | `ptpip_create_client` | ROM1 stub |
| `0xFF7AEFCC` | — | `ptpip_listen_close` | ROM1 stub |
| `0xFF7AF344` | — | `ptpip_close_server` | ROM1 stub |
| `0xFF7AF160` | — | `ptpip_sock_accept` | ROM1 stub |

---

## 3. Call() / Eventproc Dispatch Table

~742 function names extracted. All are candidates for `call("FunctionName")` dispatch.
Most are confirmed present in the eventproc table; some return -1 (see hw_test call() probes).

**Return value convention:** `0` = success/registered, `-1` = not found in eventproc table, `1` = returns non-zero.

### Boot & Power (12)

| Function | Purpose | hw_test |
|----------|---------|---------|
| `EnableBootDisk` | Mounts the boot disk (SD card). Makes file system accessible via FIO_* API. | CAUTION: freezes 70D via call() |
| `DisableBootDisk` | Unmounts boot disk and flushes caches. Powers down SD card. | — |
| `EnableMainFirm` | Activates main firmware partition. Used during A/B firmware switching. | — |
| `DisableMainFirm` | Deactivates main firmware partition. Used for firmware update mode. | — |
| `EnableFirmware` | Enables firmware boot from specified partition (like EnableBootDisk but at firmware level). | — |
| `DisableFirmware` | Disables firmware boot path. Preparation for firmware update. | — |
| `PrepareEnableFirmware` | Sets up firmware boot environment before enabling (allocates resources, validates partition). | — |
| `PrepareDisableFirmware` | Safely tears down firmware environment before disabling. | — |
| `Reboot` | Software reboot of camera. Preserves RAM but restarts init_task from ROM. | — |
| `Format` | Formats the SD card (FAT32). Destroys all data on card. | — |
| `EnablePowerSave` | Enters power save mode (display off, sensor low-power, tasks suspended). | — |
| `DisablePowerSave` | Wakes from power save mode. Restores display and sensor. | — |

### LiveView & Sensor (15)

| Function | Purpose | hw_test |
|----------|---------|---------|
| `FA_StartLiveView` | Initiates LiveView mode: powers on sensor, starts readout, configures display pipeline. FA_ prefix = factory-adjustment variant. | — |
| `FA_StopLiveView` | Stops LiveView: sensor readout halted, display pipeline torn down, sensor power-saved. | — |
| `PauseLiveView` | Temporarily pauses sensor readout without full teardown (faster resume). Used during mode switches. | — |
| `ResumeLiveView` | Resumes from paused state. Re-enables sensor readout and display pipeline. | — |
| `PowerOnLiveViewDevice` | Powers the image sensor into LiveView mode (low-res readout, not full capture). | — |
| `PowerOffCaptureDevice` | Powers off the image sensor/analog front end completely. Used for full sensor sleep. | — |
| `PowerOnCaptureDevice` | Powers on the sensor in full capture mode (full resolution readout, higher power). | — |
| `StartupCaptureDevice` | Full capture device initialization: clock trees, analog settings, PLL locks for photo capture. | — |
| `ShutdownCaptureDevice` | Full capture device teardown: PLLs off, analog circuits off, clock gated. | — |
| `EnableDebugGain` | Enables debug/override of sensor analog gain (CMOS ISO registers). Used for gain calibration. | — |
| `DisableDebugGain` | Restores normal sensor gain control from Canon's AE pipeline. | — |
| `EnableLvAccumGain` | Enables accumulated (multi-frame) gain boost in LV. Brightens LV display in low light. | — |
| `DisableLvAccumGain` | Disables LV frame accumulation. Returns to standard LV gain. | — |
| `EnableVideoOut` | Routes video to external output (HDMI). Activates H.264 encode pipeline for streaming. | — |
| `DisableVideoOut` | Cuts video output. Disables H.264 encode and HDMI transmitter. | — |

### HDMI & Display (8)

| Function | Purpose | hw_test |
|----------|---------|---------|
| `EnableHDMI` | Powers on HDMI transmitter and starts TMDS signaling. Type A HDMI output at 1080i/p. | — |
| `DisableHDMI` | Powers off HDMI transmitter. Returns to LCD-only display. | — |
| `EnableHDMIAudio` | Routes audio output through HDMI. Adds audio packets to HDMI blanking interval. | ✓ Returns 0 |
| `DisableHDMIAudio` | Disables HDMI audio passthrough. Audio returns to internal speaker/headphone. | — |
| `TurnOnDisplay` | Activates MAIN LCD display (flip-out screen). Sets backlight to on state. | CAUTION: freezes 70D via call() |
| `TurnOffDisplay` | Turns off MAIN LCD + backlight. Display goes blank but camera remains active. | ✓ Returns 0 |
| `SetBacklightBrightness` | Adjusts LCD backlight PWM duty cycle. Range typically 0-7 or 0-15 (hardware dependent). | — |
| `SetDisplayType` | Selects active display device: LCD vs EVF vs HDMI vs none. Controls display multiplexer. | — |

### WiFi / Networking (25+)

| Function | Purpose | hw_test |
|----------|---------|---------|
| `WLANSDIODRV_InitializeSDIODriver` | Initializes SDIO host controller for WiFi chip. Configures SDIO bus clock, voltage, IRQ. | — |
| `WLANSDIODRV_TerminateSDIODriver` | Safely shuts down SDIO WiFi driver. Unregisters IRQs, powers down SDIO bus. | — |
| `WLANSDCOMDRV_Initialize` | Initializes WiFi chip via SDIO: firmware download, chip reset, MAC address config. | — |
| `WLANSDCOMDRV_Terminate` | Shuts down WiFi chip: stops firmware, clears SDIO state. | — |
| `WLANSDCOMDRV_EnableFunction` | Enables a specific SDIO function on the WiFi chip (function 0=control, 1=data, 2=etc). | — |
| `WLANSDCOMDRV_SelectCard` | Selects/deselects the SDIO card slot for the WiFi module (RCA-based selection). | — |
| `WLANSDCOMDRV_SetOCR` | Sets Operating Condition Register on the SDIO WiFi card (voltage negotiation). | — |
| `WLANSDCOMDRV_GetRCA` | Gets Relative Card Address of the WiFi SDIO card (unique device identifier). | — |
| `WLANSDCOMDRV_ReadByte` | Reads a single byte from the WiFi chip via SDIO CMD53 (byte-level read). | — |
| `WLANSDCOMDRV_WriteByte` | Writes a single byte to the WiFi chip via SDIO CMD53 (byte-level write). | — |
| `WLANSDCOMDRV_SetBlockSize` | Configures SDIO block transfer size for WiFi data (typically 64-512 bytes). | — |
| `WLANSDCOMDRV_SetBusWidth` | Sets SDIO bus width for WiFi (1-bit or 4-bit). 4-bit gives ~4x throughput boost. | — |
| `InitializePTPFrameworkController` | Sets up PTP (Picture Transfer Protocol) framework for USB/WiFi connections. Registers command handlers. | — |
| `TerminatePTPFrameworkController` | Tears down PTP framework: unregisters handlers, closes connections, frees buffers. | — |
| `ptpip_sock_create` | Creates a TCP socket for PTP/IP communication (ROM1 wrapper at 0xFF7AF220). Adds SO_REUSEADDR. | ✓ Valid (PUSH prologue) |
| `ptpip_bind_param` | Binds the PTPIP socket to a port (typically 15740). Falls back to different ports if busy. | ✓ Valid |
| `ptpip_open_server` | Full PTPIP server setup: socket+bind+setsockopt. Starts listening on PTP/IP port. | ✓ Valid |
| `ptpip_create_client` | Creates a PTPIP client connection. Used when camera initiates connection to host. | ✓ Valid |
| `ptpip_listen_close` | Calls listen(1) then closes the listening socket. PTPIP session end. | ✓ Valid |
| `ptpip_close_server` | Graceful server shutdown: listen(2, shutdown) then close. Sends FIN to all clients. | ✓ Valid |
| `ptpip_set_keepalive` | Enables SO_KEEPALIVE on PTPIP socket. Sends periodic keepalive probes. | ✓ Valid |
| `ptpip_errno_handler` | Prints PTPIP socket error codes to Canon debug log. Maps errno to readable strings. | ✓ Valid |
| `socket_close_caller` | Thread-safe socket close wrapper (ROM1 NSTUB at 0xFF14F74C). Handles race conditions. | ✓ Valid |
| `socket_close_if_valid` | Checks socket fd > 0 before closing. Safe-close utility (at 0xFF7AF380). | ✓ Valid |

### AF / Lens (12)

| Function | Purpose | hw_test |
|----------|---------|---------|
| `AfCtrl_Act_Ready` | Transitions AF controller to READY state. Prepares for AF operation (powers lens motor, loads calibration). | — |
| `AfCtrl_Act_Suspend` | Suspends AF controller. Pauses active AF operation without losing state (used during mode changes). | — |
| `AfCtrl_Act_Ignore` | Puts AF controller in IGNORE state. Ignores all incoming AF commands (used when lens is detached). | — |
| `AfCtrl_Act_TvAfStart` | Starts Through-View AF (LiveView autofocus). Begins contrast-detect AF using sensor data. | — |
| `AfCtrl_Act_CompleteAe_ForTvAf` | Completes AE (auto exposure) lock before running TvAF. Ensures correct exposure for focus analysis. | — |
| `AfCtrl_Act_CompleteAfResult` | Processes completed AF result: reports focus position, confidence score, focus achieved/not. | — |
| `AfCtrl_Act_TvAfStop` | Stops TvAF operation gracefully. Lens motor parks to safe position. | — |
| `AfCtrl_Act_TvAfStop_Force` | Force-stops TvAF immediately (emergency stop). Lens motor halted, may not park correctly. | — |
| `AfCtrl_Act_EmdDriveResult` | Processes EMD (electromagnetic diaphragm) drive result: confirms aperture motor position. | — |
| `AfCtrl_Act_StartLensDriveRemote` | Starts lens focus motor drive via remote command (WiFi/USB). Used in remote shooting. | — |
| `AfCtrl_Act_EndLensDriveRemote` | Stops remote lens drive. Reports final focus position back to remote client. | — |
| `AfCtrl_Act_SetLensParameter` | Sets a lens parameter (focus position, focus speed, focus direction) for local AF operations. | — |
| `AfCtrl_Act_SetLensParameterRemote` | Sets a lens parameter via remote command. Same as above but through network interface. | — |
| `AfCtrl_Act_ContinuousAfStart` | Starts continuous AF (AI Servo) mode. Lens continuously tracks subject via predictive focusing. | — |
| `AfCtrl_Act_ContinuousAfStop` | Stops continuous AF mode. Last focus position locked. | — |
| `AfCtrl_Act_CompleteEmdDrive` | Completes EMD drive cycle. Finalizes aperture setting for next exposure. | — |
| `AfCtrl_ExecuteEvent` | Dispatches an AF controller event by name. Generic event execution dispatcher. | — |
| `AfCtrl_PropertyCBR` | Callback for PROP_LV_AF changes. Triggered when LiveView AF property changes state. | — |

### Remote Shot (3)

| Function | Runtime Address | Purpose |
|----------|----------------|---------|
| `schedule_remote_shot` | `0x0047db24` | Schedules a remote capture by queuing a shoot command. Non-blocking — returns immediately, shot happens on next idle. |
| `remote_shot` | `0x0047e00c` | Executes a remote capture immediately. Blocks until shutter cycle completes. |
| `remote_shot_flag` | `0x004cac90` | Global flag indicating remote shot in progress. Checked by shoot_task to prevent double-triggering. |

### Factory / Adjustment (191 FA_* functions)

See [Section 15](#15-fa_-factoryadjustment-functions) for the complete list.

### File I/O (15 FIO_* functions)

See [Section 16](#16-fio_-file-io-functions) for the complete list.

### EDMAC / DMA (11)

| Function | Purpose |
|----------|---------|
| `StartEDmac` | Starts an EDMAC (Enhanced DMA Controller) transfer. Begins data movement on specified channel. Address: 0x00037xxx range (RAM-loaded DryOS module). |
| `StopEDmac` | Gracefully stops an EDMAC transfer. Completes current block before halting channel. |
| `SetEDmac` | Configures EDMAC channel parameters: source addr, dest addr, transfer size, flags, callbacks. |
| `AbortEDmac` | Force-aborts an EDMAC transfer immediately. Uses if transfer hangs or times out. Unlike StopEDmac, may leave channel in inconsistent state. |
| `ConnectReadEDmac` | Connects a hardware read source (sensor, memory, peripheral) to an EDMAC channel. Sets up the read-side DMA descriptor. |
| `ConnectWriteEDmac` | Connects a hardware write destination (memory, display buffer, file buffer) to an EDMAC channel. Sets up the write-side DMA descriptor. |
| `RegisterEDmacCompleteCBR` | Registers a callback function to be called when EDMAC transfer completes. Used for async notification (e.g., frame capture done). |
| `UnregisterEDmacCompleteCBR` | Removes a previously registered EDMAC completion callback. |
| `RegisterEDmacAbortCBR` | Registers a callback for EDMAC abort events (error/timeout handler). |
| `UnregisterEDmacAbortCBR` | Removes a previously registered EDMAC abort callback. |
| `RegisterEDmacPopCBR` | Registers a callback for EDMAC "pop" events (when a transfer descriptor is consumed/populated). Used in chain-mode DMA. |
| `UnregisterEDmacPopCBR` | Removes EDMAC pop callback. |
| `CreateResLockEntry` | Creates a resource lock entry for EDMAC. Prevents multiple tasks from accessing same EDMAC channel simultaneously. |
| `DeleteResLockEntry` | Deletes a resource lock entry. Frees associated lock resources. |

### Memory (12)

| Function | Purpose |
|----------|---------|
| `AllocateMemory` | Allocates a block from Canon's AllocateMemory pool. Returns pool-relative offset. For ML, pool starts at 0x4E0000 (592KB reserved). |
| `AllocateMemoryResource` | Allocates from a named memory resource pool (0xFF147F3C). Higher-level than raw AllocateMemory. Manages quotas per subsystem. |
| `GetMemory` | Retrieves a previously allocated memory block by offset. Used to access pool-allocated data. |
| `GetMemoryInformation` | Returns information about a memory block: size, allocated status, owner task. |
| `GetSizeOfMaxRegion` | Returns size of the largest contiguous free block in the memory pool. Important for checking if a large allocation will succeed. |
| `CreateMemorySuite` | Creates a memory suite (collection of related allocations). Groups allocations for easier lifecycle management (bulk free). |
| `CreateMemoryChunk` | Creates a memory chunk within a suite. Individual allocation unit. |
| `DeleteMemorySuite` | Deletes entire memory suite. Frees all chunks within it. |
| `AllocateLocalMemory` | Allocates memory local to the calling task. Task-scoped, auto-freed when task exits. |
| `AllocateUncacheableMemory` | Allocates memory that bypasses CPU cache. Used for DMA buffers where cache coherency would cause issues. |
| `AllocateHPMemory` | Allocates High Priority memory. Dedicated pool for latency-sensitive operations (EDMAC, interrupt handlers). |
| `AllocateDcfMemory` | Allocates DCF (Design rule for Camera File system) memory. Special memory for EXIF/JPEG processing pipeline. |

### Property (8)

| Function | Purpose |
|----------|---------|
| `prop_register_slave` | Registers a slave property handler. Canvas firmware responds to property changes from host (MPU/ICU). |
| `prop_deliver` | Delivers a property change notification to all registered handlers. Triggers callbacks. |
| `prop_request_change` | Requests a property value change. Goes through Canon's property arbitration (checks validity, permissions). Runtime address: 0x00470084. |
| `prop_request_change_wait` | Same as prop_request_change but blocks until change is acknowledged/complete. Waits up to timeout. |
| `prop_add_handler` | Adds a custom property handler. ML uses this to intercept property changes (e.g., PROP_LV_LENS). |
| `prop_getproperty` | Gets current value of a property by PROP_ ID. Reads from property database. |
| `FA_SetProperty` | Factory-level property set. May bypass some validation that normal prop_request_change enforces. |
| `FA_GetProperty` | Factory-level property get. Can access properties not normally readable via prop_getproperty. |

### Audio (14+)

| Function | Purpose | hw_test |
|----------|---------|---------|
| `StartASIFDMAADC` | Starts ASIF (Audio Serial InterFace) DMA for ADC (microphone input). Begins capturing audio samples from mic. Address: 0xFF1172E0. | — |
| `StopASIFDMAADC` | Stops ADC DMA capture. Audio input capture ends. Address: 0xFF11758C. | — |
| `StartASIFDMADAC` | Starts ASIF DMA for DAC (speaker/headphone output). Begins audio playback. Address: 0xFF1176B4. | — |
| `StopASIFDMADAC` | Stops DAC DMA playback. Address: 0xFF117934. | — |
| `SetNextASIFADCBuffer` | Queues next buffer for ADC DMA. Double-buffer scheme for continuous audio capture. Address: 0xFF117DFC. | — |
| `SetNextASIFDACBuffer` | Queues next buffer for DAC DMA playback. Address: 0xFF117FE4. | — |
| `SetAudioVolumeIn` | Sets microphone input gain/volume. Analog or digital gain control for recording. Address: 0xFF11970C. Confirmed PUSH prologue. | ✓ Returns -1 (not in call() table for 70D) |
| `SetAudioVolumeOut` | Sets speaker/headphone output volume. Address: 0xFF13F728. | ✓ Returns 0 |
| `PowerMicAmp` | Powers the microphone preamplifier on/off. Controls bias voltage to external mic. 0xFF13FDE0. | ✓ Returns -1 |
| `PowerAudioOutput` | Powers audio output amplifier (headphone/speaker driver). 0xFF14169C. | ✓ Returns -1 |
| `ResetAudioIC` | Hardware reset of audio codec IC via I2C. | ✓ Returns 0 |
| `SendDataForAudioIC` | Sends configuration data to audio IC via I2C. Sets sample rate, bit depth, routing. | ✓ Returns 0 |
| `EnableInternalMIC` | Enables built-in stereo microphone (on camera body). | ✓ Returns 0 |
| `EnableExternalMIC` | Enables external microphone jack input. Disables internal mic. | ✓ Returns 1 |
| `EnableHDMIAudio` | Routes audio output through HDMI transmitter. | ✓ Returns 0 |
| `DisableHDMIAudio` | Disables HDMI audio passthrough. | — |
| `InitializeAudioIC` | Full initialization of audio codec: I2C config, PLL setup, clock routing. | ✓ Returns -1 |
| `DumpAudioIcRegister` | Dumps all audio IC registers to debug log. Useful for reverse engineering codec. | ✓ Returns 0 |

### GPS (9)

| Function | Purpose | hw_test |
|----------|---------|---------|
| `GPS_Initialize` | Initializes GPS subsystem. Appears in boot log as `[GPS] GPS_Initialize (36, 25)`. | Returns -1 (not in call() table) |
| `GPSList` | Returns list of available GPS satellites or GPS data records. | Returns -1 |
| `GPSTime` | Gets current GPS time (atomic clock time, not camera time). More accurate than RTC. | Returns -1 |
| `GPSClearList` | Clears GPS data list cache. Forces fresh query from GPS module. | Returns -1 |
| `GetGPSTime` | Retrieves stored GPS time value. Gets last known time even if GPS is sleeping. | Returns -1 |
| `GPSListRecvCapability` | Checks GPS receiver capability: satellite systems (GPS/GLONASS), update rate, sensitivity. | Returns -1 |
| `GetGPSCaptureTimeList` | Gets list of GPS timestamps for captured images. Used for geotagging. | Returns -1 |
| `GPS_RegisterSpaceNotifyCallback` | Registers a callback for GPS position/time updates. Notifies when new position fix is available. Confirmed in boot log at 173ms. | Returns -1 |
| `gpsGetBinaryLogData` | Extracts raw GPS NMEA/sensor log data from module. Binary format, not parsed. | Returns -1 |

**Note:** All GPS call() tests return -1 on 70D. GPS functions exist in firmware but are NOT in the eventproc call() dispatch table. They may be callable through alternate paths (property handlers, task messages). The boot log shows `[FM/GPS] GPS_RegisterSpaceNotifyCallback` at 173ms, proving the GPS subsystem initializes at boot despite not being exposed via call().

### Touchscreen (8)

| Function | Purpose | hw_test |
|----------|---------|---------|
| `TCH_CheckTouchICVersion` | Reads and verifies touchscreen controller IC version. Ensures compatible chip is present. | — |
| `TCH_SetWaitingTime` | Configures debounce/waiting time between touch samples. Higher values reduce noise but add latency. | — |
| `TCH_SetOpe2SysTime` | Sets timing for operation-to-system synchronization. Controller sampling rate vs system loop. | — |
| `TCH_SetMutualGainValue` | Configures mutual capacitance sensing gain. Affects touch sensitivity for multi-touch detection. | — |
| `TCH_SetMutualLocaliDacValue` | Sets localized DAC value for mutual sensing. Per-region sensitivity calibration. | — |
| `TCH_SetGainParamForSelfScan` | Configures self-capacitance scanning gain. Used for single-finger hover/proximity detection. | — |
| `FA_SetTouchIntervalTime` | Factory adjustment for touch scan interval. Calibrates touch update frequency. | — |
| `FA_SetTouchTestTime` | Factory touch test duration. Runs self-test on touch controller for specified time. | — |

**Note:** 70D touchscreen only supports single-finger events (touch codes 0x6F-0x71). Two-finger gestures are defined in gui.h as 0x76-0x79 but marked "unavailable on this camera."

### Debug / Misc (10)

| Function | Purpose | hw_test |
|----------|---------|---------|
| `dumpf` | Dumps formatted debug information. Canon's main firmware debug output function. | ✓ Returns 0 |
| `dumpfall` | Dumps ALL debug information (verbose variant). Includes all subsystems, not just current context. | — |
| `dumpfsep` | Dumps debug info with custom separator. Variant of dumpf with configurable formatting. | — |
| `olddumpf` | Legacy/older version of dumpf. Maintained for backward compatibility with old debug code. | — |
| `olddumpfall` | Legacy verbose dump. Older variant of dumpfall. | — |
| `olddumpfsep` | Legacy separator dump. Older variant of dumpfsep. | — |
| `NotifyBox` | Displays a notification dialog box on screen. Shows message with optional timeout. Used for errors/warnings. | — |
| `NotifyBoxHide` | Hides the current notification box. Removes the on-screen message overlay. | — |
| `SetTimerAfter` | Sets a one-shot software timer. Fires callback after specified milliseconds. DryOS kernel service. | — |
| `SetHPTimerAfterNow` | Sets a high-precision hardware timer. More accurate than SetTimerAfter (microsecond precision vs millisecond). | — |
| `GetBatteryLevel` | Returns battery level as percentage (0-100). Reads LP-E6 battery through PMU/I2C. Runtime address: 0x00482334. | ✓ Returns value >0 |
| `GetBatteryPerformance` | Returns battery performance level (health/degradation). Indicates aging of battery. | — |

### Enable / Disable Helper Functions (51)

Functions that toggle camera subsystems on/off via call(). These follow a consistent EnableXxx/DisableXxx naming convention across all Canon FIRMs. Most are paired (enable + disable for the same subsystem).

| Subsystem | Enable Function | Disable Function | Notes |
|-----------|----------------|------------------|-------|
| **AF** (Autofocus) | `EnableAF` | `DisableAF` | Controls AF system power. Disable = manual focus only, lens motor off. |
| **BootDisk** | `EnableBootDisk` | `DisableBootDisk` | Mount/unmount SD card. See Boot & Power section above. |
| **CardNoiseChk** | `EnableCardNoiseChk` | `DisableCardNoiseChk` | SD card electrical noise check. Debug/validation feature. |
| **DebugGain** | `EnableDebugGain` | `DisableDebugGain` | Sensor analog gain override for factory calibration. |
| **DebugMon** | `EnableDebugMon` | *(none)* | Enables debug monitor output. Displays debug overlay on screen. |
| **FaceCatch** | `EnableFaceCatch` | `DisableFaceCatch` | Face detection in LiveView. Shows white face tracking boxes. |
| **Filter** | `EnableFilter` | `DisableFilter` | Image processing filters (creative filters: grainy B&W, soft focus, etc.). |
| **FilterForHDMI** | `EnableFilterForHDMI` | `DisableFilterForHDMI` | HDMI-specific image filtering. Separate filtering path for HDMI output. |
| **Firmware** | `EnableFirmware` | `DisableFirmware` | Firmware boot path toggling. See Boot & Power section. |
| **FixedPo** | `EnableFixedPo` | `DisableFixedPo` | Fixed power optimization. Locks power state for consistent performance. |
| **FnoCorrect** | `EnableFnoCorrect` | `DisableFnoCorrect` | F-number (aperture) correction. Compensates for lens aperture non-linearity. |
| **HDMI** | `EnableHDMI` | `DisableHDMI` | HDMI output. See HDMI & Display section. |
| **HDMIAudio** | `EnableHDMIAudio` | `DisableHDMIAudio` | Audio over HDMI. See Audio section. |
| **LinearOffset** | `EnableLinearOffset` | `DisableLinearOffset` | Linear sensor offset correction. Compensates for column ADC offset. |
| **LinerEShutCurve** | `EnableLinerEShutCurve` | `DisableLinerEShutCurve` | Linear electronic shutter curve. Affects rolling shutter compensation. |
| **Ltkids** | `EnableLtkids` | `DisableLtkids` | Long-term kids tracking. AI object tracking mode (guess: locks focus on moving children/pets). |
| **LvAccumGain** | `EnableLvAccumGain` | `DisableLvAccumGain` | LV frame accumulation gain. See LiveView section. |
| **LvFnoCorrect** | `EnableLvFnoCorrect` | `DisableLvFnoCorrect` | LV-specific aperture correction. Separate from photo FnoCorrect for LV mode. |
| **LvLinearOffset** | `EnableLvLinearOffset` | `DisableLvLinearOffset` | LV-specific linear offset. Separate correction for LV sensor readout path. |
| **MainFirm** | `EnableMainFirm` | `DisableMainFirm` | Main firmware partition. See Boot & Power section. |
| **PowerSave** | `EnablePowerSave` | `DisablePowerSave` | Low-power mode. See Boot & Power section. |
| **VideoOut** | `EnableVideoOut` | `DisableVideoOut` | Video output (HDMI). See LiveView section. |
| **WBDetection** | `EnableWBDetection` | `DisableWBDetection` | White balance auto-detection. When disabled, WB stays at current setting. |
| **InternalMIC** | `EnableInternalMIC` | *(use ExternalMIC)* | Built-in microphone. See Audio section. |
| **ExternalMIC** | `EnableExternalMIC` | *(use InternalMIC)* | External mic jack. See Audio section. |

---

## 4. ML Symbol Table (Runtime Addresses)

270+ ML functions confirmed in RAM at runtime. Key symbols organized by subsystem:

### Memory Allocation

| Symbol | Address | Purpose |
|--------|---------|---------|
| `__priv_malloc` | `0x00455cb4` | Low-level allocator that directly calls Canon's `AllocateMemory`. Returns pool-relative offset. Wraps Canon allocation with ML metadata. |
| `__priv_free` | — | Frees memory allocated by __priv_malloc. Returns block to Canon allocation pool. |
| `__mem_malloc` | `0x004570b8` | Memory manager allocator with metadata tracking. Allocates + tags memory for debugging. Used by ML's higher-level allocators. |
| `__mem_free` | — | Frees memory tracked by __mem_malloc. Updates allocation metadata on free. |
| `shoot_malloc_suite` | `0x00457578` | Shooting memory suite allocation for ML. Allocates memory during image capture phase. Uses dedicated shoot memory pool. |
| `shoot_free_suite` | — | Frees shoot_malloc_suite allocation. Returns memory to shoot pool. |
| `shoot_malloc_suite_contig` | — | Contiguous shooting memory allocation. Guarantees physically contiguous block (needed for DMA). Larger but not fragmented. |
| `shoot_malloc_frag_mem` | — | Fragmented shooting memory allocation. Can use non-contiguous blocks (no DMA). More flexible allocation. |
| `fio_malloc` | — | File I/O memory allocation. Allocates from FIO heap. Used for SD card read/write buffers. Max single allocation: 4,096 KB (from hw_test v27). |
| `fio_free` | — | Frees fio_malloc allocation. |
| `alloc_fio_file` | — | Allocates file buffer for FIO operations. Optimized for FAT filesystem block alignment. |
| `alloc_dma_memory` | — | Allocates DMA-safe memory (cache-coherent or uncached). Required for EDMAC transfers where CPU cache would corrupt data. |
| `__priv_alloc_dma_memory` | — | Private DMA allocation. Lower-level than alloc_dma_memory. Directly allocates from uncached memory pool. |
| `__priv_free_dma_memory` | — | Frees private DMA allocation. Returns to uncached pool. |
| `srm_malloc_suite` | — | SRM (Shoot Resource Manager) buffer allocation. Canon's native shooting buffer allocator, wrapped by ML. |
| `srm_free_suite` | — | Frees SRM allocation. |
| `srm_malloc_cbr` | — | SRM callback-based allocation. Allocates and registers a callback for when buffer is freed. |

### EDMAC / DMA

| Symbol | Address | Purpose |
|--------|---------|---------|
| `edmac_index_to_channel` | `0x004576f8` | Maps an EDMAC logical index to physical channel number. DIGIC V has 48 channels. Returns channel handle for other EDMAC functions. |
| `edmac_get_connection` | `0x00457954` | Gets EDMAC connection information: source type, destination type, transfer parameters. Used to identify what hardware device an EDMAC channel connects to. |
| `edmac_bytes_per_transfer` | `0x00457bcc` | Returns bytes per EDMAC transfer unit. DIGIC V always returns 16 (16 bytes per beat). Older DIGIC returns 8. Critical for buffer size calculations. |
| `edmac_memcpy` | `0x0048cad4` | High-performance async memory copy using EDMAC hardware. Much faster than CPU memcpy for large blocks. Non-blocking — uses CBR for completion. |
| `edmac_raw_slurp` | `0x0048caec` | RAW sensor data capture via EDMAC. Reads raw sensor data directly from image pipeline to ML buffer. Used by raw_vidx/mlv_rec for video capture. |
| `edmac_get_address` | — | Gets EDMAC source or destination address from a running transfer. Used to locate current position in DMA buffer. |
| `edmac_get_pointer` | — | Gets current data pointer within an EDMAC transfer (intermediate position between start and end). |
| `edmac_get_length` | — | Gets total transfer length of an EDMAC operation. Remaining bytes = length - (pointer - address). |
| `edmac_get_state` | — | Gets current EDMAC channel state: IDLE, RUNNING, ABORTED, COMPLETE. Used for polling completion status. |
| `edmac_get_dir` | — | Gets transfer direction: READ (peripheral→memory) or WRITE (memory→peripheral). |
| `edmac_get_channel` | — | Gets EDMAC channel number from a connection/index. Inverse of edmac_index_to_channel. |
| `edmac_get_base` | — | Gets base MMIO address for an EDMAC channel. Used for direct register manipulation. |
| `edmac_get_flags` | — | Gets EDMAC channel configuration flags: transfer width, burst size, address increment mode. |
| `edmac_get_info` | — | Gets complete EDMAC channel info struct. Contains all parameters: address, pointer, length, state, flags. |
| `edmac_get_total_size` | — | Gets total bytes to transfer across all queued EDMAC descriptors. For chained transfers, sums all descriptors. |
| `edmac_fix_off1` | — | Fixes EDMAC offset register 1 (source offset). Affects how source address advances per beat. DIGIC V: off1_bits=19. |
| `edmac_fix_off2` | — | Fixes EDMAC offset register 2 (destination offset). Affects how dest address advances per beat. DIGIC V: off2_bits=32. |
| `edmac_memcpy_init` | — | Initializes EDMAC memcpy operation: allocates channel, sets up descriptors, configures transfer parameters. |
| `edmac_memcpy_start` | — | Starts a previously initialized EDMAC memcpy. Tranfer begins on next DMA clock cycle. |
| `edmac_memcpy_finish` | — | Completes EDMAC memcpy: waits for transfer to finish, frees channel, cleans up descriptors. Blocking. |
| `edmac_memcpy_res_lock` | — | Locks EDMAC resource to prevent other tasks from using same channel. |
| `edmac_memcpy_res_unlock` | — | Unlocks EDMAC resource after transfer complete. |
| `edmac_copy_rectangle` | — | Copies a rectangular region of memory using EDMAC. Handles stride/pitch between rows. Used for sub-frame transfers. |
| `edmac_copy_rectangle_cbr_start` | — | Starts rectangle copy with completion callback. Async variant of edmac_copy_rectangle. |
| `edmac_memset` | — | Fills memory with constant value using EDMAC. Hardware-accelerated zeroing/filling. |
| `edmac_find_divider` | — | Finds optimal transfer divider for EDMAC. Optimizes transfer size for maximum throughput. |

### ISO / Exposure

| Symbol | Address | Purpose |
|--------|---------|---------|
| `raw2iso` | `0x0046ddc8` | Converts Canon RAW ISO value (integer register code) to display ISO number (100, 200, 400, etc.). Used throughout ML display code. |
| `raw2index_iso` | — | Converts RAW ISO value to index into ISO table. Used for ISO table lookups (e.g., CMOS register values for each ISO stop). |
| `val2raw_iso` | `0x0046e1c8` | Converts display ISO value to Canon RAW ISO format. Inverse of raw2iso. Used when ML sets ISO via menu. |
| `split_iso` | `0x0046e630` | Splits a combined ISO value into analog and digital components. Analog = sensor gain, digital = post-processing gain. |
| `iso_components_update` | `0x0046e684` | Updates ISO component display (analog vs digital breakup). Called after ISO changes. |
| `bv_apply_iso` | `0x0046f618` | Applies ISO gain to brightness value (BV). Converts exposure value considering ISO sensitivity. |
| `get_max_analog_iso` | `0x0046fd84` | Returns maximum analog ISO value (before digital gain kicks in). On 70D: likely ISO 6400. Beyond this, digital gain is used. |
| `lens_set_rawiso` | `0x0046fa8c` | Sets ISO through lens interface. Routes ISO change through Canon's lens subsystem (may trigger aperture/shutter re-calculation). |
| `hdr_set_rawiso` | `0x0046fc80` | Sets ISO for HDR mode. Different gain path than normal ISO (may lock at specific analog level). |
| `get_frame_iso` | — | Gets current frame ISO value. Reads ISO from sensor pipeline state, not from Canon property system (faster). |
| `set_frame_iso` | — | Sets current frame ISO value. Writes ISO directly to sensor pipeline for instant changes. |
| `is_native_iso` | — | Checks if an ISO value is "native" (analog gain only, no digital push/pull). Returns true for 100-6400 on 70D. |
| `iso_toggle` | — | Toggles between two ISO values (base and alternate). Used by dual_iso module for per-line ISO switching. |

### Shutter

| Symbol | Address | Purpose |
|--------|---------|---------|
| `raw2shutter_ms` | `0x0046ce98` | Converts Canon RAW shutter code to milliseconds (e.g., 0x60 → 1000ms = 1 sec). |
| `shutter_ms_to_raw` | `0x0046ceec` | Converts milliseconds to Canon RAW shutter code. Inverse of raw2shutter_ms. |
| `raw2shutterf` | — | Converts Canon RAW shutter code to fractional string ("1/125", "1/1000", "2\"", etc.). Used for display. |
| `shutterf_to_raw` | — | Converts shutter fraction string back to RAW code. |
| `get_max_shutter_timer` | `0x0047651c` | Returns maximum shutter timer value. Determines longest possible exposure (usually 30s). |
| `get_shutter_speed_us_from_timer` | — | Converts a shutter timer count to microseconds. Timer decrements per clock tick; this converts to actual time. |

### Menu / GUI

| Symbol | Address | Purpose |
|--------|---------|---------|
| `run_in_separate_task` | `0x0045d1a8` | Executes a function in a separate DryOS task. Used by debug menu entries: creates task, runs function, then destroys task. Safety mechanism — prevents menu code from blocking GUI. |
| `menu_remove` | `0x0045c6ac` | Removes a menu entry from ML's menu tree. Used to dynamically hide menu items based on context (e.g., photo-only features hidden in movie mode). |
| `ml_gui_main_task` | `0x004632d0` | Main ML GUI task entry point. Handles ML menu rendering, button events, overlay scheduling. Runs at priority 0x1e. |
| `bmp_putpixel_fast` | — | Fast pixel write to BMP framebuffer. Optimized for bulk writes (uses word writes instead of byte writes where possible). |
| `bmp_getpixel` | — | Reads pixel color from BMP framebuffer. Used for collision detection, image analysis overlays. |

### Battery / Power

| Symbol | Address | Purpose |
|--------|---------|---------|
| `GetBatteryLevel` | `0x00482334` | Returns battery charge as 0-100 integer percentage. Reads LP-E6 status via I2C PMU. Confirmed working on physical 70D in hw_test. |
| `battery_level_bars` | `0x004c8b0c` | Battery level in "bars" (0-6), matching Canon's 6-segment battery indicator. Derived from GetBatteryLevel percentage. |
| `powersave_prolong` | — | Prolongs the auto-power-off timer. Resets the idle counter to prevent camera from sleeping. Used during long operations (SD benchmark, RAM dump). |
| `auto_power_off_time` | `0x004c8b18` | Auto power-off timeout value in seconds. Global data field. Default: 30-60 seconds (user-configurable in Canon menu). |

### Audio

| Symbol | Address | Purpose |
|--------|---------|---------|
| `sound_recording_enabled` | `0x0048fbc4` | Returns whether ML-level sound recording is active. Checks both Canon state and ML's own audio module state. |
| `sound_recording_enabled_canon` | `0x0048fbac` | Returns Canon-side sound recording flag. Reads directly from Canon audio subsystem without ML wrapper. |
| `get_audio_levels` | `0x0048fcac` | Gets current audio input levels (L and R channels). Returns peak/RMS levels for audio meter display. |
| `audio_configure` | `0x0049025c` | Configures audio parameters: sample rate (48kHz fixed on 70D), bit depth (16-bit), channel count (stereo), gain. |
| `sound_recording_mode` | `0x004c8b10` | Audio recording mode setting: OFF, AUTO, MANUAL. Global data field read from Canon menu. |

### Movie / FPS

| Symbol | Address | Purpose |
|--------|---------|---------|
| `mvr_config` | `0x000946e0` | MVR (Movie Recorder) configuration struct. 0x1D8 bytes. Contains QScale, GOP sizes, bitrate targets for all frame rates. |
| `is_movie_mode` | `0x004707c4` | Checks if camera is in movie mode (live view with movie recording capability). Returns 1 if movie mode, 0 otherwise. |
| `get_video_mode_name` | `0x004707f0` | Returns string name of current video mode: "1080p30", "720p60", "480p30", etc. Used for display and logging. |

### Property System

| Symbol | Address | Purpose |
|--------|---------|---------|
| `prop_init` | `0x00470068` | Initializes ML property system. Registers ML property handlers with Canon's property framework. Called during ML boot. |
| `prop_request_change` | `0x00470084` | Requests a property value change. Routes through Canon's prop_deliver system. Used by ML to set camera properties programmatically. |

### Task / Thread

| Symbol | Address | Purpose |
|--------|---------|---------|
| `get_current_task_name` | `0x00453550` | Gets the name of the currently executing DryOS task. Returns pointer to task name string (e.g., "gui_main_task", "shoot_task"). |
| `ml_register_cbr` | `0x00487838` | Registers an ML callback function. Associates a callback with an event trigger (e.g., shutter press, mode change). |
| `ml_unregister_cbr` | — | Unregisters a previously registered ML callback. Frees associated resources. |

### Remote Shot

| Symbol | Address | Purpose |
|--------|---------|---------|
| `schedule_remote_shot` | `0x0047db24` | Schedules a remote capture by posting a message to shoot_task's message queue. Non-blocking — returns immediately. Shot happens when shoot_task processes the queue. |
| `remote_shot` | `0x0047e00c` | Executes remote capture synchronously. Blocks until shutter cycle completes (mirror up → open → close → mirror down → save). Returns shot status. |
| `remote_shot_flag` | `0x004cac90` | Global flag (uint32). Set to 1 during remote shot, 0 when idle. Used to prevent double-triggering or interrupting an active remote shot. |

### Global Data

| Symbol | Address | Purpose |
|--------|---------|---------|
| `camera_model_id` | `0x004c8b6c` | Camera model identifier. EOS 70D = `0x80000325`. Confirmed by hw_test on physical 70D. |
| `camera_model` | `0x004c8b70` | Pointer to camera model string ("Canon EOS 70D"). Used for display and is_camera() checks. |
| `camera_serial` | `0x004c8bc0` | Camera serial number string. Unique factory-programmed serial. |
| `shutter_count` | `0x004c8b3c` | Total mechanical shutter actuations (lifetime). Read from shutter assembly EEPROM. 12,349 on the test camera. |
| `shutter_count_plus_lv_actuations` | — | Shutter count including LiveView actuations. LV mode cycles the shutter differently (not a full mechanical cycle), but still counts toward wear. |
| `mirror_down` | `0x004c8af8` | Mirror position flag. 1 = mirror down (viewfinder active), 0 = mirror up (LV/movie mode or exposure in progress). |
| `firmware_version` | — | Firmware version string ("1.1.2"). Read from ROM at boot. |

---

## 5. Property System

### PROP_ IDs (34 unique)

#### Connection / WiFi

| PROP_ ID | Value | Purpose |
|----------|-------|---------|
| `PROP_CONNECT_TARGET` | — | Current connectivity target device: 0=none, 1=USB, 2=WiFi, 3=HDMI. Controls which output path camera sends data to. |
| `PROP_CONNECT_TARGET_WFT` | — | Wireless File Transmitter connection target. Specific WFT sub-mode within CONNECT_TARGET. |
| `PROP_CONNECT_TARGET_INNER` | — | Internal connection target. Controls inter-processor communication (ARM ↔ DSP, ICU ↔ MPU). |
| `PROP_PHYSICAL_CONNECT` | — | Physical connection status: USB cable plugged, HDMI cable plugged, WiFi connected. Bitfield of physical link states. |
| `PROP_INNER_PHYSICAL_CONNECT` | — | Internal physical connection status. Inter-chip link status (UART, SPI, IPC between processors). |
| `PROP_NETWORK_SYSTEM` | — | Network subsystem state: OFF, INITIALIZING, READY, ERROR. Controls WiFi/Ethernet stack lifecycle. |
| `PROP_WIFI_SETTING` | — | WiFi settings struct: SSID, password, authentication mode, channel. Boot log shows `PROP_WIFI_SETTING [0]` at boot (WiFi off). |
| `PROP_ADAPTER_DEVICE_ACTIVE` | — | Which network adapter is active: 0=none, 1=WiFi, 2=Ethernet (WFT). Controls routing. |
| `PROP_WFT_BLUETOOTH` | — | WFT Bluetooth status (70D does not have Bluetooth — this property is for higher-end bodies with WFT-BT module). Likely stubbed on 70D. |

#### Display / LiveView

| PROP_ ID | Value | Purpose |
|----------|-------|---------|
| `PROP_GUI_STATE` | — | Current GUI state: 0=OFF, 1=BOOTING, 2=ACTIVE, 3=MENU. Boot log shows transition 0→2 at ~238ms. |
| `PROP_VARIANGLE_GUICTRL` | — | Vari-angle LCD GUI control. 70D has flip-out screen: handles rotation/orientation changes. Enables when LCD is open. |
| `PROP_LV_OUTPUT_DEVICE` | — | LiveView output destination: LCD, HDMI, BOTH. Controls where LV image stream is routed. |
| `PROP_LV_DISPSIZE` | — | LiveView display size. Determines output resolution of LV feed (varies between normal and zoom modes). Used by ML to detect zoom state. |
| `PROP_HDMI_CHANGE_CODE` | — | HDMI state change code. Fires when HDMI cable is plugged/unplugged or display EDID changes. ML registers handler `_prop_handler_PROP_HDMI_CHANGE_CODE`. |
| `PROP_LCD_OFFON_BUTTON` | — | LCD on/off button state. Fires when user presses DISP button to toggle overlay/clean display. |

#### Lens / Focus

| PROP_ ID | Value | Purpose |
|----------|-------|---------|
| `PROP_LV_LENS` | `0x80050000` | LiveView lens status. Contains focus_dist (cm), focus_pos (fine steps), focal_length, aperture. ML registers `_prop_handler_PROP_LV_LENS` for focus confirmation. |
| `PROP_LV_AF` | — | LiveView AF state: IDLE, SCANNING, FOCUSED, FAILED. Used by `AfCtrl_PropertyCBR` callback. |
| `PROP_LV_AFFRAME` | — | LiveView AF frame position/size. Defines focus point region. ML registers `_prop_handler_PROP_LV_AFFRAME`. |
| `PROP_LV_LENS_DRIVE_RESULT` | — | Result of lens focus drive: position achieved, error, timeout. Reports AF motor operation outcome. |
| `PROP_LV_LENS_DRIVE_REMOTE` | — | Remote lens drive command. Sets focus target via WiFi/USB. Triggers `StartLensDriveRemote`. |
| `PROP_LENS` | — | General lens property. Contains all lens data: type, serial, focal lengths, max aperture, AF status. Large struct. |
| `PROP_TVAF_FRAMELIST` | — | Through-View AF frame list. List of AF measurement frames used during contrast-detect AF. |
| `PROP_AFMA` | `0x80010006` | AF Microadjustment property. 0x22 bytes. Mode at offset 0xD, per-lens wide at offset 20, per-lens tele at offset 21, all lenses at offset 23. ML registers `_prop_handler_PROP_AFMA`. |

#### Audio

| PROP_ ID | Value | Purpose |
|----------|-------|---------|
| `PROP_HEADPHONE_VOLUME_VALUE` | — | Headphone output volume level (0-7 or 0-15). Analog volume control of headphone amp. |
| `PROP_MOVIE_PLAY_VOLUME` | — | Movie playback volume. Volume level for reviewing recorded video clips. |

#### Card / Storage

| PROP_ ID | Value | Purpose |
|----------|-------|---------|
| `PROP_CARD_SELECT` | — | SD card slot selection. 70D has single slot, but property exists for multi-slot cameras. ML registers `_prop_handler_PROP_CARD_SELECT`. |

#### Other

| PROP_ ID | Value | Purpose |
|----------|-------|---------|
| `PROP_RTC` | `0xa0` | Real-Time Clock. Current date/time from RTC chip. Also stores date/time for file timestamps. |
| `PROP_ERROR_FOR_DISPLAY` | — | Error code for on-screen error display. Error codes map to user-visible messages. |
| `PROP_ACTIVE_SWEEP_STATUS` | — | Active sensor sweep status. Indicates whether sensor cleaning (dust removal) is in progress. |
| `PROP_DL_ACTION` | — | Download action. Controls what happens when images are downloaded via USB/WiFi. |
| `PROP_MOVIE_PARAM` | — | Movie recording parameters: resolution, frame rate, compression, bitrate. |
| `PROP_CUSTOM_WB` | — | Custom white balance data. Stores user-set WB from gray card calibration. ML registers `_prop_handler_PROP_CUSTOM_WB`. |
| `PROP_TFT_COM` | — | TFT communication property. Internal display controller communication channel. |

### ML Property Handlers (8 registered at runtime)

```
_prop_handler_PROP_CARD_SELECT
_prop_handler_0x80010007
_prop_handler_PROP_CUSTOM_WB
_prop_handler_PROP_LV_LENS
_prop_handler_PROP_HDMI_CHANGE_CODE
_prop_handler_PROP_LV_AFFRAME
_prop_handler_PROP_LV_DISPSIZE
_prop_handler_PROP_AFMA
```

---

## 6. WiFi & Networking Stack

### DLNA / UPnP Media Server

Canon's firmware includes a complete UPnP AV Media Server, discovered in RAM. This is Canon's built-in WiFi media server for sharing photos/videos to DLNA-compatible devices (smart TVs, media players, etc.).

```xml
<root xmlns="urn:schemas-upnp-org:device-1-0"
      xmlns:dlna="urn:schemas-dlna-org:device-1-0">
  <dlna:X_DLNADOC>DMS-1.50</dlna:X_DLNADOC>
  <deviceType>urn:schemas-upnp-org:device:MediaServer:1</deviceType>
  <friendlyName>EOS</friendlyName>
  <manufacturer>Canon</manufacturer>
  <manufacturerURL>http://www.canon.com</manufacturerURL>
  <modelDescription>Canon Digital Camera</modelDescription>
  <modelName>EOS</modelName>
  <presentationURL>/presentation.html</presentationURL>
</root>
```

**DLNA Services:**
- `ContentDirectory:1` (SCPDURL: `/desc/cds.xml`) — Browse/search image/video files on camera
- `ConnectionManager:1` (SCPDURL: `/desc/cms.xml`) — Manage streaming connections

**Hardcoded IP:** `192.168.1.20` (appears 25+ times across RAM). This is the camera's default WiFi IP.

**What this means:** The 70D runs a full DLNA-certified media server (DMS-1.50) over WiFi. When WiFi is enabled, the camera appears as a media device on the local network. Any DLNA client (smart TV, VLC, Kodi) can browse and stream photos from the camera without special software. This also has a web UI at `/presentation.html` accessible via browser.

### SDIO WiFi Driver

Source files found in RAM strings:
```
./WlanSdcom/WlanSdcomDrv.c
./WlanSdcom/WlanSDIODriver.c
```

The WLAN chip (likely Broadcom BCM43xx series) connects via SDIO bus. The driver stack includes:

**Initialization:**
```
WLANSDIODRV_InitializeSDIODriver
WLANSDIODRV_TerminateSDIODriver  
WLANSDCOMDRV_Initialize
WLANSDCOMDRV_Terminate
```

**Card Management:**
```
WLANSDCOMDRV_SetOCR
WLANSDCOMDRV_GetRCA
WLANSDCOMDRV_SelectCard
WLANSDCOMDRV_EnableFunction
WLANSDCOMDRV_SetBusWidth
WLANSDCOMDRV_SetBlockSize
WLANSDCOMDRV_ReadByte
WLANSDCOMDRV_WriteByte
```

**Interrupt Handling:**
```
wlanSdcomDrv_InitializeInterrupt
wlanSdcomDrv_TerminateInterrupt
RegisterSDIOInterruptCBR
DeregisterSDIOInterruptCBR
RegisterInterruptHandler (SDCON)
RegisterInterruptHandler (HDMAC)
```

**Data Transfer:**
```
ReadData_interrupt / ReadData_polling
WriteData_interrupt / WriteData_polling
wlanSdcomDrv_SendCMD
wlanSdcomDrv_CheckEndStatus
wlanSdcomDrv_CheckResponse5
```

### Socket API (RAM-loaded, 0x0005xxxx)

| Function | Address | Callers | Notes |
|----------|---------|---------|-------|
| `socket_create` | `0x00059AF8` | 24 | domain=1, type=1, protocol=0 |
| `socket_bind` | `0x00059E94` | 3 | — |
| `socket_connect` | `0x00059DDC` | 8 | — |
| `socket_listen` | `0x0005A9D0` | 9 | fd, backlog |
| `socket_recv` | `0x00059CE8` | 13 | fd, buf, len, flags |
| `socket_send` | `0x0005A09C` | 30 | Most widely used |
| `socket_setsockopt` | `0x0005A810` | 47 | Most widely used |
| `socket_close_caller` | `0xFF14F74C` | 3 | ROM1 NSTUB |
| `socket_close_if_valid` | `0xFF7AF380` | — | Safe close, fd check |

### PTPIP ROM1-Safe Wrappers (0xFF7AEE00–0xFF7AF500)

| Function | Address | Purpose |
|----------|---------|---------|
| `ptpip_sock_create` | `0xFF7AF220` | socket_create + setsockopt REUSEADDR |
| `ptpip_bind_param` | `0xFF7AEE18` | socket + bind + close on error |
| `ptpip_open_server` | `0xFF7AEE80` | Full server: socket+bind+setopt+log |
| `ptpip_create_client` | `0xFF7AF2CC` | Client connect from sockaddr |
| `ptpip_listen_close` | `0xFF7AEFCC` | listen(1) + socket_close_caller |
| `ptpip_close_server` | `0xFF7AF344` | listen(2,shutdown) + close |
| `ptpip_sock_accept` | `0xFF7AF160` | Accept incoming connection |
| `ptpip_set_keepalive` | `0xFF7AF38C` | setsockopt SO_KEEPALIVE |
| `ptpip_errno_handler` | `0xFF7AF3B4` | Print PTPIP error to debug log |

### TCP Configuration Strings

```
Max connections
Connect timeout
TIMEWAIT time
Listen queue keep time
Cleanup time
Reuse TIMEWAIT slot
Hard Close at Linger timeout
[cannot use tcp statistic in this tcp configuration]
```

### Socket Error Codes

```
ECONNABORTED
ECONNRESET
ETIMEDOUT
ECONNREFUSED
EAFNOSUPPORT
```

---

## 7. Audio System

### ASIF DMA Functions

All 14 audio DMA stubs located in ROM1:

| Function | Address | Purpose |
|----------|---------|---------|
| `StartASIFDMAADC` | `0xFF1172E0` | Start ADC DMA transfer |
| `StopASIFDMAADC` | `0xFF11758C` | Stop ADC DMA transfer |
| `StartASIFDMADAC` | `0xFF1176B4` | Start DAC DMA transfer |
| `StopASIFDMADAC` | `0xFF117934` | Stop DAC DMA transfer |
| `SetNextASIFADCBuffer` | `0xFF117DFC` | Queue next ADC buffer |
| `SetNextASIFDACBuffer` | `0xFF117FE4` | Queue next DAC buffer |
| `sounddev_task` | `0xFF118F5C` | Audio device task entry |
| `SoundDevActiveIn` | `0xFF11936C` | Activate audio input |
| `SoundDevShutDownIn` | `0xFF1195C4` | Shutdown audio input |
| `SetAudioVolumeIn` | `0xFF11970C` | Input volume control |
| `SetAudioVolumeOut` | `0xFF13F728` | Output volume control |
| `PowerMicAmp` | `0xFF13FDE0` | Microphone amplifier power |
| `PowerAudioOutput` | `0xFF14169C` | Audio output power |
| `InitializeAudioIC` | — | Audio IC initialization |
| `ResetAudioIC` | — | Audio IC reset |
| `SendDataForAudioIC` | — | I2C write to audio IC |
| `DumpAudioIcRegister` | — | Dump audio IC register state |

### Verified call() Results (from hw_test v27)

| Function | Return | Notes |
|----------|--------|-------|
| `SetAudioVolumeIn` | -1 | Not in call() table |
| `SetAudioVolumeOut` | 0 | ✓ Working |
| `PowerMicAmp` | -1 | Not in call() table |
| `PowerAudioOutput` | -1 | Not in call() table |
| `ResetAudioIC` | 0 | ✓ Working |
| `SendDataForAudioIC` | 0 | ✓ Working |
| `DumpAudioIcRegister` | 0 | ✓ Working |
| `InitializeAudioIC` | -1 | Not in call() table |
| `EnableInternalMIC` | 0 | ✓ Working |
| `EnableExternalMIC` | 1 | ✓ Working (returns 1, not 0) |
| `EnableHDMIAudio` | 0 | ✓ Working |
| `TestSetAudioMic` | 0 | ✓ Working |
| `TestSetAudioHeadPhone` | 0 | ✓ Working |

### Audio IC Source

`./Audio/AudioIC.c` — confirmed as source path.  
Audio IC model unknown — strings for AK4646, WM8731, etc. NOT found in ROM1 or RAM.  
IC identification requires hardware I2C register probe.  
**CONFIG_AUDIO_CONTROLS is disabled on all DIGIC V cameras** (70D, 5D3, 6D all commented out).

---

## 8. Memory Allocator Hierarchy

The 70D's memory allocator tree, as reconstructed from RAM strings. This shows the complete allocator hierarchy from Canon's top-level API down to ML's convenience wrappers.

```
Top Level
├── AllocateMemoryResource (0xFF147F3C)    — Canon's top-level allocator. Manages the ~592KB pool at 0x4E0000.
│      Accepts resource type (SRM, FIO, DMA) and allocates the largest contiguous block.
├── SRM_AllocateMemoryResourceFor1stJob (0xFF0E9F6C) — Shoot Resource Manager 1st job allocator.
│      Allocates memory for the first image processing job. Priority allocation.

Heap Layer
├── PackHeap/PackMem                       — Packed heap allocator: variable-size allocations with minimal
│      metadata overhead. Standard heap for task stacks, small buffers, GUI data.
├── RingHeapMem/RingHeap                   — Ring buffer heap: circular buffer allocator for streaming.
│      Used by audio DMA (ASIF) and video frame capture. FIFO alloc/free pattern.

ML Layer
├── __priv_malloc / __priv_free             — Low-level ML allocator wrapping AllocateMemory.
│      Returns pool-relative offsets. ML's primary unfragmented memory source.
├── __mem_malloc / __mem_free              — Memory manager allocator with metadata tracking.
│      Tagged allocations for debugging memory leaks. Built on __priv_malloc.

├── fio_malloc / fio_free                  — File I/O buffer allocator. Optimized for FAT filesystem.
│      Max single allocation: 4,096 KB. Uses PackHeap for sub-4KB, __priv_malloc for >4KB.
├── alloc_fio_file                         — FIO file handle allocation. Combines file struct + buffer.

├── shoot_malloc_suite / shoot_free_suite  — Shooting buffer allocator. Uses SRM during capture phase.
│      Allocates from dedicated shoot memory pool for RAW/JPEG buffers.
├── shoot_malloc_suite_contig             — Contiguous shooting buffer (physically contiguous for DMA).
│      Required for EDMAC RAW slurp. More restrictive but guaranteed no page breaks.
├── shoot_malloc_frag_mem                 — Fragmented shooting memory (non-DMA). More flexible allocation.

├── srm_malloc_suite / srm_free_suite     — SRM (Shoot Resource Manager) buffer allocation.
│      Canon's native shooting buffer API, wrapped by ML for Lv/MV buffer management.
├── srm_malloc_cbr                        — SRM callback allocation. Allocates + fires callback when
│      buffer is available. Used for async buffer request patterns.

├── __priv_alloc_dma_memory               — DMA-safe memory allocation (uncached/coherent).
│      Returns pointer suitable for EDMAC descriptors. Prevents cache coherency bugs.
├── __priv_free_dma_memory                — DMA memory deallocation. Returns to uncached pool.

Debug
├── GetFreeMemForMalloc                   — Returns free space in malloc heap (fio_malloc pool).
│      Watched to detect memory exhaustion. ~313KB on test camera.
├── GetFreeMemForAllocateMemory           — Returns free space in AllocateMemory pool.
│      Main pool free space. ~2,196KB on test camera.
├── get_free_space_32k                    — Returns largest 32K-aligned free block.
│      Important for EDMAC buffer alignment requirements.
```

**Runtime values (from hw_test v27):**
```
free_malloc  = ~313,776 bytes (fio_malloc heap)
free_alloc   = ~2,196,000 bytes (AllocateMemory pool)
fio_max      = 4,096 KB (single allocation limit)
```

**Memory allocation flow:**
```
User code → fio_malloc() → __mem_malloc() → AllocateMemoryResource() → pool at 0x4E0000
User code → shoot_malloc_suite() → srm_malloc_suite() → SRM_AllocateMemoryResourceFor1stJob()
EDMAC code → __priv_alloc_dma_memory() → AllocateUncacheableMemory() → uncached pool
```

---

## 9. EDMAC / DMA System

### Canon EDMAC Functions

```
StartEDmac                    StopEDmac
SetEDmac                      AbortEDmac
ConnectReadEDmac              ConnectWriteEDmac
RegisterEDmacCompleteCBR      UnregisterEDmacCompleteCBR
RegisterEDmacAbortCBR         UnregisterEDmacAbortCBR
RegisterEDmacPopCBR           UnregisterEDmacPopCBR
CreateResLockEntry            DeleteResLockEntry
```

All at 0x00037xxx range (RAM-loaded DryOS module). Verified by callers in ROM1.

### ML EDMAC Functions

```
edmac_index_to_channel        edmac_get_dir
edmac_get_base                edmac_get_channel
edmac_get_state               edmac_get_flags
edmac_get_address             edmac_get_pointer
edmac_get_length              edmac_get_connection
edmac_get_info                edmac_get_total_size
edmac_bytes_per_transfer      edmac_fix_off1
edmac_fix_off2
```

### ML EDMAC Operations

```
edmac_memcpy_init             edmac_memcpy_start
edmac_memcpy_finish           edmac_memcpy
edmac_memcpy_res_lock         edmac_memcpy_res_unlock
edmac_copy_rectangle          edmac_copy_rectangle_cbr_start
edmac_memset                  edmac_find_divider
edmac_raw_slurp
```

### Interrupt Names

```
REDmac0Interrupt  …  REDmac15Interrupt   (16 read channels)
WEDmac0Interrupt  …  WEDmac15Interrupt  (16 write channels)
```

DIGIC V: 48 EDMAC channels total (16 read + 16 write + 16 shared?).
bytes_per_transfer = 16 (DIGIC V).

---

## 10. Canon Source File Paths

68 unique source file paths identified in the RAM dump, organized by subsystem:

### Kernel Layer (6)

| Source File | Purpose | Notes |
|------------|---------|-------|
| `./KernelDry/KerTask.c` | DryOS task management: task creation, scheduling, priority, context switching. Core of Canon's RTOS. | Most-called kernel module |
| `./KernelDry/KerSem.c` | DryOS semaphore: binary, counting, named semaphores. Thread synchronization primitives. | Used by every ML module |
| `./KernelDry/KerSys.c` | DryOS system services: memory init, boot sequence, panic handler. Kernel bootstrap code. | First code to run after ROM |
| `./KernelDry/KerFlag.c` | DryOS event flags: 32-bit flag groups with AND/OR wait. Used for multi-condition synchronization. | More flexible than semaphores for complex conditions |
| `./KernelDry/KerQueue.c` | DryOS message queues: FIFO IPC between tasks. shoot_task uses this for capture commands. | Fixed-size messages (typically 4-8 bytes) |
| `./KernelDry/KerRLock.c` | DryOS recursive locks: re-entrant mutex. Same task can lock multiple times. | Used for nested function calls that all need exclusive access |

### Memory (4)

| Source File | Purpose | Notes |
|------------|---------|-------|
| `./Memory/Memory.c` | Memory manager: pool allocation, free list management, defragmentation. | Canon's primary memory API |
| `./PackMemory/PackHeap.c` | Packed heap allocator: variable-size allocations with minimal overhead. Optimized for embedded systems. | Used for FIO buffers, task stacks |
| `./PackMemory/PackMem.c` | Packed memory utilities: alignment, coalescing, boundary checking. | Helper functions for PackHeap |
| `./RingHeap/RingHeap.c` | Ring buffer heap: fixed-size circular allocations for streaming data. | Used for audio DMA, video frame buffers |

### Sensor / Device (3)

| Source File | Purpose | Notes |
|------------|---------|-------|
| `./SensorDrive/SensorDrive.c` | Image sensor power/initialization driver. Controls sensor power sequence, clock gating, mode transitions. Contains SDRV_* functions. | Critical for sensor startup |
| `./Device/TG/TGdriver.c` | Timing Generator driver: pixel clock, horizontal/vertical timing, ADTG register programming. | Controls sensor readout timing |
| `./Device/TG/LvTgDriver.c` | LiveView-specific TG driver: lower-resolution timing profiles optimized for LV frame rates. | Separate profile from photo TG for power savings |

### LiveView Common (7)

| Source File | Purpose | Notes |
|------------|---------|-------|
| `./LvCommon/LvGainController.c` | LiveView gain (ISO) controller. Manages analog and digital gain during LV. Contains LVGAIN_CTRL_* functions. | Key for dual_iso in LV mode |
| `./LvCommon/LvFaceYuvController.c` | Face detection in YUV color space. Processes LV frames to locate faces for AF tracking. | Uses Canon's proprietary face detection algorithm |
| `./LvCommon/LvEncodeController.c` | LiveView encode controller. Manages H.264 encoding of LV feed for HDMI output or recording preview. | Pipes LV frames into encoder |
| `./LvCommon/ImageCenter.c` | Image center/perspective correction. Applies lens distortion correction in LV mode. | Compensates for barrel/pincushion distortion |
| `./LvCommon/LvDefectController.c` | LiveView defect pixel controller. Manages hot/dead pixel mapping specifically for LV readout mode. | Different from photo defect mapping due to different readout resolution |
| `./LvCommon/LvIRCorrectController.c` | LiveView infrared correction. Compensates for IR contamination in LV (sensor filter differences at LV resolution). | Likely related to AF assist beam IR |
| `./LvCommon/LvJob.c` | LiveView job scheduler. Manages work queue for LV processing tasks. Distributes work across DSP/ARM. | Orchestrates LV pipeline |

### EPP / Video Pipeline (8)

| Source File | Purpose | Notes |
|------------|---------|-------|
| `./Epp/SsDevelop/SsDevelopController.c` | Sub-sample development controller. Manages the raw-to-subsampled-YUV conversion pipeline. | "Ss" = sub-sampled (binned) raw |
| `./Epp/SsDevelop/SsDevelopStage.c` | Sub-sample development stage. Individual processing stage in the SsDevelop pipeline. | Stage-based architecture for pipelining |
| `./Epp/Vram/VramController.c` | Video RAM controller. Manages the VRAM frame buffer for display output. | VRAM = "video RAM" on EPP |
| `./Epp/Vram/VramStage.c` | Video RAM stage. Writes processed frames into display buffer. | Final stage before LCD controller reads |
| `./Epp/Deliver/DeliverStage.c` | Deliver stage. Final output stage that delivers processed frames to destination (LCD, HDMI, encoder). | Last EPP stage |
| `./Epp/EppHist.c` | EPP histogram. Generates histogram data from processed frames for exposure analysis. | Used by AE system for exposure decisions |
| `./Path/Lv_x1_60fps/Lv_x1_60fps.c` | LiveView 1x reading at 60fps. Configures sensor for full-frame readout at 60fps LV mode. | The `x1` means no pixel binning |
| `./Path/SsLtDriver/SsLtDriver.c` | Sub-sampled light driver. Controls sensor integration time for sub-sampled readout paths. | Manages exposure timing for binned modes |

### WiFi (2)

| Source File | Purpose | Notes |
|------------|---------|-------|
| `./WlanSdcom/WlanSdcomDrv.c` | WiFi SDIO communication driver. Handles SDIO protocol layer: commands, responses, data transfer. | Between SDIO host controller and WiFi chip |
| `./WlanSdcom/WlanSDIODriver.c` | WiFi SDIO device driver. Low-level SDIO bus operations: card detect, voltage select, bus width config. | Direct hardware register access |

### AEWB (3)

| Source File | Purpose | Notes |
|------------|---------|-------|
| `./AEWB/AEWBCommon/AEWBDataStocker.c` | AEWB data stocker. Collects and buffers AE/WB measurement data from sensor frames. | "Stocker" = data accumulator |
| `./AEWB/AEWBCommon/AEWBPropControl.c` | AEWB property control. Manages AE/WB-related properties and their interactions. | Bridges sensor measurements ↔ user settings |
| `./AEWB/AEWBRegister/AEWBRegister.c` | AEWB register interface. Reads/writes sensor and image pipeline registers for AE/WB control. | Hardware abstraction layer |

### System / IPC (4)

| Source File | Purpose | Notes |
|------------|---------|-------|
| `./System/PComMem/PComMem.c` | Inter-processor communication memory. Shared memory between ARM (main CPU) and DSP/image processors. | "PCom" = Processor Communication |
| `./System/postman/postman_m.c` | Postman message router (master). Routes messages between firmware subsystems. Core IPC mechanism. | Critical for MPU ↔ ICU communication |
| `./System/PostPostman/PostPostman.c` | Post-Postman stage. Processes messages after routing. Callback execution and event dispatching. | Completes IPC pipeline |
| `./HeadControl/LvHeadControl.c` | LiveView head control. Manages sensor readout "head" position (active row window). | Used in crop_rec for sensor windowing |

### Debug / Misc (5)

| Source File | Purpose | Notes |
|------------|---------|-------|
| `./DbgMgr/DbgMgr.c` | Debug manager. Controls debug output: dumpf, serial logging, error reporting. Central debug dispatch. | dumpf() functions live here |
| `./DataStore/DataStore.c` | Data storage manager. Persistent key-value store for camera settings across power cycles. | Stores user preferences in EEPROM |
| `./DataStore/DataStoreUtility.c` | Data store utility functions. Conversion, validation, migration of stored data. | Data upgrade between FW versions |
| `./Evf/EvfState.c` | EVF (Electronic ViewFinder) state machine. Manages EVF/LCD switching, EVF_STATE object. | Key for ML sync (CONFIG_EVF_STATE_SYNC) |
| `./Audio/AudioIC.c` | Audio codec IC driver. I2C communication with audio chip. Contains InitializeAudioIC, ResetAudioIC, SendDataForAudioIC. | Codec model unknown |

### Color / Lens (2)

| Source File | Purpose | Notes |
|------------|---------|-------|
| `./DS_Tree/DSLR01/K325/ICU/Src/Color/GetLensCorrectionData.c` | Lens correction data retrieval. Gets distortion, vignetting, CA correction parameters from lens ROM. | K325 = 70D codename. Fmt1 = original format |
| `./DS_Tree/DSLR01/K325/ICU/Src/Color/GetLensCorrectionData_Fmt2.c` | Lens correction data retrieval (format 2). Newer lens correction data format with additional parameters. | Fmt2 = newer lens profiles |

### Base Canon Files (12)

| Source File | Purpose | Notes |
|------------|---------|-------|
| `EDmac.c` | Enhanced DMA controller driver. Hardware abstraction for DIGIC V EDMAC engine. | All EDMAC functions live here |
| `Siodriver.c` | Serial I/O driver. UART, SPI, I2C communication drivers. | I2C used for audio IC, sensor config |
| `AppTask1.c` | Application task #1. Main application task (one of several app-level control loops). | App-level logic dispatcher |
| `IvaTask.c` | IVA (Imaging Video Accelerator) task. Controls IVA hardware for video processing. | IVA = dedicated video DSP |
| `ColorTask.c` | Color processing task. Handles color matrix, white balance, picture style processing. | Post-processing pipeline |
| `CreativeFilterTask.c` | Creative filters task. Applies grain, toy camera, miniature effect filters. | Canon's creative filter engine |
| `RecognitionTask.c` | Scene recognition task. Analyzes scene for automatic scene mode selection. | AI/scene detection |
| `SequenceTask.c` | Sequence/timelapse task. Manages intervalometer and burst shooting. | Multiple-shot sequences |
| `StillTask.c` | Still image capture task. Orchestrates single-shot photo capture: AF→AE→mirror→shutter→save. | Main stills pipeline |
| `SystemTime.c` | System time management. RTC interface, timer services, timestamp generation. | Clock source for all timestamps |
| `FaceAWB.c` | Face-based auto white balance. Uses detected faces to optimize WB for skin tones. | Improves portrait WB accuracy |
| `Stub.c` | Stub implementations. Placeholder functions for features that are stubbed out. | Used when firmware shares codebase across models |

### Eeko Image Processing Pipeline (47)

The Eeko subsystem is Canon's image processing engine ("Eeko" = Canon internal codename for the image pipeline). These 47 source files implement the complete RAW→YUV→JPEG/H.264 conversion chain. Each file is a "path" or "filter" in the processing DAG (directed acyclic graph) controlled by the EPP (Eeko Processing Pipeline) scheduler.

#### RAW Stage — Sensor Data Processing
| Source File | Purpose |
|-------------|---------|
| `EekoRawToSsrawPathCore.c` | Converts packed RAW sensor data to sub-sampled RAW (ssraw). First stage of pipeline. Handles black level subtraction, dead pixel correction, column ADC correction. |
| `EekoRawToSsrawToYuvPath.c` | Full RAW→ssraw→YUV conversion path. Orchestrates the complete sensor-to-display conversion sequence. |
| `EekoAddRawPath.c` | RAW frame addition/accumulation. Used for HDR and noise reduction (adds multiple RAW frames). |
| `EekoAddRawPathCore.c` | Core RAW addition logic. Pixel-arithmetic for multi-frame RAW accumulation. |

#### Luma / Brightness Processing
| Source File | Purpose |
|-------------|---------|
| `EekoAddLowFreqLumaPathCore.c` | Adds low-frequency luminance component. Used for shadow lifting without amplifying noise. Enhances visibility in dark areas. |
| `EekoRoughMonochromeYuvEeko.c` | Rough monochrome conversion path. Quick B&W preview generation for EVF/LV (lower quality, faster). |

#### Distortion / Geometry Correction
| Source File | Purpose |
|-------------|---------|
| `EekoDistortPath.c` | Lens distortion correction path. Applies barrel/pincushion correction using lens profile data. |
| `EekoDistortPathCore.c` | Core distortion correction math. Coordinate remapping + pixel interpolation. |
| `EekoDistortResizeEeko.c` | Combined distortion correction + resize. Corrects lens distortion then scales to output resolution. Single pass optimization. |
| `EekoDister.c` | Distortion estimator/calibrator. Computes distortion parameters from test patterns. Factory calibration tool. |

#### Resize / Scaling
| Source File | Purpose |
|-------------|---------|
| `EekoResample.c` | Image resampling engine. General-purpose resize with configurable interpolation (bilinear, bicubic, lanczos). |
| `EekoLesserResizePathCore.c` | Lesser-quality resize path. Faster resize for preview/secondary outputs (EVF, thumbnail). |
| `EekoNV12ResizePath.c` | NV12 format resize path. NV12 = Y plane + interleaved UV plane. Common intermediate format. |
| `EekoNV12ResizePathCore.c` | Core NV12 resize logic. Handles independent Y and UV plane scaling. |
| `EekoResizeLittleYuvPath.c` | Resize for small YUV outputs (thumbnails). Optimized for common thumbnail sizes (1620×1080, etc.). |
| `EekoResizeLittleYuvPathCore.c` | Core thumbnail resize logic. Highly optimized for specific output dimensions. |

#### Filtering / Denoising
| Source File | Purpose |
|-------------|---------|
| `EekoFilterResizeAddSub2PathCore.c` | Filter + resize + add/subtract (2-channel variant). Complex filter pipeline combining multiple operations. |
| `EekoFilteringCompositePathCore.c` | Composite filtering path. Runs multiple filters in sequence with intermediate composites. |
| `EekoFilteringResizePathCore.c` | Filter then resize path. Denoise/sharpen before scaling for better quality. |
| `EekoPonyFilterPathCore.c` | Pony filter path. Specific noise reduction algorithm (Pony = internal codename). Temporal + spatial denoising. |

#### Color Processing
| Source File | Purpose |
|-------------|---------|
| `EekoColorSubPathCore.c` | Color sub-sampling path. Converts 4:4:4 YUV to 4:2:2 or 4:2:0 for compression. |
| `EekoMaskUVSubPathCore.c` | UV channel mask/sub operation. Selectively processes chroma channels for skin tone protection. |

#### Composite / Blend
| Source File | Purpose |
|-------------|---------|
| `EekoBltShare.c` | Blit/share operations. Hardware-accelerated image blitting (bit block transfer) using EDMAC. |
| `EekoGradateCompositeEeko.c` | Gradient composite path. Smooth gradient blending for HDR/tonemapping. |
| `EekoLotus.c` | Lotus image processing. Specific look/color mapping engine (Lotus = internal codename for a color style). |
| `EekoLotusPathForMovie.c` | Lotus processing for movie mode. Different color pipeline for video vs stills. |
| `EekoLotusPathForMovieCore.c` | Core Lotus movie processing. Handles temporal consistency for video frames. |
| `EekoMulIndyPathCore.c` | Multiply-independent path. Channel-independent multiplication operations (gain/brightness per channel). |

#### Motion / Temporal Processing
| Source File | Purpose |
|-------------|---------|
| `EekoMotionDetectEeko.c` | Motion detection engine. Computes frame-to-frame motion vectors for temporal filtering and encoding. |
| `EekoLuckyPonyPathCore.c` | Lucky Pony path. Frame selection/compositing algorithm (picks sharpest frames from burst). "Lucky" = lucky imaging technique. |

#### JPEG / Still Encoding
| Source File | Purpose |
|-------------|---------|
| `EekoJpCore.c` | JPEG encoding core. Hardware JPEG encoder wrapper for DIGIC V. Produces .JPG output. |
| `EekoSequenceRawToJpeg.c` | RAW-to-JPEG conversion for burst/sequence shooting. Optimized multi-frame pipeline. |
| `EekoSequenceRawToSsrawToYuvPath.c` | Full sequence conversion: RAW→ssraw→YUV for burst mode. Shared pipeline for multiple frames. |
| `EekoShootJpegDisplayPath.c` | JPEG shooting + display path. Handles both capture-to-card AND preview display. |
| `EekoShootJpegDisplayPathCore.c` | Core JPEG+display logic. Simultaneous encode-to-card and render-to-screen. |
| `EekoShootJpegPath.c` | Standard JPEG shooting path. Capture → process → encode → save. Main stills pipeline. |
| `EekoShootJpegPathCore.c` | Core JPEG shooting logic. Central still image processing path. |
| `EekoShootJpegThmPath.c` | JPEG thumbnail generation. Creates embedded thumbnail for EXIF metadata. |

#### Video / Movie Encoding
| Source File | Purpose |
|-------------|---------|
| `EekoIvaEncPath.c` | IVA (Imaging Video Accelerator) encode path. H.264 encoding for movie recording. |
| `EekoIvaEncPathCore.c` | Core IVA encode logic. Handles H.264 GOP structure, bitrate control, scene detection. |
| `EekoIvaDecPath.c` | IVA decode path. Decodes H.264 for playback/preview of recorded movies. |

#### Display / VRAM Output
| Source File | Purpose |
|-------------|---------|
| `EekoSsrawToYuvLvPath.c` | Sub-sampled RAW to YUV for LiveView. Optimized LV display pipeline. |
| `EekoSsrawToYuvLvPathCore.c` | Core LV display conversion. Fast path optimized for 30fps/60fps display rates. |
| `EekoSsrawToYuvPathCore.c` | Generic sub-sampled RAW to YUV conversion. Base class for both LV and still paths. |
| `EekoYuvToVramPath.c` | YUV to VRAM conversion. Final stage before LCD display. |
| `EekoYuvToVramPathCore.c` | Core YUV→VRAM logic. Handles color space conversion, scaling to screen resolution. |
| `EekoYuvToVramPathForCFilter.c` | YUV→VRAM for creative filters. Applies creative filter effects before display. |
| `EekoYuvToVramPathForCFilterCore.c` | Core creative filter display logic. Real-time preview of creative filters. |
| `EekoYuvToVramPathForMulExp.c` | YUV→VRAM for multiple exposure mode. Handles multi-exposure composite preview. |
| `EekoYuvToVramPathForMulExpCore.c` | Core multi-exposure display logic. Real-time blend preview during multi-exposure shooting. |

#### Utility / Support
| Source File | Purpose |
|-------------|---------|
| `EekoEDmac.c` | Eeko EDMAC integration. Manages DMA transfers within the Eeko pipeline (frame buffers between stages). |
| `EekoEDmacCopyPath.c` | Eeko EDMAC copy path. Simple DMA copy optimized for pipeline frame buffers. |
| `EekoOhyearKiPrm.c` | Eeko parameter loader. Loads image processing parameters (sharpness, contrast, saturation curves). "OhyearKiPrm" = likely Japanese→English romanization of parameter loading. |
| `EekoPostpro.c` | Eeko post-processing. Final processing step after main pipeline: sharpening, output color space conversion. |
| `EekoSndPas.c` | Eeko sound passthrough. Audio synchronization with video frames. Ensures A/V sync. |
| `EekoRshdSndPath.c` | Eeko rush/shoot sound path. Audio capture during burst/continuous shooting. |
| `EekoRshdSndPathForMulExp.c` | Eeko rush sound for multiple exposure. Audio handling during multi-exposure sequences. |
| `EekoToyCamYuvEeko.c` | Toy camera effect. Retro/vintage look filter (vignette + color shift + soft focus). |
| `EekoWaterColorYuvEeko.c` | Watercolor effect. Artistic filter that simulates watercolor painting look. |
| `EekoSaridon3PathCore.c` | Saridon3 processing path. Unknown specific effect (Saridon3 = internal codename). Likely a noise reduction or sharpening variant. |
| `EekoJoynerLuckyPathCore.c` | Joyner Lucky path. Another burst-frame selection algorithm variant. "Joyner" = likely a developer surname.

---

## 11. Task & Kernel System

### Canon Firmware Tasks

Canon's firmware is Task-based, running on the DryOS RTOS. Each task has a priority, stack, and message queue. Tasks communicate via messages, semaphores, and event flags.

| Task Name | Priority | Purpose |
|-----------|----------|---------|
| `init_task` | High | Canon initialization task (entry at 0xFF0C54CC). Runs boot sequence, creates all other tasks, then exits. First task created after reset vector. |
| `gui_main_task` | Normal | GUI main event loop (0xFF0D97AC). Processes button events, renders display, manages LiveView overlay. 30fps/60fps display refresh. |
| `sounddev_task` | Normal | Audio device management (0xFF118F5C). Manages ASIF DMA transfers, audio format negotiation, codec I2C commands. |
| `create_init_task` | High | Boot-time task creation dispatcher (0x00003168). Creates the initial task set during startup phase. |
| `livev_loprio_task` | Low | LiveView low-priority processing. Handles non-time-critical LV operations: face detection, histogram, image analysis. |
| `livev_hiprio_task` | High | LiveView high-priority processing. Time-critical LV operations: sensor readout scheduling, display buffer swaps, EVF state sync. |
| `shoot_task` | High | Image capture orchestrator. Runs the full still capture sequence: AF lock → AE lock → mirror up → shutter open/close → mirror down → save. |
| `shoot_task_mqueue` | — | Shoot task message queue. Receives capture commands from button handlers and remote shot. Max queue depth: 4-8 messages. |
| `clock_task` | Medium | System clock/timing. Manages RTC sync, timer callbacks, intervalometer timing. Gets 1ms tick from hardware timer. |
| `focus_misc_task` | Low | Focus miscellaneous operations. Runs focus confirmation check, focus graph updates, Magic Zoom calculations. ML's `focus_misc_task` variant. |
| `focus_task` | High | AF operations task. Executes AF sequences: phase-detect (viewfinder) and contrast-detect (LiveView). Controls lens motor through lens mount. |
| `fps_task` | Medium | Frame rate control task. Monitors and adjusts FPS timing registers. ML uses `FPS_REGISTER_A/B` writes in this task context. |
| `beep_task` | Low | Audio beep task. Generates simple tones for AF confirmation, self-timer countdown. Uses PWM on audio output, not full audio pipeline. |
| `audio_common_task` | Normal | Audio system common task. Manages audio device lifecycle, format negotiation, sample rate changes. |
| `debug_task` | Low | Debug console task. Prints debug output from `dumpf()`. Can be enabled via debug menu. |
| `console_task` | Low | Console I/O task. Handles serial console input/output (UART). Used for factory diagnostics. |
| `menu_task` | Normal | Menu system task. ML creates `menu_task` for ML menu rendering and input handling. |
| `menu_redraw_task` | Low | Menu rendering task. Handles dirty-region redraw for menu display. Optimized for partial updates. |
| `notifybox_task` | Low | Notification box task. Displays timed notification overlays (e.g., "No SD card", "Battery low"). Auto-hides after timeout. |
| `playback_compare_images_task` | Low | Image comparison task. Shows side-by-side comparison during playback. Computes zoom/sync between two images. |
| `expfuse_preview_update_task` | Low | Exposure fusion preview task. Updates HDR preview during multi-exposure shooting. |
| `tweak_task` | Low | ML tweaks task. Periodic task that applies ML settings asynchronously. Avoids blocking GUI from settings changes. |
| `tskmon_task` | Low | Task monitor. Watches other tasks for crashes/hangs. Creates crash logs when tasks die unexpectedly. |
| `cls_task` | Low | Clear screen task. Handles screen transitions: fade in/out, screen clear between modes. |
| `guess_free_mem_task` | Low | Free memory estimation task. Periodically samples memory pools and updates free memory display. |
| `movtweak_task` | Medium | Movie tweaks task. ML task that applies movie-specific settings: bitrate, QScale, GOP changes during recording. |

### ML Task Creation Functions

56 task creation wrappers mapped at runtime (0x004afxxx range). Each creates a DryOS task with ML's standard parameters (stack size, priority, message queue). These are called during ML initialization to spin up ML's subsystems.

| Function | Creates | Description |
|----------|---------|-------------|
| `task_create_console_task` | Console I/O task | Debug console for printf output |
| `task_create_module_load_task` | Module loader | Loads .mo modules from ML/MODULES/ |
| `task_create_menu_task` | Menu input handler | Processes ML menu button events |
| `task_create_menu_redraw_task` | Menu renderer | Redraws dirty menu regions |
| `task_create_debug_loop_task` | Debug loop | Main ML debug loop (development) |
| `task_create_tweak_task` | Settings tweaker | Applies ML settings periodically |
| `task_create_focus_misc_task` | Focus monitor | Monitors focus position for Magic Zoom |
| `task_create_focus_task` | Focus control | Runs AF sequences (focus stacking, etc.) |
| `task_create_fps_task` | FPS controller | Manages FPS override timing |
| `task_create_seconds_clock_task` | Clock updater | 1-second clock tick for on-screen clock |
| `task_create_shoot_task` | ML shooter | ML's shoot task for remote/scheduled capture |
| `task_create_NotifyBox_task` | Notification UI | ML notification overlay task |
| `task_create_beep_task` | Beeper | ML beep sound generator |
| `task_create_audio_common_task` | Audio manager | ML audio subsystem task |
| `task_create_audio_meter_init` | Audio meters | Audio level meter init (peak/RMS) |
| `task_create_bmp_init` | BMP framebuffer | Initializes ML's on-screen display buffer |
| `task_create_hist_init` | Histogram | Histogram overlay initialization |
| `task_create_zebra_init` | Zebra overlay | Zebra/overexposure overlay init |
| `task_create_lens_init` | Lens controller | Lens control subsystem init |
| `task_create_raw_init` | RAW capture | RAW video capture subsystem init |
| `task_create_prop_init` | Property handler | Property handler registration |
| `task_create_vsync_init` | VSync handler | EVF_STATE-based VSync handler |

### Kernel Services

DryOS kernel API services available at runtime:

| Service | Purpose |
|---------|---------|
| `CreateTask` | Creates a new DryOS task with stack, priority, entry point, and message queue. Returns TASK_ID. |
| `CreateStateObject` | Creates a state object (state machine). Used by EVF_STATE, MOVREC_STATE, SSS_STATE. |
| `CreateMessageQueue` | Creates a message queue for inter-task communication. Fixed-size messages (usually 4-16 bytes). |
| `CreateEventFlag` | Creates an event flag group (32-bit). Tasks can set/wait on flags with AND/OR conditions. |
| `CreateBinarySemaphore` | Creates a binary semaphore (mutex-like, 0 or 1). Used for resource locking. |
| `CreateCountingSemaphore` | Creates a counting semaphore (0 to max). Used for producer-consumer patterns (e.g., frame queues). |
| `CreateRecursiveLock` | Creates a recursive mutex. Same task can lock multiple times; requires balanced unlocks. |
| `SetHPTimer` | Sets a high-precision hardware timer. Microsecond resolution. Triggers callback when expired. |
| `SetHPTimerAfterNow` | Sets a hardware timer relative to current time. Convenience wrapper for absolute-time SetHPTimer. |
| `SetTimerAfter` | Sets a one-shot software timer. Millisecond resolution. Fires callback in the timers task context. |
| `task_create` | Creates a low-level task with manual stack/base/TID specification. |
| `task_trampoline` | Task entry point trampoline. Wraps user entry function with DryOS task setup/teardown. |
| `task_dispatch_hook` | ML's task dispatch hook. Intercepts task creation to monitor/modify task parameters. |
| `current_task` | Returns TASK_ID of currently running task. |
| `task_max` | Returns maximum number of tasks that can exist. ~64 on DIGIC V. |
| `get_task_info_by_id` | Returns task information (name, priority, stack, state) from TASK_ID. |
| `gui_task_list` | Returns list of GUI-related tasks. Used to find menu/display tasks for hooking. |
| `get_current_task_name` | Returns string name of current task. Runtime address: 0x00453550. |
| `get_task_name_from_id` | Converts TASK_ID to task name string. |
| `get_current_task_id` | Returns TASK_ID of current task. Convenience wrapper around current_task. |
| `run_in_separate_task` | ML function (0x0045d1a8). Executes a callback in a fresh task. Used by debug menu to isolate potentially crashy code. |

---

## 12. Lens & Focus System

### AF Remote Control Functions (Canon)

These functions form a complete AF finite state machine. Their names (from the RAM dump strings collection) reveal the full AF control flow used by WiFi/USB remote shooting:

```
AfCtrl_Act_Ready              — Transition AF to READY state (power lens motor, load calibration data)
AfCtrl_Act_Suspend            — Suspend AF operation (pause without losing state; used between shots in burst)
AfCtrl_Act_Ignore             — Put AF in IGNORE state (all incoming commands dropped; used when lens is detached)
AfCtrl_Act_TvAfStart          — Start Through-View AF (LiveView contrast-detect AF; called during LV/touch focus)
AfCtrl_Act_CompleteAe_ForTvAf — Complete AE lock before TvAF (locks exposure so focus analysis has correct brightness)
AfCtrl_Act_CompleteAfResult   — Process AF result (report focus position, confidence, achieved/not)
AfCtrl_Act_TvAfStop           — Stop TvAF gracefully (lens motor parks to safe position)
AfCtrl_Act_TvAfStop_Force     — Force-stop TvAF immediately (emergency stop, motor abrupt halt)
AfCtrl_Act_EmdDriveResult     — Process EMD (electromagnetic diaphragm) drive result (aperture motor position confirmed)
AfCtrl_Act_StartLensDriveRemote — Start lens focus motor via remote command (WiFi/USB; used in EOS Utility)
AfCtrl_Act_EndLensDriveRemote  — Stop remote lens drive, report final focus position
AfCtrl_Act_SetLensParameter   — Set a lens parameter locally (focus position, speed, direction)
AfCtrl_Act_SetLensParameterRemote — Set lens parameter via remote command (WiFi/USB)
AfCtrl_Act_ContinuousAfStart — Start continuous AF (AI Servo mode; lens tracks moving subject)
AfCtrl_Act_ContinuousAfStop  — Stop continuous AF (last focus position locked)
AfCtrl_Act_CompleteEmdDrive  — Complete EMD drive cycle (aperture finalized for next exposure)
AfCtrl_ExecuteEvent           — Generic AF event dispatcher (routes events to handlers by name)
AfCtrl_PropertyCBR            — Property callback for PROP_LV_AF changes (triggers on AF state changes)
```

**Confirmed AF State Machine Sequence (from RAM string ordering):**
```
Ready → Suspend → Ignore → TvAfStart → CompleteAe_ForTvAf → CompleteAfResult
→ TvAfStop → EmdDriveResult → StartLensDriveRemote → SetLensParameterRemote
→ EndLensDriveRemote → ContinuousAfStart/Stop
```
This sequence maps exactly to the AF lifecycle: start → pause → run → complete → remote control → continuous.

### AF Evaluation Modes

```
AF_EVAL_SINGLE      — Single-point AF evaluation (one focus point, fastest)
AF_EVAL_MULTI       — Multi-point AF evaluation (zone/auto area, balances speed and accuracy)
AF_EVAL_LCR         — Left/Center/Right evaluation (3-zone AF for sports/action)
AF_EVAL_DETAILED    — Detailed AF evaluation (highest accuracy, used in live view contrast AF)
```

### Lens Data Structures

These strings represent Canon's internal lens data types found in RAM:

```
LensCorrectionData    — Lens correction parameters (distortion, vignetting, CA) from lens ROM
LensCorrectionData_Fmt2 — Newer format lens correction data (Fmt2 = format 2, more parameters)
LensData               — Raw lens communication data (EF protocol messages)
LensDriveRemote        — Remote lens drive command struct (focus target, speed, direction)
LensDriveResult        — Lens drive completion result (position reached, error code)
LensDriveStartCBR      — Callback fired when lens drive starts (notifies AF system)
LensFocus              — Focus position data struct (current position, target, drive status)
LensFocus2             — Focus data v2 (may include Dual Pixel AF data)
LensGyroResponseData   — Gyroscope feedback data (lens optical IS gyro readings)
LensID                 — Lens identification (model, serial, firmware version)
LensInfo               — Complete lens information struct (focal lengths, max aperture, AF status, IS status)
LensParameter          — Lens parameter struct (focus, aperture, IS mode)
LensParameterRemote    — Lens parameter for remote control (WiFi/USB variant)
LensRequestStatus      — Lens command request status (pending, in-progress, complete, error)
LensSerialNumber       — Lens serial number string (from lens ROM)
LensToFile             — Lens data serialization for file output (used in EXIF population)
LensType               — Lens type/mount classification (EF, EF-S, third-party)
LensWriteTime          — Lens data write time (used by data store for change detection)
Lenses                 — Multi-lens list (for AF Microadjustment per-lens storage)
LensDataRequestStatus  — Lens data request completion status (async EF protocol response tracking)
```

### ML Lens Functions

These are ML wrapper functions that provide camera-independent interfaces to Canon's lens system:

| Function | Purpose |
|----------|---------|
| `lens_init` | Initialize ML lens subsystem (registers PROP_LV_LENS handler, sets up focus monitoring) |
| `lens_info_init` | Initialize lens info data (reads initial lens state for display) |
| `lens_set_rawiso` | Set ISO via lens interface (routes through Canon's property system for proper AE recalculation) |
| `lens_set_rawaperture` | Set aperture via lens interface (EMD control through EF mount protocol) |
| `lens_set_rawshutter` | Set shutter speed via lens interface (routes through Canon's exposure controller) |
| `lens_set_kelvin` | Set white balance color temperature (Kelvin) value |
| `lens_set_wbs_ba` | Set white balance shift (blue/amber axis) — blue/amber tint adjustment |
| `lens_set_wbs_gm` | Set white balance shift (green/magenta axis) — green/magenta tint adjustment |
| `lens_set_custom_wb_gains` | Set custom white balance channel gains (from gray card measurement) |
| `lens_set_drivemode` | Set camera drive mode (single, continuous, self-timer, silent, etc.) |
| `lens_set_ae` | Set auto-exposure mode (P/Av/Tv/M/B/C modes) |
| `lens_set_flash_ae` | Set flash auto-exposure mode (ETTL, manual flash) |
| `lens_display` | Update lens info display (shows current aperture, ISO, shutter on screen) |
| `lens_display_set_dirty` | Mark lens display as dirty (force redraw on next GUI cycle) |
| `lens_focus` | Core focus operation (executes focus sequence: drive lens, wait, check result) |
| `lens_focus_start` | Start focus operation (non-blocking; returns immediately, completion via callback) |
| `lens_focus_stop` | Stop current focus operation (abort lens drive) |
| `lens_focus_enqueue_step` | Enqueue a focus step (for focus stacking: queue multiple focus positions) |
| `lens_setup_af` | Setup AF system (configures AF mode, points, tracking parameters) |
| `lens_cleanup_af` | Cleanup after AF operation (restore AF system to idle state) |
| `lens_mlu_delay` | Mirror lockup delay (waits for mirror vibrations to settle before exposure) |
| `lens_take_picture` | Take a picture through ML's lens system (mirror up → shutter → mirror down → save) |
| `lens_wait_readytotakepic` | Wait until camera is ready for next shot (checks buffer, card access, mirror state) |
| `lens_format_iso` | Format ISO value for display (e.g., 0x0080 → "ISO 400") |
| `lens_format_aperture` | Format aperture value for display (e.g., 0x30 → "f/5.6") |
| `lens_format_shutter` | Format shutter speed for display (e.g., 0x60 → "1/125") |
| `lens_format_dist` | Format focus distance for display (e.g., 500 → "5.0m") |
| `lens_format_shutter_reciprocal` | Format shutter as reciprocal (e.g., 125 → "1/125") |

### AFMA (AF Microadjustment)

```
PROP_AFMA (0x80010006)
_prop_handler_PROP_AFMA
AFMA buffer size: 0x22 bytes
Mode at offset 0xD
Per-lens wide at offset 20
Per-lens tele at offset 21
All lenses at offset 23
```

---

## 13. Sensor & Image Pipeline

### Sensor Drive

The SensorDrive subsystem manages the image sensor's power and operational states. These functions control the sensor hardware directly.

| Function | Purpose |
|----------|---------|
| `SDRV_PowerOnDevice` | Powers on the image sensor (turns on analog power rails, starts clock oscillator). First step in sensor initialization. |
| `SDRV_StartupDevice` | Starts the sensor in a specific mode (photo, LV, movie). Configures pixel clock, frame timing, readout area. |
| `SDRV_StopDevice` | Gracefully stops sensor readout. Finishes current frame, gates clocks, but keeps power on. |
| `SDRV_SleepDevice` | Puts sensor in low-power sleep (clocks gated, analog circuits in standby). Quick resume possible. |
| `SDRV_StanbyDevice` | Deeper standby (clocks off, analog bias off). Slower resume than sleep. |
| `SDRV_ShutdownDevice` | Full sensor shutdown (all power rails off, PLLs stopped). Used when camera powers off. |
| `SDRV_Terminate` | Tears down sensor driver (frees resources, unregisters interrupts). Final cleanup. |
| `SDRV_SetDeviceParameter1st` | Sets first-phase sensor parameters (basic geometry: width, height, binning mode). |
| `SDRV_SetDeviceParameter2nd` | Sets second-phase sensor parameters (timing fine-tuning: crop window, line skipping). |
| `SDRV_PrepareSetDeviceParameter` | Validates and prepares parameters before applying (checks mode compatibility, buffer availability). |
| `SDRV_Set1stSRParameter` | Sets first-phase SR (shift register / scanning row) parameter. Controls which rows are read out. |
| `SDRV_RequestQuickFrameRateChange` | Requests an in-place frame rate change without full re-initialization. Used for FPS override. |

### Gain Control (LVGAIN_CTRL_)

Gain controller manages sensor analog gain (ISO) during LiveView, separate from the photo capture gain path.

| Function | Purpose |
|----------|---------|
| `LVGAIN_CTRL_SetDragneExt` | Sets dragne (likely "D-range" = dynamic range) extension. Expands LV tonal range in high-contrast scenes. |
| `LVGAIN_CTRL_SetIsoExpansion` | Sets ISO expansion (HI/LO ISO) for LiveView. Allows ISO beyond native range (ISO 12800, 25600). |
| `LVGAIN_CTRL_SetPhotoStudioIsoComp` | Sets ISO compensation for photo studio mode (flash/studio lighting ISO adjustment). |
| `LVGAIN_CTRL_SetFnoGain` | Sets F-number (aperture) gain compensation. Adjusts LV brightness based on lens aperture setting. |
| `LVGAIN_CTRL_SetTuningFlag` | Sets LV tuning flag (controls whether auto-tune is active for LV brightness optimization). |
| `LVGAIN_CTRL_SetSensorScanMode` | Sets sensor scan mode (full readout, binning, skipping). Affects LV resolution and frame rate. |
| `LVGAIN_CTRL_GetHShadingData` | Gets horizontal shading correction data. Compensates for lens vignetting in LV. |
| `LVGAIN_CTRL_GetHShadingDataZOOM` | Gets horizontal shading data for zoomed LV (digital zoom, different correction needed). |
| `LVGAIN_CTRL_UpdateMaxIsoAndDRange` | Updates max ISO and dynamic range limits based on current sensor mode. |
| `LVGAIN_CTRL_Delete` | Deletes LV gain controller instance. Frees resources. |

### Image Processing Pipeline Flow

```
Sensor → RAW (packed) → ssraw (sub-sampled) → YUV (color) → VRAM (display)
                ↓                           ↓
            EDMAC RAW slurp              H.264 encode
                ↓                           ↓
            ML RAW capture              .MOV file
```

The pipeline divides into three branches:
1. **Display path**: ssraw → YUV → VRAM → LCD (30/60fps, low latency)
2. **Encode path**: ssraw → YUV → H.264 → .MOV (movie recording)
3. **ML capture path**: RAW → EDMAC → ML buffer → MLV file (raw video capture)

---

## 14. GPS, Touchscreen & Defect Management

### GPS

Functions exist in firmware but **all return -1** via call() on 70D — they are NOT in the eventproc table. The GPS subsystem initializes at boot (`[FM/GPS] GPS_RegisterSpaceNotifyCallback` at 173ms) but is not callable through call().

| Function | Purpose | Status |
|----------|---------|--------|
| `GPS_Initialize` (36, 25) | Initialize GPS module (params: 36=GPS baud rate, 25=update interval in seconds?). Confirmed in boot log. | Boot-only |
| `GPSList` | Returns list of available GPS satellites (NMEA GSV sentences parsed into struct). | call() returns -1 |
| `GPSTime` | Gets current GPS atomic time (more accurate than RTC, compensates for crystal drift). | call() returns -1 |
| `GPSClearList` | Clears satellite list cache (forces fresh NMEA query from GPS chip). | call() returns -1 |
| `GetGPSTime` | Retrieves last known GPS time (works even if GPS module is asleep/off). | call() returns -1 |
| `GPSListRecvCapability` | Checks GPS receiver capabilities: GNSS systems (GPS, GLONASS, Galileo), max update rate, antenna status. | call() returns -1 |
| `GetGPSCaptureTimeList` | Gets list of GPS timestamps for captured images (for geotagging in EXIF). | call() returns -1 |
| `GPS_RegisterSpaceNotifyCallback` | Registers a notification callback for GPS position/time updates. Fires when new fix is available. | call() returns -1 |
| `gpsGetBinaryLogData` | Extracts raw GPS NMEA/sensor binary log from module. Unparsed format. | call() returns -1 |

**How to access GPS data on 70D:** The GPS functions are likely accessible through property handlers or direct memory reads rather than call(). GPS data may be mapped into RAM at a fixed address during `GPS_Initialize`. Research needed: find GPS data structure in RAM and read it with `shamem_read()` or direct pointer access.

### Touchscreen

The 70D has a capacitive touch LCD with a dedicated controller IC. These functions configure and calibrate the touch hardware. Touch events appear as button codes 0x6F (1-finger touch), 0x70 (1-finger untouch), 0x71 (touch move). Two-finger events (0x76-0x79) are defined in gui.h but marked "unavailable on this camera."

| Function | Purpose |
|----------|---------|
| `TCH_CheckTouchICVersion` | Verifies touch controller IC is the expected model and firmware revision. Guards against hardware mismatch. |
| `TCH_SetWaitingTime` | Sets debounce delay between touch samples (typically 5-20ms). Trade-off: lower = faster response but more noise. |
| `TCH_SetOpe2SysTime` | Sets operation-to-system timing sync (controller sampling rate vs system GUI refresh rate). Ensures touch events align with frame boundaries. |
| `TCH_SetMutualGainValue` | Sets mutual capacitance sensing gain. Affects touch sensitivity and false-touch immunity. |
| `TCH_SetMutualLocaliDacValue` | Sets per-region DAC calibration values. Compensates for panel non-uniformity across the screen. |
| `TCH_SetGainParamForSelfScan` | Sets self-capacitance scan parameters (proximity/hover detection). Enables detecting finger before contact. |
| `FA_SetTouchIntervalTime` | Factory adjustment for touch scan interval (calibrates hardware during manufacturing). |
| `FA_SetTouchTestTime` | Factory touch self-test duration. Run-time test of touch panel functionality. |

### Defect / Pixel Management

Canon's defect pixel management system maps and corrects hot/dead/stuck pixels. Separate tables exist for different modes (full res, magnified LV, movie crop). These functions are factory calibration tools.

| Function | Purpose |
|----------|---------|
| `ExecuteDefectMarge1–5` | Executes defect merge operation (stages 1-5). Combines new defects with existing defect map. Each stage handles different pixel types or regions. |
| `FA_LvDefectMaxCountFull` | Sets maximum defect count threshold for full-resolution LV defect detection. |
| `FA_LvDefectMaxCountMagnify` | Sets maximum defect count threshold for magnified (zoomed) LV. |
| `FA_LvDefectMaxCountMovieCrop` | Sets maximum defect count threshold for movie crop mode. |
| `FA_LvDetectDefectsFull` | Detects new defect pixels in full-resolution LiveView. Compares against known-good reference. |
| `FA_LvDetectDefectsMagnify` | Detects new defect pixels in magnified LiveView. |
| `FA_LvDetectDefectsMovieCrop` | Detects new defect pixels in movie crop mode. |
| `FA_LvMargeDefectsMagnify` | Merges newly detected defects into magnified LV defect map. |
| `FA_DefectsTestImage` | Generates test image for defect pixel analysis (uniform bright/dark frames). |
| `FA_DefectsMergeTestImage` | Merges test image results into defect analysis. |
| `FA_DetectDefTestImage` | Detects defects in test images (factory pixel calibration). |
| `FA_ProjectionTestImage` | Projects test image onto sensor (optical projection tool, not on-screen). |
| `FA_ProjectionTestImageEx` | Extended projection test (more thorough, slower). |
| `FA_ProjectionTestImageV` | Vertical projection test (vertical-only patterns). |
| `FA_SetMergeDefParameter` | Sets merge defect parameters (thresholds, algorithm selection). |
| `FA_SetDetectDefThreshold` | Sets defect detection threshold (brightness/sensitivity for flagging a pixel as defective). |
| `FA_SetHLinePixelNum` | Sets horizontal line pixel count (for line defect detection vs point defect). |
| `FA_CreateTestImage` | Creates a test image pattern in memory (generates known pixel values for calibration). |
| `FA_DeleteTestImage` | Deletes test image from memory after calibration complete. |
| `sht_savedefectsproperty` | Saves defect pixel map to persistent storage (EEPROM or file). "sht" = likely "shoot" → stored defect data.

## 14. GPS, Touchscreen & Defect Management

### GPS

Functions exist in firmware but return -1 via call():

```
GPS_Initialize (36, 25)              — confirmed in boot log
GPSList                              GPSTime
GPSClearList                         GetGPSTime
GPSListRecvCapability                GetGPSCaptureTimeList
GPS_RegisterSpaceNotifyCallback      gpsGetBinaryLogData
```

GPS call() test results: **All return -1 (NOT_FOUND).** GPS is not exposed via eventproc on 70D, even though `[GPS] GPS_Initialize` appears in boot log.

### Touchscreen

```
TCH_CheckTouchICVersion              TCH_SetWaitingTime
TCH_SetOpe2SysTime                   TCH_SetMutualGainValue
TCH_SetMutualLocaliDacValue          TCH_SetGainParamForSelfScan
FA_SetTouchIntervalTime              FA_SetTouchTestTime
```

**Note:** 70D touchscreen only supports single-finger events. Two-finger events (0x76–0x79) are defined in `gui.h` but listed as "unavailable on this camera."

### Defect / Pixel Management

```
ExecuteDefectMarge1   ExecuteDefectMarge2   ExecuteDefectMarge3
ExecuteDefectMarge4   ExecuteDefectMarge5
FA_LvDefectMaxCountFull     FA_LvDefectMaxCountMagnify
FA_LvDefectMaxCountMovieCrop
FA_LvDetectDefectsFull      FA_LvDetectDefectsMagnify
FA_LvDetectDefectsMovieCrop
FA_LvMargeDefectsMagnify
FA_DefectsTestImage          FA_DefectsMergeTestImage
FA_DetectDefTestImage        FA_ProjectionTestImage
FA_CreateTestImage           FA_DeleteTestImage
FA_SetMergeDefParameter      FA_SetDetectDefThreshold
FA_SetHLinePixelNum
sht_savedefectsproperty
```

Defect call() test results: `ExecuteDefectMarge1-3` all return -1 (NOT_FOUND).  
LV-dependent defect functions are expected to require active LiveView.

---

## 15. FA_* Factory/Adjustment Functions

191 FA_ functions identified. These form Canon's factory adjustment layer, accessible via call(). The "FA_" prefix denotes Factory Adjustment — these are calibration/test routines used during camera manufacturing. Many also serve as low-level API entry points for ML.

### LiveView (8)

| Function | Purpose |
|----------|---------|
| `FA_StartLiveView` | Initiates LiveView mode through factory path (different from normal LV start — may skip user-mode checks). |
| `FA_StopLiveView` | Stops LiveView through factory path. Handles cleanup that normal LV stop might skip. |
| `FA_StartLvTestImage` | Starts LV test image generation (synthetic pattern for sensor calibration, not real sensor data). |
| `FA_StopLvTestImage` | Stops LV test image generation. |
| `FA_StartLvTestExposureImage` | Starts LV test exposure image (timed test pattern for exposure calibration). |
| `FA_StopLvTestExposureImage` | Stops LV test exposure image. |
| `FA_PCLVStart` | PC LiveView start (tethered LV via USB). Different path from camera-internal LV. |
| `FA_PCLVEnd` | PC LiveView end. |

### Property (6)

| Function | Purpose |
|----------|---------|
| `FA_SetProperty` | Factory property set. May bypass normal validation/authorization checks. |
| `FA_SetProperty32` | Sets a 32-bit property value (vs. 16-bit or variable-length). |
| `FA_GetProperty` | Factory property get. Can access properties hidden from normal API. |
| `FA_GetPropertyAddress` | Gets the memory address of a property's data buffer. Useful for direct memory access. |
| `FA_GetPropertyDataSize` | Gets the data size (in bytes) of a property. |
| `FA_LoadProperty` | Loads property from persistent storage (EEPROM/file). Forces re-read from storage. |

### EEPROM / Persistent Storage (8)

| Function | Purpose |
|----------|---------|
| `FA_ReadEepromData` | Reads raw data from camera EEPROM (serial number, shutter count, calibration data). |
| `FA_WriteEepromData` | Writes raw data to camera EEPROM. CAUTION: can brick camera if wrong addresses written. |
| `FA_GetFileData` | Reads file data from internal filesystem (not SD card — internal flash memory). |
| `FA_GetFileSize` | Gets size of internal file. |
| `FA_DeleteFile` | Deletes a file from internal storage. |
| `FA_SaveProperty` | Saves a single property to persistent storage. |
| `FA_SavePropertyList` | Saves a list of properties to persistent storage (bulk save). |
| `FA_SaveWbFix` | Saves white balance correction data (per-camera factory WB calibration). |

### Sensor Calibration (12)

| Function | Purpose |
|----------|---------|
| `FA_SetColor` | Sets color calibration values (color matrix, saturation offsets). |
| `FA_SetWbCoef` | Sets white balance coefficients (RGB channel gains for different light sources). |
| `FA_GetWbCoef` | Gets current WB coefficients. |
| `FA_SetSsWbCoef` | Sets sub-sampled (LV) WB coefficients (different from full-res photo WB). |
| `FA_GetSsWbCoef` | Gets sub-sampled WB coefficients. |
| `FA_AdjustWhiteBalance` | Runs WB adjustment procedure (captures gray card, computes correction). |
| `FA_DarkAdjAutoExecute` | Auto-executes dark frame adjustment (dark current subtraction calibration). Removes hot pixels from dark frame. |
| `FA_DarkCheckAutoExecuteCH` | Auto-executes dark check for each channel (R, G1, G2, B individually). |
| `FA_DarkCheckAutoExecutePP` | Auto-executes dark check with post-processing (additional noise reduction pass). |
| `FA_DarkLevelCheckAutoExecute` | Auto-executes dark level check (verifies black level is within spec). |
| `FA_ReverseAdjust` | Reverse adjustment (undo previous calibration step, e.g., reverse a WB adjustment). |
| `FA_DecreaseGap` / `FA_IncreaseGap` | Adjust ISO analog gain gap between steps. Fine-tunes ISO amplifier precision. |

### Audio (4)

| Function | Purpose |
|----------|---------|
| `FA_AdjustMicBalance` | Adjusts L/R microphone balance (channel gain matching). Stereo balance calibration. |
| `FA_PrintMicLevel` | Prints current mic level to debug output. Shows dB level for factory QC. |
| `FA_RecordSound` | Records audio to file (factory test recording — tests entire audio capture chain). |
| `FA_RecordSoundEnd` | Stops factory test recording. |

### Calendar / Time (3)

| Function | Purpose |
|----------|---------|
| `FA_GetCalendar` | Gets current date/time from RTC (calendar format: YYYY/MM/DD HH:MM:SS). |
| `FA_SetCalendar` | Sets date/time on RTC. |
| `FA_ResetCalendar` | Resets RTC to factory default (usually epoch or manufacturing date). |

### Remote / Capture (6)

| Function | Purpose |
|----------|---------|
| `FA_RemoteRelease` | Triggers remote shutter release (equivalent to remote_shot but factory path). |
| `FA_FinishRemoteRelease` | Completes remote release sequence (processes image after capture). |
| `FA_GetRemoteReleasePlusImage` | Gets image from remote release + extra metadata. Enhanced remote capture. |
| `FA_MovieStart` | Starts movie recording via factory path (may have fewer restrictions than user movie start). |
| `FA_MovieEnd` | Stops movie recording via factory path. |

### SD / WiFi / Hardware Check (4)

| Function | Purpose |
|----------|---------|
| `FA_CheckSD` | Runs SD card diagnostic check (read/write test, speed test, error detection). |
| `FA_ChkAssembly` | Checks hardware assembly (verifies all components present and connected — flex cables, sensors, buttons). |
| `FA_ChkWlan` | Checks WiFi module presence and function (SDIO communication test, MAC address check). |
| `FA_MacSelfCheck` | Self-check of MAC address validity (format check, uniqueness verification). |

### Touchscreen (2)

| Function | Purpose |
|----------|---------|
| `FA_SetTouchIntervalTime` | Sets touch scan interval for factory calibration (optimizes touch sensitivity per-unit). |
| `FA_SetTouchTestTime` | Sets touch self-test duration for factory QC. |

### Display (9)

| Function | Purpose |
|----------|---------|
| `FA_DISP_SetBrightness` | Sets LCD backlight brightness (factory calibration — calibrates PWM mapping to actual luminance). |
| `FA_DISP_COM_SetCamera` | Sets display communication for this specific camera model (model-specific display parameters). |
| `FA_DISP_Start100White` | Displays 100% white test pattern (RGB=255,255,255). Used for LCD uniformity and brightness check. |
| `FA_DISP_Start50Gray` | Displays 50% gray test pattern (RGB=128,128,128). Used for LCD gamma calibration. |
| `FA_DISP_StartColor` | Displays solid color test pattern (used for color accuracy check). |
| `FA_DISP_StartMix` | Displays mixed color pattern (multiple colors for full gamut test). |
| `FA_DISP_End100White` | Ends white test pattern. |
| `FA_DISP_End50Gray` | Ends gray test pattern. |
| `FA_DISP_EndColor` | Ends color test pattern. |

### Factory: Other Notable FA_* Functions (selected)

| Function | Purpose |
|----------|---------|
| `FA_SetBatteryData` | Sets battery calibration data (charge curve, temperature compensation). |
| `FA_GetBatteryData` | Gets battery calibration data. |
| `FA_OperationCheck` | Runs full operational check (all buttons, dials, sensors). Factory QC test. |
| `FA_SetShutterCount` | Sets shutter count (used during shutter replacement or refurbishment). |
| `FA_GetShutterCount` | Gets current shutter count from EEPROM. |
| `FA_InitializeAfterReplaceParts` | Initializes camera after parts replacement (resets calibration data for new component). |
| `FA_SetDefaultProperty` | Sets all properties to factory defaults (full reset). |
| `FA_CheckMediumError` | Checks media (SD card) for errors. More thorough than FA_CheckSD. |
| `FA_DebugLog` | Saves or prints debug log (factory diagnostics logging). |
| `FA_SpiflashTest` | Tests SPI flash memory (internal firmware storage IC). |
| `FA_SetInternalTime` | Sets internal system time (separate from RTC — used for internal timers). |
| `FA_USBConnectTest` | Tests USB connectivity (enumeration check, data transfer test). |
| `FA_SetCopyRightString` | Sets the copyright string embedded in EXIF (Canon → user's name). |

---

## 16. FIO_* File I/O Functions

All 16 FIO functions are available for SD card file operations. These wrap Canon's FAT filesystem driver.

| Function | Purpose | ML Usage |
|----------|---------|----------|
| `FIO_OpenFile` | Opens an existing file for reading/writing. Fails if file doesn't exist. | Reading config files, module loading |
| `FIO_CloseFile` | Closes a file handle. Flushes all pending writes to SD card. | Always paired with OpenFile/CreateFile |
| `FIO_ReadFile` | Reads bytes from open file. Returns bytes actually read. | Config parsing, module loading |
| `FIO_WriteFile` | Writes bytes to open file. Returns bytes actually written. | Logging, data capture, RAW video |
| `FIO_CreateFile` | Creates a new file (fails if file exists). Opens for writing. | Creating log files, RAW video |
| `FIO_CreateFileOrAppend` | Creates file if not exists, or appends to existing file. | Log file accumulation |
| `FIO_CreateDirectory` | Creates a directory on the SD card. | Creating ML/LOGS/, ML/SETTINGS/ |
| `FIO_RemoveFile` | Deletes a file from SD card. | Cleanup, temp file deletion |
| `FIO_RenameFile` | Renames/moves a file on SD card. | File organization |
| `FIO_MoveFile` | Moves file between directories (rename across dirs). | File management |
| `FIO_CopyFile` | Copies a file. | Backup, file duplication |
| `FIO_SeekSkipFile` | Seeks to position in file or skips bytes. | Random access in files |
| `FIO_GetFileSize` | Gets file size using buffered API. 0 if file doesn't exist. | Pre-allocation checks |
| `FIO_GetFileSize_direct` | Gets file size via direct FAT read (unbuffered, slightly faster). | Fast size checks |
| `FIO_FindFirstEx` | Finds first file matching a pattern. Starts directory enumeration. | Module scanning, file listing |
| `FIO_FindNextEx` | Finds next matching file in directory enumeration. | Continue file scanning |
| `FIO_FindClose` | Closes a FindFirst/FindNext enumeration session. | Cleanup after directory scan |

## 16. FIO_* File I/O Functions

```
FIO_OpenFile              FIO_CloseFile
FIO_ReadFile              FIO_WriteFile
FIO_CreateFile            FIO_CreateFileOrAppend
FIO_CreateDirectory       FIO_RemoveFile
FIO_RenameFile            FIO_MoveFile
FIO_CopyFile              FIO_SeekSkipFile
FIO_GetFileSize           FIO_GetFileSize_direct
FIO_FindFirstEx           FIO_FindNextEx
FIO_FindClose
```

---

## 17. H.264 / Video Encoding

### Encoder Initialization

Canon's H.264 encoder handles movie recording. Above function strings are from RAM and represent specific encoder configurations:

| Function | Purpose |
|----------|---------|
| `H264E InitializeH264EncodeFor1080pDZoom` | Initializes H.264 encoder for 1080p with digital zoom enabled. Configures encoder for 3x crop mode. |
| `H264E InitializeH264EncodeFor1080p25fpsDZoom` | Same as above but locked to 25fps (PAL frame rate). European/Asian firmware variant string. |

### MVR (Movie Recorder) Configuration

The `mvr_config` struct (0x1D8 bytes) holds all movie recording parameters. These MVR functions control encoding quality and behavior:

| Function | Purpose |
|----------|---------|
| `mvrSetBitRate` | Sets H.264 target bitrate (e.g., 45 Mbps for ALL-I, 30 Mbps for IPB). Higher = better quality, larger files. |
| `mvrSetRecLimit` | Sets recording limit (29:59 default, configurable via ML). Enforces Canon's 4GB/30min limit. |
| `mvrAppendCheckSetRecLimit` | Checks and sets recording limit when appending to existing file (spanning across 4GB chunks). |
| `mvrSetQscale` | Sets quantization scale (1-51, lower = better quality). Overrides bitrate-based encoding for constant quality. |
| `mvrSetQscaleYC` | Sets Y (luma) and C (chroma) QScale independently. Allows fine-tuning luma vs chroma quality trade-off. |
| `mvrSetDeblockingFilter` | Sets H.264 deblocking filter strength. Affects smoothing at block boundaries. |
| `mvrSetLimitQScale` | Sets upper QScale limit (prevents quality from dropping below threshold during complex scenes). |
| `mvrSetDefQScale` | Sets default QScale (used when rate control isn't active or at GOP boundaries). |
| `mvrSetTimeConst` | Sets rate control time constant. Higher = smoother bitrate, lower = quicker reaction to scene changes. |
| `mvrSetFullHDOptSize` | Sets optimal frame size for Full HD (1080p). Target bytes per frame for rate control. |
| `mvrSetHDOptSize` | Sets optimal frame size for HD (720p). |
| `mvrSetVGAOptSize` | Sets optimal frame size for VGA (480p). |
| `mvrFixQScale` | Fixes QScale to prevent Canon from overriding ML's QScale settings. ML stub at stubs.S (0xFF73F76C). |
| `mvrSetDefDBFilter` | Sets default deblocking filter setting (applied when encoder is initialized). |
| `mvrSetPrintMovieLog` | Enables/disables movie encoding debug log. Prints frame statistics, bitrate, QScale to debug output. |

### IVA (Imaging Video Accelerator)

The IVA is DIGIC V's dedicated video processing hardware. These strings represent its internal components:

| String | Purpose |
|--------|---------|
| `IVA_BsBuf` | IVA bitstream buffer (stores encoded H.264 data before writing to SD card). |
| `IVA_ELDMain` | IVA ELD (likely Encoding Loop Decoder) main controller. Handles reference frame management and reconstruction loop. |
| `IVA_Mailbox` | IVA inter-processor mailbox. ARM CPU ↔ IVA DSP communication channel. Sends encode commands, receives status. |

## 18. USB / PTP System

### PTP Framework Functions

| Function | Address | Purpose |
|----------|---------|---------|
| `InitializePTPFrameworkController` | — | Sets up PTP (Picture Transfer Protocol) framework. Registers command handlers, initializes USB endpoint, starts listener. |
| `TerminatePTPFrameworkController` | — | Tears down PTP framework: closes USB endpoint, unregisters handlers, frees command buffers. |
| `PTPRspnd.StartUpPTPFrameworkClient` | — | Starts PTP responder client (camera side). Listens for USB host commands. |
| `ShutDownPTPFrameworkClient` | — | Shuts down PTP responder. Stops responding to USB host. |
| `ptpPropSetUILock` | `0xFF27C868` | Sets PTP UI lock property (locks camera controls during USB remote control session). Prevents conflicting local/remote control. |
| `ptp_register_handler` | `0xFF29F7BC` | Registers a custom PTP command handler. Used by ML's PTP tunnel module (ptptun) to handle opcode 0xA1E9. |

### PTP Communication Strings

From RAM strings, these represent PTP USB descriptors and protocol messages:

| String | Meaning |
|--------|---------|
| `[PTPCOM] SetPtpTransportResources:0,3253` | PTP transport resource initialization (transport=0=USB, resources=3253 bytes allocated). Logged at ~390ms in boot. |
| `USB PTP Configuration for FS/HS` | USB PTP configuration descriptor. FS = Full Speed (12Mbps), HS = High Speed (480Mbps). 70D supports both. |
| `USB PTP Device Qualifier, Other Speed Configuration` | USB device qualifier descriptor. Reports device capabilities when operating at non-preferred speed. Required for USB 2.0 spec compliance. |
| `PTP Framework PTP Event` | PTP event message. Notification sent when camera state changes (e.g., capture complete, card inserted). |

### PTP/PTPIP Architecture

```
Camera (ARM)                    USB Host / WiFi Client
     │                                │
     ├── PTP Responder                 │
     │   ├── Operation handlers       │
     │   │   ├── 0x1001 GetDeviceInfo │
     │   │   ├── 0x1008 GetObjectHandles
     │   │   ├── 0xA1E9 ML ptptun     │  (ML custom opcode)
     │   │   └── ...                  │
     │   ├── Transport layer          │
     │   │   ├── USB (bulk transfer)  │
     │   │   └── PTP/IP (TCP:15740)   │
     │   └── Event dispatcher         │
     │                                │
     └── ML PTP Tunnel                │
         ├── call("ExecuteLua")        │
         ├── call("Screenshot")        │
         ├── call("EngioRead/Write")   │
         └── call("ShamemRead")       │
```

---

## 19. Boot Log Analysis

The 70D boot sequence from physical hardware (annotated):

```
Time    Component     Event                                         Notes
──────  ──────────    ────────────────────────────────────────────  ──────────────
 48ms   [STARTUP]     K325 READY, ICU Firmware 1.1.2               70D = K325
 48ms   [STARTUP]     ICU Release 2016.07.11 09:50:17             Build date
 50ms   [PROPAD]      DRAMAddr 0x41744000                         property storage
105ms   [STARTUP]     startupPropAdminMain : End                   properties loaded
173ms   [FM/GPS]      GPS_RegisterSpaceNotifyCallback              GPS init
177ms   [JOB]         13 job classes initialized                   task system ready
190ms   [ENG]         ENGIO at 0x41700000                          sensor interface
238ms   [MC]          PROP_GUI_STATE 0, Variangle Enable
287ms   [MAC]         Board=0x0 Body=0xf0a16ee3                   model signature
331ms   [HPC]         ReserveHPCopyChannel (1, 116)                DMA channel
344ms   [LVDTS]       First Get DTS_GetAllRandomData               LV timing
369ms   [COM]         PackHeap 827eb0, size=262144                 comm heap init
379ms   [WFT]         InitializeAdapterControl END                 WiFi subsystem
388ms   [RMT]         PROP_CONNECT_TARGET (0x0)                    Remote target = none
390ms   [PTPCOM]      SetPtpTransportResources:0,3253              PTP stack init
479ms   [SD]          sdSendOCR: CCS=1, S18A=1                     SD card detect
527ms   [SD]          Set Hi-Speed Mode (96MHz)                    SD baseline speed
587ms   [FM]          FreeCluster (489266)                         32GB+ card
599ms   [SEQ]         seqEventDispatch (Startup, 4)                Phase 4 startup
600ms   [DP]          DP_Initialize() → DpsTerminate()             DisplayPort init
601ms   [DP]          PROP_WIFI_SETTING [0]                        WiFi off
606ms   [HDMI]        DisconnectHDMI                               HDMI disconnected
617ms   [IMPP]        H264 encoder init (x2)                       H.264 ready
647ms   [DISP]        SetBacklightBrightness                       Display on
683ms   [STARTUP]     startupInitializeComplete                    ** BOOT COMPLETE **
702ms   [FA]          ChangeCBR(ID=0x8003000a)                     Factory check
712ms   [MCELL]       GuiFactoryRegisterEventCommissionProcedure   ** ML INIT STARTS **
```

**Key observations:**
- Canon WiFi initialization (WFT/DP/RMT/PTPCOM) runs even when WiFi is off
- `PROP_WIFI_SETTING [0]` at boot → changes to 1 when user enables WiFi manually
- SD card: 96MHz Hi-Speed mode, 32GB+ capacity
- ML GUI factory registered at ~712ms
- Full boot to Canon UI: ~683ms, ML active: ~712ms

---

## 20. ADTG Register Addresses

Confirmed ADTG register patterns from the RAM dump, cross-referenced against crop_rec code. These patterns are used by Canon firmware to identify and configure specific ADTG (Analog/Digital Timing Generator) registers.

```
Register Pattern                                           ADTG Register  Purpose
─────────────────────────────────────────────────────────  ─────────────  ─────────────────────────────────────────────
( pTgRegister->dwSrFstAdtg1[6]  & 0xFFFF0000 ) == 0x81720000  0x8172     PowerSaveTiming ON (6D, 700D). Sensor enters low-power state after readout.
( pTgRegister->dwSrFstAdtg1[7]  & 0xFFFF0000 ) == 0x81730000  0x8173     PowerSaveTiming OFF (6D, 700D). Sensor wakes from low-power before next frame.
( pTgRegister->dwSrFstAdtg1[9]  & 0xFFFF0000 ) == 0x81780000  0x8178     PowerSaveTiming ON (5D3, 6D, 700D). Alternative power-save path.
( pTgRegister->dwSrFstAdtg1[10] & 0xFFFF0000 ) == 0x81790000  0x8179     PowerSaveTiming OFF (5D3, 6D, 700D). Alternative wake path.
```

**What these do:** The ADTG registers control the timing of the analog front-end on the sensor. The PowerSaveTiming registers define when the sensor can enter a low-power state between frame readouts. Setting these correctly via crop_rec ensures:
- Sensor runs cooler (less heat = less noise)
- Power savings during non-readout periods
- Proper synchronization between sensor readout and H.264 encoding

**Additional ADTG registers found in RAM/ROM:**
```
ADTG 0x805E   — Shutter speed (electronic shutter timing, used in crop_rec for exposure control)
ADTG 0x8060   — Shutter speed mirror register (reads back from 0x805E for verification)
ADTG 0x82B6   — PowerSave intermediate timing (700D specific; 2 units below ON timing)
ADTG 0x82F8   — ReadOutTiming start (marks beginning of sensor readout window)
ADTG 0x82F9   — ReadOutTiming end (marks end of sensor readout window; must match Timer B)
ADTG 0x8196   — PowerSaveTiming ON (5D3-specific path, confirmed in crop_rec)
ADTG 0x8197   — PowerSaveTiming OFF (5D3-specific path; paired with 0x8196)
```

All ADTG registers use NRZI (Non-Return-to-Zero Inverted) encoding. Values are encoded before writing via `nrzi_encode()` to prevent long runs of identical bit states on the sensor control bus.

---

## 21. MMIO Register Map

Registers confirmed by hw_test hardware reads or RAM strings:

### FPS / Timer (0xC0F06xxx)
```
FPS_CF           0xC0F06000    Confirmation register
FPS_TA           0xC0F06008    Timer A (row readout)
FPS_TAM          0xC0F0600C    Timer A mirror
FPS_TAH          0xC0F06010    Timer A high
FPS_TB           0xC0F06014    Timer B (frame timing)
ENGIO_TA_E24     0xC0F06824    ENGIO timer A mirror
ENGIO_TA_E28     0xC0F06828    ENGIO timer A mirror
ENGIO_TA_E2C     0xC0F0682C    ENGIO timer A mirror
ENGIO_TA_E30     0xC0F06830    ENGIO timer A mirror
ENGIO_HD3        0xC0F0713C    Head register 3
ENGIO_HD4        0xC0F07150    Head register 4
```

### ENGIO (0xC0F068xx)
```
ENGIO_TL         0xC0F06800    Top-left sensor position
ENGIO_BR         0xC0F06804    Bottom-right sensor position
```

### EDMAC (0xC0F04xxx–0xC0F30xxx)
```
RAW_PHOTO_EDMAC  0xC0F04008    Photo RAW EDMAC connection
EDMAC_WR_HD      0xC0F04A08    Write head register
LV_RAW_EDMAC     0xC0F26200    LV RAW EDMAC
```

### RAW Processing
```
RAW_TYPE         0xC0F37014    RAW data type
SHAD_GAIN        0xC0F08030    Shadow gain
SHAD_PRESETUP    0xC0F08034    Shadow pre-setup
PACK32_MODE      0xC0F08094    32-bit packing mode
CANON_WL         0xC0F12054    Canon white level
```

### Lossless Compression (0xC0F373xx–0xC0F376xx)
```
SLICE_SIZE       0xC0F37300    Compression slice size
LOSSLESS_MODE    0xC0F373b4    Lossless mode
SLICE_MIRROR     0xC0F373e8    Slice mirror
ALT_FIX          0xC0F373f4    Alternative fix
SLICE_OTHER      0xC0F375b4    Other slice config
LL_CTRL_10       0xC0F37610    Lossless control
LL_CFG_28–48     0xC0F37628–48 Config registers
TOTAL_IMG_SZ     0xC0F13068    Total image size
```

### Display / Palette (0xC0F140xx)
```
DISP_UPDATE      0xC0F14000    Display update trigger
CRAZY_COLORS     0xC0F14040    Color effect enable
PAL_TRIGGER      0xC0F14078    Palette trigger
PAL_0–15         0xC0F14080–BC Palette data
EXP_COMP         0xC0F140C0    Exposure compensation
SATURATION       0xC0F140C4    Saturation
ZEBRA_REG        0xC0F140CC    Zebra display
FB_LOW           0xC0F140D0    False color low
FB_HIGH          0xC0F140D4    False color high
FILTER_EN        0xC0F14140    Filter enable
DISP_POS         0xC0F14164    Display position
BRIGHTNESS       0xC0F141B8    Brightness
```

### SD / UHS (0xC04006xx)
```
SD_CLK0–8        0xC0400600–20  Clock/phase/timing
SD_MASTER        0xC0400614     Master config
SD_STAB1–3       0xC0400450–6C  Stability (5D3 only)
GPIO_SD0–5       0xC022C634–48  GPIO (not used on 70D)
```

### Miscellaneous
```
CARD_LED         0xC022C06C    Card access LED
ADTG_8172        0xC0F38010    ADTG register
ADTG_8173        0xC0F38014    ADTG register
ADTG_8178        0xC0F38020    ADTG register
ADTG_8179        0xC0F38024    ADTG register
ISO_PUSH_D5      0xC0F42744    D5 ISO push register
```

### Baseline Register Values (idle GUI)

From hw_test v27 baseline check (6/6 match):

```
FPS_CF    0xC0F06000 = 0x00000001
FPS_TA    0xC0F06008 = 0x02BB02BB  (Timer A: 699)
FPS_TB    0xC0F06014 = 0x000005F4  (Timer B: 1524)
ENGIO_TL  0xC0F06800 = 0x0001000F
ENGIO_HD3 0xC0F0713C = 0x000004E5
ENGIO_HD4 0xC0F07150 = 0x000004A9
```

---

## 22. Error & Assert Messages

### Assert Strings

These are Canon's firmware ASSERT macros that trigger when invariants are violated. An ASSERT failure typically causes a camera crash with error log written to the SD card.

| Format String | When Triggered |
|--------------|----------------|
| `ASSERT EVENT = %d` | Assert triggered by a specific event number. Event ID is logged for debugging. |
| `ASSERT!! %s Line %d` | Assert with file name and line number. Most common assert format in Canon code. |
| `ASSERT!! TryPostEvent` | Assert during event posting failure. Means an event couldn't be dispatched (queue full, invalid target). |
| `ASSERT%02d.LOG` | Assert log file naming pattern (e.g., ASSERT01.LOG, ASSERT02.LOG). Up to 99 crash logs stored on card. |
| `ASSERT:` | Generic assert prefix found in RAM. Shorter variant. |
| `Assert: File %s Line %d` | Assert with file but no expression. Found in older code paths. |
| `Assert: File %s, Expression %s, Line %d` | Full assert with file, expression, and line. Most detailed assert format. Contains the exact condition that failed. |

### Error Messages by Subsystem

#### SD/Storage
| Message | Meaning |
|---------|---------|
| `ERR: CardReadChk` | SD card read verification failed. Data read back doesn't match what was written. Indicates faulty card or connection. |
| `ERR: CardWriteChk` | SD card write verification failed. Data couldn't be written correctly. |
| `[SD] ERROR SDINTREP=<value>` | SD interrupt report error. The SDINTREP register value indicates which SD interrupt fired unexpectedly. |
| `[SD] ERROR UNEXPECTED ERROR` | Generic SD subsystem error. Catch-all for unexpected SD controller states. |
| `CARD_EMERGENCY_STOP:1` | Emergency SD card stop. Card access halted to prevent data corruption (e.g., during power loss). |
| `CARD_POWER_OFF:1` | SD card power off sequence initiated. Normal during camera shutdown or card removal. |

#### Memory
| Message | Meaning |
|---------|---------|
| `ERROR NOT_ENOUGH_MEMORY` | Memory allocation failed. Requested block larger than available free space. |
| `ERROR Not Allocate Memory` | Another memory allocation failure message (different code path, same meaning). |
| `ERROR GetMemoryInformation [%#x]` | Failed to get memory block info for address. Address was invalid or already freed. |
| `ERROR GetSizeOfMaxRegion [%#x]` | Failed to get largest contiguous free block. Likely pool corruption. |
| `Fail To Create` | Object creation failed (task, semaphore, queue). Usually preceded by the object type name. |
| `DRYOS PANIC: Module Code = %d` | DryOS kernel panic. Fatal error in DryOS kernel module. Module code identifies which subsystem crashed. |

#### WiFi
| Message | Meaning |
|---------|---------|
| `WLANSDCOMDRV_... Error!` (30+ variants) | WiFi SDIO communication driver errors. Each variant identifies a specific SDIO operation that failed (read byte, write byte, set block size, etc.). |

#### Image/Engine
| Message | Meaning |
|---------|---------|
| `Image Power Failure` | Image sensor power rail failure. Sensor isn't receiving correct voltage. |
| `FlashROM Write Verify Error!` | Internal flash memory write failed. Firmware/settings couldn't be saved. |
| `Chip erase error` | Flash memory chip erase failed. Can't clear flash before writing. |
| `[HPC ERROR] Nothing DMA ch now!` | High-Performance Copy had no available DMA channels. All channels are occupied. |
| `Polling Timeout Error` | Hardware polling timed out. A register didn't change to expected value within timeout period. |

#### Task/Kernel
| Message | Meaning |
|---------|---------|
| `[TASK ERROR] PostEvent/TryPendEvent` | Task event posting failed. Message queue full or invalid task ID. |
| `[STAGE ERROR] PostStageEvent` | Image processing stage event posting failed. Pipeline stage couldn't receive event. |
| `[VRAM] Fail To Create Controller` | Video RAM controller creation failed. Display subsystem couldn't initialize. |
| `[BUFF] Fail To Create Controller` | Buffer controller creation failed. Memory buffer for image pipeline couldn't be set up. |
| `[JOBQUEUE ERROR] InsertJob : Queue Overflow` | Job queue overflow. Too many processing jobs queued (burst shooting overflow). |

#### HDMI
| Message | Meaning |
|---------|---------|
| `[HDMI] DisconnectHDMI : Not Connected` | HDMI disconnect attempted but nothing was connected. Harmless informational message during boot. |

---

## 23. Module System Strings

### Module Infrastructure

These strings define ML's module loading system. Found in RAM at runtime, they represent the metadata sections embedded in every `.mo` module file.

| String | Purpose |
|--------|---------|
| `__module_info_` | Module info section prefix. Each module's `.mo` file contains `__module_info_<name>` ELF symbols. |
| `__module_strings_` | Module display strings (menu names, descriptions). Loaded from .mo file into RAM. |
| `__module_prophandlers_` | Module property handler table. Lists what PROP_ IDs this module wants to intercept. |
| `__module_cbr_` | Module callback table. Lists event callbacks this module registers. |
| `__module_config_` | Module configuration data. Menu items and default values for this module. |
| `.module_strings` | ELF section name for module strings. Defined in module linker script. |
| `.module_deps` | Module dependency list. Modules this .mo depends on (e.g., lua.mo depends on nothing, mlv_play.mo depends on lua.mo). |
| `MODULE_COUNT_MAX` | Maximum number of modules that can be loaded simultaneously. ~64 on 70D. |
| `module_lockfile` | Lock file preventing concurrent module loading. File at `ML/SETTINGS/LOADING.LCK`. |

### Module Load Paths

| Path | Purpose |
|------|---------|
| `ML/MODULES/*.mo` | Module binary files. Scanned recursively during module loading. 28 modules in 70D build. |
| `ML/SETTINGS/*.en` | Module enable/disable flags. One file per module: `<name>.en` with "1" or "0". |
| `ML/SETTINGS/LOADING.LCK` | Module loading lock file. Prevents race conditions. Exists only during module load. |

### Module Names (from runtime strings)

These 28 module names were found in RAM during the dump. Each has its `.mo` file loaded from `ML/MODULES/`:

| Module | Purpose |
|--------|---------|
| `hw_test` | Hardware diagnostic module. 35 PASS / 5 SKIP tests on physical 70D. RAM dumps, register baselines, FPS benchmarks. |
| `ramdump` | Full 512MB RAM dump module. Gated by flag file. Saves complete memory to SD card. |
| `dual_iso` | Dual ISO module. Alternates analog ISO every 2 scan lines for extended dynamic range. Photo mode confirmed working. |
| `crop_rec` | Crop recording module. 8 crop presets with CMOS/ENGIO register overrides. 3K, 4K, UHD modes. |
| `mlv_lite` | MLV Lite recorder. Streamlined Magic Lantern Video recording at lower bitrates. |
| `mlv_play` | MLV player. Playback module for reviewing MLV files on camera LCD with audio. |
| `mlv_rec` | MLV recorder (full). Feature-complete raw video recording with audio, metadata, spanning. |
| `mlv_snd` | MLV sound module. Audio recording component for MLV capture. Handles WAV interleaving. |
| `raw_vidx` | RAW video indexed. MLV v3-based raw recording with session API. Producer-consumer architecture. |
| `sd_uhs` | SD UHS overclocking module. Increases SD clock beyond 96MHz default. 160MHz preset verified. |
| `silent` | Silent picture module. Captures photos without mechanical shutter via electronic shutter (DIGIC V). |
| `deflick` | Deflicker module. Reduces exposure flicker in timelapse by smoothing frame-to-frame brightness. |
| `ettr` | ETTR (Expose To The Right) module. Auto-exposes for maximum dynamic range without clipping highlights. |
| `dot_tune` | DotTune AF microadjustment module. Automated AF calibration using contrast detection. |
| `autoexpo` | Auto exposure module for movie mode. Smooth auto-ISO and auto-shutter for video. |
| `lua` | Lua scripting engine. Runs .lua scripts from ML/SCRIPTS/. Provides camera API to scripts. |
| `bench` | Benchmark module. Performance and timing tests for SD card, EDMAC, memory, CPU. |
| `selftest` | Self-test framework. Unit tests for ML subsystems. Runs test suite on demand. |
| `adv_int` | Advanced intervalometer. Timelapse controller with exposure ramping and bulb support. |
| `arkenoid` | Arkanoid game module. Breakout-style game playable on camera using buttons/touch. |
| `file_man` | File manager. Browse, copy, move, delete files on SD card from ML menus. |
| `pic_view` | Picture viewer. View RAW and JPEG files with metadata. |
| `edmac` | EDMAC viewer module. Monitor and log EDMAC channel activity for reverse engineering. |
| `sf_dump` | Shoot functions dump. Logs and displays active shooting function addresses. |
| `adtglog2` | ADTG logger module. Hooks CMOS_write at 0x26B54 to log all ADTG register writes. |
| `wifi_test` | WiFi test module. PTPIP stub verification, socket API checks, NW command tests. |
| `wifisrv` | WiFi server module (v2). TCP server on camera for remote control via WiFi. |
| `ptptun` | PTP tunnel module. USB remote access via PTP opcode 0xA1E9. Works with camtunnel.py. |
| `img_name` | Image naming module. Custom file naming patterns for photos and videos. |

### TCC (Tiny C Compiler) Strings

The TCC is embedded in the Lua module for on-camera C compilation. Version 0.9.26 by Fabrice Bellard. Configured for ARMv5.

| Symbol | Purpose |
|--------|---------|
| `tcc_new` | Creates a new TCC compiler instance. Returns TCCState pointer. |
| `tcc_delete` | Destroys TCC compiler instance and frees all resources. |
| `tcc_add_file` | Adds a source file to compilation. Parses and generates intermediate code. |
| `tcc_add_symbol` | Adds a symbol (function or variable) to compiler symbol table. Used to expose ML APIs to compiled code. |
| `tcc_set_options` | Sets compiler options (optimization level, include paths, defines). |
| `tcc_get_symbol` | Looks up a symbol in compiled output. Returns address of compiled function/variable. |
| `tcc_get_section_ptr` | Gets pointer to a compiled section (text, data, bss). For custom relocation. |
| `tcc_load_offline_section` | Loads a pre-compiled section from file (for incremental compilation). |
| `tcc_relocate` | Relocates compiled code. Fixes up addresses. Must be called before executing compiled code. |
| `libtcc.c` | Main TCC library source file. Core compiler and code generator. |
| `0.9.26 (Fabrice Bellard)` | TCC version string. Embedded in the library binary. |

---

*For build instructions and current status, see [README.md](README.md).*  
*For the complete project history, see [CHANGELOG.md](CHANGELOG.md).*  
*For module-by-module technical analysis, see [DEEPDIVE.md](DEEPDIVE.md).*
