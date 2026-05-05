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

### Boot & Power (12)

```
EnableBootDisk       DisableBootDisk       EnableMainFirm
DisableMainFirm      EnableFirmware        DisableFirmware
PrepareEnableFirmware PrepareDisableFirmware Reboot
Format               EnablePowerSave       DisablePowerSave
```

### LiveView & Sensor (15)

```
FA_StartLiveView      FA_StopLiveView       PauseLiveView
ResumeLiveView        PowerOnLiveViewDevice PowerOffCaptureDevice
PowerOnCaptureDevice  StartupCaptureDevice  ShutdownCaptureDevice
EnableDebugGain       DisableDebugGain      EnableLvAccumGain
DisableLvAccumGain    EnableVideoOut        DisableVideoOut
```

### HDMI & Display (8)

```
EnableHDMI            DisableHDMI           EnableHDMIAudio
DisableHDMIAudio      TurnOnDisplay         TurnOffDisplay
SetBacklightBrightness SetDisplayType
```

### WiFi / Networking (25+)

```
WLANSDIODRV_InitializeSDIODriver    WLANSDIODRV_TerminateSDIODriver
WLANSDCOMDRV_Initialize             WLANSDCOMDRV_Terminate
WLANSDCOMDRV_EnableFunction         WLANSDCOMDRV_SelectCard
WLANSDCOMDRV_SetOCR                 WLANSDCOMDRV_GetRCA
WLANSDCOMDRV_ReadByte               WLANSDCOMDRV_WriteByte
WLANSDCOMDRV_SetBlockSize           WLANSDCOMDRV_SetBusWidth
InitializePTPFrameworkController    TerminatePTPFrameworkController
ptpip_sock_create                   ptpip_bind_param
ptpip_open_server                   ptpip_create_client
ptpip_listen_close                  ptpip_close_server
ptpip_set_keepalive                 ptpip_errno_handler
socket_close_caller                 socket_close_if_valid
```

### AF / Lens (12)

```
AfCtrl_Act_Ready              AfCtrl_Act_Suspend
AfCtrl_Act_Ignore             AfCtrl_Act_TvAfStart
AfCtrl_Act_CompleteAe_ForTvAf AfCtrl_Act_CompleteAfResult
AfCtrl_Act_TvAfStop           AfCtrl_Act_TvAfStop_Force
AfCtrl_Act_EmdDriveResult     AfCtrl_Act_StartLensDriveRemote
AfCtrl_Act_EndLensDriveRemote AfCtrl_Act_SetLensParameter
AfCtrl_Act_SetLensParameterRemote AfCtrl_Act_ContinuousAfStart
AfCtrl_Act_ContinuousAfStop   AfCtrl_Act_CompleteEmdDrive
```

### Remote Shot (3)

```
schedule_remote_shot   (runtime: 0x0047db24)
remote_shot            (runtime: 0x0047e00c)
remote_shot_flag       (runtime: 0x004cac90)
```

### Factory / Adjustment (191 FA_* functions)

See [Section 15](#15-fa_-factoryadjustment-functions) for the complete list.

### File I/O (15 FIO_* functions)

See [Section 16](#16-fio_-file-io-functions) for the complete list.

### EDMAC / DMA (8)

```
StartEDmac            StopEDmac             SetEDmac
AbortEDmac            ConnectReadEDmac      ConnectWriteEDmac
RegisterEDmacCompleteCBR  UnregisterEDmacCompleteCBR
RegisterEDmacAbortCBR     UnregisterEDmacAbortCBR
```

### Memory (12)

```
AllocateMemory          AllocateMemoryResource
GetMemory               GetMemoryInformation
GetSizeOfMaxRegion      CreateMemorySuite
CreateMemoryChunk       DeleteMemorySuite
AllocateLocalMemory     AllocateUncacheableMemory
AllocateHPMemory        AllocateDcfMemory
```

### Property (8)

```
prop_register_slave     prop_deliver          prop_request_change
prop_request_change_wait prop_add_handler     prop_getproperty
FA_SetProperty          FA_GetProperty
```

### Audio (14+)

```
StartASIFDMAADC     (0xFF1172E0)
StopASIFDMAADC      (0xFF11758C)
StartASIFDMADAC     (0xFF1176B4)
StopASIFDMADAC      (0xFF117934)
SetNextASIFADCBuffer (0xFF117DFC)
SetNextASIFDACBuffer (0xFF117FE4)
SetAudioVolumeIn    (0xFF11970C)
SetAudioVolumeOut   (0xFF13F728)
PowerMicAmp         (0xFF13FDE0)
PowerAudioOutput    (0xFF14169C)
ResetAudioIC        SendDataForAudioIC
EnableInternalMIC   EnableExternalMIC
EnableHDMIAudio     DisableHDMIAudio
```

### GPS (8)

```
GPS_Initialize      GPSList              GPSTime
GPSClearList        GetGPSTime           GPSListRecvCapability
GetGPSCaptureTimeList GPS_RegisterSpaceNotifyCallback
```

### Touchscreen (5+)

```
TCH_CheckTouchICVersion  TCH_SetWaitingTime
TCH_SetOpe2SysTime       TCH_SetMutualGainValue
TCH_SetMutualLocaliDacValue TCH_SetGainParamForSelfScan
FA_SetTouchIntervalTime  FA_SetTouchTestTime
```

### Debug / Misc (6)

```
dumpf      dumpfall     dumpfsep
olddumpf   olddumpfall  olddumpfsep
NotifyBox  NotifyBoxHide
```

### Enable / Disable Helper Functions (51)

Functions that toggle camera subsystems on/off via call():

```
EnableAF              DisableAF
EnableBootDisk        DisableBootDisk
EnableCardNoiseChk    DisableCardNoiseChk
EnableDebugGain       DisableDebugGain
EnableDebugMon
EnableFaceCatch       DisableFaceCatch
EnableFilter          DisableFilter
EnableFilterForHDMI   DisableFilterForHDMI
EnableFirmware        DisableFirmware
EnableFixedPo         DisableFixedPo
EnableFnoCorrect      DisableFnoCorrect
EnableHDMI            DisableHDMI
EnableHDMIAudio       DisableHDMIAudio
EnableLinearOffset    DisableLinearOffset
EnableLinerEShutCurve DisableLinerEShutCurve
EnableLtkids          DisableLtkids
EnableLvAccumGain     DisableLvAccumGain
EnableLvFnoCorrect    DisableLvFnoCorrect
EnableLvLinearOffset  DisableLvLinearOffset
EnableMainFirm        DisableMainFirm
EnablePowerSave       DisablePowerSave
EnableVideoOut        DisableVideoOut
EnableWBDetection     DisableWBDetection
EnableInternalMIC     EnableExternalMIC
```

---

## 4. ML Symbol Table (Runtime Addresses)

270+ ML functions confirmed in RAM at runtime. Key symbols organized by subsystem:

### Memory Allocation

| Symbol | Address | Purpose |
|--------|---------|---------|
| `__priv_malloc` | `0x00455cb4` | Low-level allocator |
| `__mem_malloc` | `0x004570b8` | Memory manager allocator |
| `shoot_malloc_suite` | `0x00457578` | Shooting memory suite allocation |
| `shoot_malloc_suite_contig` | — | Contiguous shooting memory |
| `shoot_malloc_frag_mem` | — | Fragmented shooting memory |
| `fio_malloc` | — | File I/O memory allocation |
| `alloc_fio_file` | — | FIO file buffer allocation |
| `alloc_dma_memory` | — | DMA-safe memory allocation |

### EDMAC / DMA

| Symbol | Address | Purpose |
|--------|---------|---------|
| `edmac_index_to_channel` | `0x004576f8` | Map EDMAC index to channel |
| `edmac_get_connection` | `0x00457954` | Get EDMAC connection info |
| `edmac_bytes_per_transfer` | `0x00457bcc` | Bytes per EDMAC transfer (DIGIC V: 16) |
| `edmac_memcpy` | `0x0048cad4` | EDMAC-based memory copy |
| `edmac_raw_slurp` | `0x0048caec` | RAW sensor data slurp via EDMAC |
| `edmac_get_address` | — | Get EDMAC source/destination address |
| `edmac_get_pointer` | — | Get EDMAC current pointer |
| `edmac_get_length` | — | Get EDMAC transfer length |
| `edmac_get_state` | — | Get EDMAC channel state |
| `edmac_get_dir` | — | Get EDMAC transfer direction |
| `edmac_memcpy_init` | — | Initialize EDMAC memcpy |
| `edmac_memcpy_start` | — | Start EDMAC memcpy |
| `edmac_memcpy_finish` | — | Complete EDMAC memcpy |

### ISO / Exposure

| Symbol | Address | Purpose |
|--------|---------|---------|
| `raw2iso` | `0x0046ddc8` | RAW value to ISO |
| `raw2index_iso` | — | RAW value to ISO index |
| `val2raw_iso` | `0x0046e1c8` | ISO value to RAW code |
| `split_iso` | `0x0046e630` | Split combined ISO into components |
| `iso_components_update` | `0x0046e684` | Update ISO component display |
| `bv_apply_iso` | `0x0046f618` | Apply ISO to brightness value |
| `get_max_analog_iso` | `0x0046fd84` | Max analog ISO value |
| `lens_set_rawiso` | `0x0046fa8c` | Set ISO via lens interface |
| `hdr_set_rawiso` | `0x0046fc80` | Set ISO for HDR mode |
| `get_frame_iso` | — | Get current frame ISO |
| `set_frame_iso` | — | Set current frame ISO |
| `is_native_iso` | — | Check if native ISO |
| `iso_toggle` | — | Toggle between ISO values |

### Shutter

| Symbol | Address | Purpose |
|--------|---------|---------|
| `raw2shutter_ms` | `0x0046ce98` | RAW shutter to milliseconds |
| `shutter_ms_to_raw` | `0x0046ceec` | Milliseconds to RAW shutter |
| `raw2shutterf` | — | RAW to shutter fraction |
| `shutterf_to_raw` | — | Shutter fraction to RAW |
| `get_max_shutter_timer` | `0x0047651c` | Maximum shutter timer value |
| `get_shutter_speed_us_from_timer` | — | Timer to microseconds |

### Menu / GUI

| Symbol | Address | Purpose |
|--------|---------|---------|
| `run_in_separate_task` | `0x0045d1a8` | Debug menu action dispatcher |
| `menu_remove` | `0x0045c6ac` | Remove menu entry |
| `ml_gui_main_task` | `0x004632d0` | ML GUI main task entry |
| `bmp_putpixel_fast` | — | Fast pixel write to BMP |
| `bmp_getpixel` | — | Read pixel from BMP |

### Battery / Power

| Symbol | Address | Purpose |
|--------|---------|---------|
| `GetBatteryLevel` | `0x00482334` | Battery level 0–100% |
| `battery_level_bars` | — | Battery bar display value |
| `powersave_prolong` | — | Prolong powersave timer |
| `auto_power_off_time` | — | Auto power-off timer value |

### Audio

| Symbol | Address | Purpose |
|--------|---------|---------|
| `sound_recording_enabled` | `0x0048fbc4` | Is sound recording active |
| `sound_recording_enabled_canon` | `0x0048fbac` | Canon-side sound recording flag |
| `get_audio_levels` | `0x0048fcac` | Get current audio levels |
| `audio_configure` | `0x0049025c` | Configure audio parameters |
| `sound_recording_mode` | — | Audio recording mode setting |

### Movie / FPS

| Symbol | Address | Purpose |
|--------|---------|---------|
| `mvr_config` | — | MVR configuration struct |
| `is_movie_mode` | `0x004707c4` | Check if in movie mode |
| `get_video_mode_name` | `0x004707f0` | Get video mode name string |

### Property System

| Symbol | Address | Purpose |
|--------|---------|---------|
| `prop_init` | `0x00470068` | Initialize property system |
| `prop_request_change` | `0x00470084` | Request property change |

### Task / Thread

| Symbol | Address | Purpose |
|--------|---------|---------|
| `get_current_task_name` | `0x00453550` | Get current task name |
| `ml_register_cbr` | `0x00487838` | Register ML callback |
| `ml_unregister_cbr` | — | Unregister ML callback |

### Remote Shot

| Symbol | Address | Purpose |
|--------|---------|---------|
| `schedule_remote_shot` | `0x0047db24` | Schedule remote capture |
| `remote_shot` | `0x0047e00c` | Execute remote capture |
| `remote_shot_flag` | `0x004cac90` | Remote shot state flag |

### Global Data

| Symbol | Address | Purpose |
|--------|---------|---------|
| `camera_model_id` | `0x004c8b6c` | Camera model ID (0x80000325) |
| `camera_model` | `0x004c8b70` | Camera model string pointer |
| `camera_serial` | `0x004c8bc0` | Camera serial number |
| `shutter_count` | `0x004c8b3c` | Total shutter actuations |
| `shutter_count_plus_lv_actuations` | — | Shutter + LV actuations |
| `mirror_down` | `0x004c8af8` | Mirror position flag |
| `firmware_version` | — | Firmware version string |

---

## 5. Property System

### PROP_ IDs (34 unique)

#### Connection / WiFi
```
PROP_CONNECT_TARGET (0x0)           PROP_CONNECT_TARGET_WFT
PROP_CONNECT_TARGET_INNER           PROP_PHYSICAL_CONNECT
PROP_INNER_PHYSICAL_CONNECT         PROP_NETWORK_SYSTEM
PROP_WIFI_SETTING                   PROP_ADAPTER_DEVICE_ACTIVE
PROP_WFT_BLUETOOTH
```

#### Display / LiveView
```
PROP_GUI_STATE                      PROP_VARIANGLE_GUICTRL
PROP_LV_OUTPUT_DEVICE               PROP_LV_DISPSIZE
PROP_HDMI_CHANGE_CODE               PROP_LCD_OFFON_BUTTON
```

#### Lens / Focus
```
PROP_LV_LENS (0x80050000)           PROP_LV_AF
PROP_LV_AFFRAME                     PROP_LV_LENS_DRIVE_RESULT
PROP_LV_LENS_DRIVE_REMOTE           PROP_LENS
PROP_TVAF_FRAMELIST                 PROP_AFMA (0x80010006)
```

#### Audio
```
PROP_HEADPHONE_VOLUME_VALUE         PROP_MOVIE_PLAY_VOLUME
```

#### Card / Storage
```
PROP_CARD_SELECT
```

#### Other
```
PROP_RTC (0xa0)                     PROP_ERROR_FOR_DISPLAY
PROP_ACTIVE_SWEEP_STATUS            PROP_DL_ACTION
PROP_MOVIE_PARAM                    PROP_CUSTOM_WB
PROP_TFT_COM
```

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

Canon's firmware includes a complete UPnP AV Media Server, discovered in RAM:

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

Services: `ContentDirectory:1` (SCPDURL: `/desc/cds.xml`), `ConnectionManager:1` (SCPDURL: `/desc/cms.xml`).  
Hardcoded IP: `192.168.1.20` (appears 25+ times across RAM).

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

The 70D's memory allocator tree, as reconstructed from RAM strings:

```
Top Level
├── AllocateMemoryResource (0xFF147F3C)    — Canon's top-level allocator
├── SRM_AllocateMemoryResourceFor1stJob (0xFF0E9F6C) — Shoot Resource Manager
│
Heap Layer
├── PackHeap/PackMem                       — Packed heap allocator
├── RingHeapMem/RingHeap                   — Ring buffer allocator
│
ML Layer
├── __priv_malloc / __priv_free             — Low-level ML allocator
├── __mem_malloc / __mem_free              — Memory manager allocator
│
├── fio_malloc / fio_free                  — File I/O buffer allocator
├── alloc_fio_file                         — FIO file allocation
│
├── shoot_malloc_suite / shoot_free_suite  — Shooting buffer allocation
├── shoot_malloc_suite_contig             — Contiguous shooting buffer
├── shoot_malloc_frag_mem                 — Fragmented shooting memory
│
├── srm_malloc_suite / srm_free_suite     — SRM buffer allocation
├── srm_malloc_cbr                        — SRM callback-based allocation
│
├── __priv_alloc_dma_memory               — DMA-safe memory allocation
├── __priv_free_dma_memory                — DMA memory deallocation
│
Debug
├── GetFreeMemForMalloc                   — Free malloc pool size
├── GetFreeMemForAllocateMemory           — Free allocate pool size
├── get_free_space_32k                    — 32K-aligned free space
```

**Runtime values (from hw_test v27):**
```
free_malloc  = ~313,776 bytes
free_alloc   = ~2,196,000 bytes
fio_max      = 4,096 KB
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

```
./KernelDry/KerTask.c
./KernelDry/KerSem.c
./KernelDry/KerSys.c
./KernelDry/KerFlag.c
./KernelDry/KerQueue.c
./KernelDry/KerRLock.c
```

### Memory (4)

```
./Memory/Memory.c
./PackMemory/PackHeap.c
./PackMemory/PackMem.c
./RingHeap/RingHeap.c
```

### Sensor / Device (3)

```
./SensorDrive/SensorDrive.c
./Device/TG/TGdriver.c
./Device/TG/LvTgDriver.c
```

### LiveView Common (7)

```
./LvCommon/LvGainController.c
./LvCommon/LvFaceYuvController.c
./LvCommon/LvEncodeController.c
./LvCommon/ImageCenter.c
./LvCommon/LvDefectController.c
./LvCommon/LvIRCorrectController.c
./LvCommon/LvJob.c
```

### EPP / Video Pipeline (8)

```
./Epp/SsDevelop/SsDevelopController.c
./Epp/SsDevelop/SsDevelopStage.c
./Epp/Vram/VramController.c
./Epp/Vram/VramStage.c
./Epp/Deliver/DeliverStage.c
./Epp/EppHist.c
./Path/Lv_x1_60fps/Lv_x1_60fps.c
./Path/SsLtDriver/SsLtDriver.c
```

### WiFi (2)

```
./WlanSdcom/WlanSdcomDrv.c
./WlanSdcom/WlanSDIODriver.c
```

### AEWB (3)

```
./AEWB/AEWBCommon/AEWBDataStocker.c
./AEWB/AEWBCommon/AEWBPropControl.c
./AEWB/AEWBRegister/AEWBRegister.c
```

### System / IPC (4)

```
./System/PComMem/PComMem.c
./System/postman/postman_m.c
./System/PostPostman/PostPostman.c
./HeadControl/LvHeadControl.c
```

### Debug / Misc (4)

```
./DbgMgr/DbgMgr.c
./DataStore/DataStore.c
./DataStore/DataStoreUtility.c
./Evf/EvfState.c
```

### Color / Lens (2)

```
./DS_Tree/DSLR01/K325/ICU/Src/Color/GetLensCorrectionData.c
./DS_Tree/DSLR01/K325/ICU/Src/Color/GetLensCorrectionData_Fmt2.c
```

### Base Canon Files (12)

```
EDmac.c        Siodriver.c    AppTask1.c     IvaTask.c
ColorTask.c    CreativeFilterTask.c  RecognitionTask.c  SequenceTask.c
StillTask.c    SystemTime.c   FaceAWB.c      Stub.c
```

### Eeko Image Processing Pipeline (47)

The Eeko subsystem is Canon's image processing engine. 47 source files identified:

```
EekoAddLowFreqLumaPathCore.c   EekoAddRawPath.c
EekoAddRawPathCore.c           EekoBltShare.c
EekoColorSubPathCore.c         EekoDister.c
EekoDistortPath.c              EekoDistortPathCore.c
EekoDistortResizeEeko.c        EekoEDmac.c
EekoEDmacCopyPath.c            EekoFilterResizeAddSub2PathCore.c
EekoFilteringCompositePathCore.c  EekoFilteringResizePathCore.c
EekoGradateCompositeEeko.c     EekoIvaDecPath.c
EekoIvaEncPath.c               EekoIvaEncPathCore.c
EekoJoynerLuckyPathCore.c      EekoJpCore.c
EekoLesserResizePathCore.c     EekoLotus.c
EekoLotusPathForMovie.c        EekoLotusPathForMovieCore.c
EekoLuckyPonyPathCore.c        EekoMaskUVSubPathCore.c
EekoMotionDetectEeko.c         EekoMulIndyPathCore.c
EekoNV12ResizePath.c           EekoNV12ResizePathCore.c
EekoOhyearKiPrm.c              EekoPonyFilterPathCore.c
EekoPostpro.c                  EekoRawToSsrawPathCore.c
EekoRawToSsrawToYuvPath.c      EekoResample.c
EekoResizeLittleYuvPath.c      EekoResizeLittleYuvPathCore.c
EekoRoughMonochromeYuvEeko.c   EekoRshdSndPath.c
EekoRshdSndPathForMulExp.c     EekoSaridon3PathCore.c
EekoSequenceRawToJpeg.c        EekoSequenceRawToSsrawToYuvPath.c
EekoShootJpegDisplayPath.c     EekoShootJpegDisplayPathCore.c
EekoShootJpegPath.c            EekoShootJpegPathCore.c
EekoShootJpegThmPath.c         EekoSndPas.c
EekoSsrawToYuvLvPath.c         EekoSsrawToYuvLvPathCore.c
EekoSsrawToYuvPathCore.c       EekoToyCamYuvEeko.c
EekoWaterColorYuvEeko.c        EekoYuvToVramPath.c
EekoYuvToVramPathCore.c        EekoYuvToVramPathForCFilter.c
EekoYuvToVramPathForCFilterCore.c  EekoYuvToVramPathForMulExp.c
EekoYuvToVramPathForMulExpCore.c
```

---

## 11. Task & Kernel System

### Canon Firmware Tasks

| Task Name | Purpose |
|-----------|---------|
| `init_task` | Canon initialization (0xFF0C54CC) |
| `gui_main_task` | GUI main loop (0xFF0D97AC) |
| `sounddev_task` | Audio device management (0xFF118F5C) |
| `create_init_task` | Boot-time task creation (0x00003168) |
| `livev_loprio_task` | LiveView low-priority processing |
| `livev_hiprio_task` | LiveView high-priority processing |
| `shoot_task` | Image capture |
| `shoot_task_mqueue` | Shoot message queue |
| `clock_task` | System clock/timing |
| `focus_misc_task` | Focus miscellaneous operations |
| `focus_task` | AF operations |
| `fps_task` | Frame rate control |
| `beep_task` | Audio beep |
| `audio_common_task` | Audio system |
| `debug_task` | Debug console |
| `console_task` | Console I/O |
| `menu_task` | Menu system |
| `menu_redraw_task` | Menu rendering |
| `notifybox_task` | Notification box |
| `playback_compare_images_task` | Image comparison |
| `expfuse_preview_update_task` | Exposure fusion preview |
| `tweak_task` | Camera tweaks |
| `tskmon_task` | Task monitor |
| `cls_task` | Clear screen |
| `guess_free_mem_task` | Free memory estimation |
| `movtweak_task` | Movie tweaks |

### ML Task Creation Functions

56 task creation wrappers mapped at runtime (0x004afxxx range):

```
task_create_console_task        task_create_module_load_task
task_create_menu_task           task_create_menu_redraw_task
task_create_debug_loop_task     task_create_tweak_task
task_create_focus_misc_task     task_create_focus_task
task_create_fps_task            task_create_seconds_clock_task
task_create_shoot_task          task_create_NotifyBox_task
task_create_vignetting_init     task_create_beep_task
task_create_audio_common_task   task_create_audio_menus_init
task_create_audio_meter_init    task_create_beep_init
task_create_bitrate_init        task_create_bmp_init
task_create_clearscreen_task    task_create_config_menu_init
task_create_dlg_init            task_create_edmac_memcpy_init
task_create_electronic_level_init task_create_exmem_init
task_create_fio_init            task_create_hist_init
task_create_lens_info_init      task_create_lens_init
task_create_livev_hipriority_task task_create_livev_lopriority_task
task_create_lv_img_init         task_create_lvinfo_init
task_create_module_init         task_create_movtweak_init
task_create_patch_simple_init   task_create_picstyle_features_init
task_create_powersave_init      task_create_prop_init
task_create_raw_init            task_create_state_init
task_create_vectorscope_feature_init task_create_vsync_init
task_create_zebra_init          task_create_debug_init_func
task_create_help_menu_init
```

### Kernel Services

```
CreateTask            CreateStateObject     CreateMessageQueue
CreateEventFlag       CreateBinarySemaphore CreateCountingSemaphore
CreateRecursiveLock   SetHPTimer            SetHPTimerAfterNow
SetTimerAfter         task_create           task_trampoline
task_dispatch_hook    current_task          task_max
get_task_info_by_id   gui_task_list         get_current_task_name
get_task_name_from_id get_current_task_id   run_in_separate_task
```

---

## 12. Lens & Focus System

### AF Remote Control Functions (Canon)

```
AfCtrl_Act_Ready
AfCtrl_Act_Suspend
AfCtrl_Act_Ignore
AfCtrl_Act_TvAfStart
AfCtrl_Act_CompleteAe_ForTvAf
AfCtrl_Act_CompleteAfResult
AfCtrl_Act_TvAfStop
AfCtrl_Act_TvAfStop_Force
AfCtrl_Act_EmdDriveResult
AfCtrl_Act_StartLensDriveRemote
AfCtrl_Act_EndLensDriveRemote
AfCtrl_Act_SetLensParameter
AfCtrl_Act_SetLensParameterRemote
AfCtrl_Act_ContinuousAfStart
AfCtrl_Act_ContinuousAfStop
AfCtrl_Act_CompleteEmdDrive
AfCtrl_ExecuteEvent
AfCtrl_PropertyCBR (PROP_LV_AF)
```

### AF Evaluation Modes

```
AF_EVAL_SINGLE       AF_EVAL_MULTI        AF_EVAL_LCR
AF_EVAL_DETAILED
```

### Lens Data Structures

```
LensCorrectionData    LensCorrectionData_Fmt2   LensData
LensDriveRemote       LensDriveResult           LensDriveStartCBR
LensFocus             LensFocus2                LensGyroResponseData
LensID                LensInfo                  LensParameter
LensParameterRemote   LensRequestStatus         LensSerialNumber
LensToFile            LensType                  LensWriteTime
Lenses                LensDataRequestStatus
```

### ML Lens Functions

```
lens_init             lens_info_init        lens_set_rawiso
lens_set_rawaperture  lens_set_rawshutter   lens_set_kelvin
lens_set_wbs_ba       lens_set_wbs_gm       lens_set_custom_wb_gains
lens_set_drivemode    lens_set_ae           lens_set_flash_ae
lens_display          lens_display_set_dirty
lens_focus            lens_focus_start      lens_focus_stop
lens_focus_enqueue_step lens_setup_af       lens_cleanup_af
lens_mlu_delay        lens_take_picture     lens_wait_readytotakepic
lens_format_iso       lens_format_aperture  lens_format_shutter
lens_format_dist      lens_format_shutter_reciprocal
```

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

```
SensorDrive.c         SDRV_PowerOnDevice    SDRV_StartupDevice
SDRV_StopDevice       SDRV_SleepDevice      SDRV_StanbyDevice
SDRV_ShutdownDevice   SDRV_Terminate
SDRV_SetDeviceParameter1st  SDRV_SetDeviceParameter2nd
SDRV_PrepareSetDeviceParameter  SDRV_Set1stSRParameter
SDRV_RequestQuickFrameRateChange
```

### Gain Control

```
LvGainController.c
LVGAIN_CTRL_SetDragneExt          LVGAIN_CTRL_SetIsoExpansion
LVGAIN_CTRL_SetPhotoStudioIsoComp LVGAIN_CTRL_SetFnoGain
LVGAIN_CTRL_SetTuningFlag         LVGAIN_CTRL_SetSensorScanMode
LVGAIN_CTRL_GetHShadingData       LVGAIN_CTRL_GetHShadingDataZOOM
LVGAIN_CTRL_UpdateMaxIsoAndDRange LVGAIN_CTRL_Delete
```

### Image Processing Functions

```
ImageCenter.c         LvFaceYuvController.c
LvEncodeController.c
LvHeadControl.c
RAW -> ssraw -> YUV pipeline (through Eeko subsystem)
```

---

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

191 FA_ functions identified. These form Canon's factory adjustment layer, accessible via call():

### LiveView (8)
```
FA_StartLiveView          FA_StopLiveView
FA_StartLvTestImage       FA_StopLvTestImage
FA_StartLvTestExposureImage FA_StopLvTestExposureImage
FA_PCLVStart              FA_PCLVEnd
```

### Property (6)
```
FA_SetProperty            FA_SetProperty32
FA_GetProperty            FA_GetPropertyAddress
FA_GetPropertyDataSize    FA_LoadProperty
```

### EEPROM / Persistent Storage (8)
```
FA_ReadEepromData         FA_WriteEepromData
FA_GetFileData            FA_GetFileSize
FA_DeleteFile             FA_SaveProperty
FA_SavePropertyList       FA_SaveWbFix
```

### Sensor Calibration (12)
```
FA_SetColor               FA_SetWbCoef          FA_GetWbCoef
FA_SetSsWbCoef            FA_GetSsWbCoef         FA_AdjustWhiteBalance
FA_DarkAdjAutoExecute     FA_DarkCheckAutoExecuteCH
FA_DarkCheckAutoExecutePP FA_DarkLevelCheckAutoExecute
FA_ReverseAdjust          FA_DecreaseGap / FA_IncreaseGap
```

### Defect / Pixel (14)
```
FA_DefectsTestImage       FA_DefectsMergeTestImage
FA_DetectDefTestImage     FA_ProjectionTestImage
FA_ProjectionTestImageEx  FA_ProjectionTestImageV
FA_SetMergeDefParameter   FA_SetDetectDefThreshold
FA_LvDefectMaxCountFull   FA_LvDefectMaxCountMagnify
FA_LvDefectMaxCountMovieCrop
FA_LvDetectDefectsFull    FA_LvDetectDefectsMagnify
FA_LvDetectDefectsMovieCrop
```

### Audio (4)
```
FA_AdjustMicBalance       FA_PrintMicLevel
FA_RecordSound            FA_RecordSoundEnd
```

### Calendar / Time (3)
```
FA_GetCalendar            FA_SetCalendar         FA_ResetCalendar
```

### Remote / Capture (4)
```
FA_RemoteRelease          FA_FinishRemoteRelease
FA_GetRemoteReleasePlusImage
FA_MovieStart / FA_MovieEnd
```

### SD / WiFi / Hardware Check (4)
```
FA_CheckSD                FA_ChkAssembly
FA_ChkWlan                FA_MacSelfCheck
```

### Touchscreen (2)
```
FA_SetTouchIntervalTime   FA_SetTouchTestTime
```

### Display (8)
```
FA_DISP_SetBrightness     FA_DISP_COM_SetCamera
FA_DISP_Start100White     FA_DISP_Start50Gray
FA_DISP_StartColor        FA_DISP_StartMix
FA_DISP_End100White       FA_DISP_End50Gray
FA_DISP_EndColor
```

---

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

### Encoder

```
H264E InitializeH264EncodeFor1080pDZoom
H264E InitializeH264EncodeFor1080p25fpsDZoom
(and 55+ Eeko image processing paths — see Section 10)
```

### MVR Configuration

```
mvr_config (struct, 0x1D8 bytes)
mvrSetBitRate             mvrSetRecLimit
mvrAppendCheckSetRecLimit mvrSetQscale
mvrSetQscaleYC            mvrSetDeblockingFilter
mvrSetLimitQScale         mvrSetDefQScale
mvrSetTimeConst           mvrSetFullHDOptSize
mvrSetHDOptSize           mvrSetVGAOptSize
mvrFixQScale              mvrSetDefDBFilter
mvrSetPrintMovieLog
```

### IVA (Video Processing)

```
IVA_BsBuf                IVA_ELDMain           IVA_Mailbox
EekoIvaDecPath           EekoIvaEncPath        EekoIvaEncPathCore
```

---

## 18. USB / PTP System

```
InitializePTPFrameworkController    TerminatePTPFrameworkController
PTPRspnd.StartUpPTPFrameworkClient  ShutDownPTPFrameworkClient
SetPtpTransportResources:0,3253
ptpPropSetUILock (0xFF27C868)
ptp_register_handler (0xFF29F7BC)
```

PTP communication strings found:
```
[PTPCOM] SetPtpTransportResources:0,3253
USB PTP Configuration for FS/HS
USB PTP Device Qualifier, Other Speed Configuration
PTP Framework PTP Event
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

Confirmed ADTG register patterns from the RAM dump, cross-referenced against crop_rec code:

```
( pTgRegister->dwSrFstAdtg1[6]  & 0xFFFF0000 ) == 0x81720000
( pTgRegister->dwSrFstAdtg1[7]  & 0xFFFF0000 ) == 0x81730000
( pTgRegister->dwSrFstAdtg1[9]  & 0xFFFF0000 ) == 0x81780000
( pTgRegister->dwSrFstAdtg1[10] & 0xFFFF0000 ) == 0x81790000
```

These match the ADTG registers used in crop_rec code for sensor timing configuration.

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
```
ASSERT EVENT = %d                          ASSERT! TryPostEvent
ASSERT!! %s Line %d                        ASSERT%02d.LOG
ASSERT:                                    ASSERT: %s
Assert: File %s Line %d                     Assert: File %s, Expression %s, Line %d
```

### Error Messages by Subsystem

**SD/Storage:**
```
ERR: CardReadChk                           ERR: CardWriteChk
[SD] ERROR SDINTREP                        [SD] ERROR UNEXPECTED ERROR
CARD_EMERGENCY_STOP:1                      CARD_POWER_OFF:1
```

**Memory:**
```
ERROR NOT_ENOUGH_MEMORY                    ERROR Not Allocate Memory
ERROR GetMemoryInformation [%#x]           ERROR GetSizeOfMaxRegion [%#x]
Fail To Create                             DRYOS PANIC: Module Code = %d
```

**WiFi:**
```
WLANSDCOMDRV_... Error! (30+ distinct strings)
```

**Image/Engine:**
```
Image Power Failure                        FlashROM Write Verify Error!
Chip erase error                           [HPC ERROR] Nothing DMA ch now!
Polling Timeout Error
```

**Task/Kernel:**
```
[TASK ERROR] PostEvent/TryPendEvent        [STAGE ERROR] PostStageEvent
[VRAM] Fail To Create Controller           [BUFF] Fail To Create Controller
[JOBQUEUE ERROR] InsertJob : Queue Overflow
```

**HDMI:**
```
[HDMI] DisconnectHDMI : Not Connected
```

---

## 23. Module System Strings

### Module Infrastructure
```
__module_info_        __module_strings_       __module_prophandlers_
__module_cbr_         __module_config_        .module_strings
.module_deps          MODULE_COUNT_MAX        module_lockfile
```

### Module Load Paths
```
ML/MODULES/*.mo       ML/SETTINGS/*.en
ML/SETTINGS/LOADING.LCK
```

### Module Names (from runtime strings)
```
hw_test               ramdump                 dual_iso
crop_rec              mlv_lite                mlv_play
mlv_rec               mlv_snd                 raw_vidx
sd_uhs                silent                  deflick
ettr                  dot_tune                autoexpo
lua                   bench                   selftest
adv_int               arkanoid                file_man
pic_view              edmac                   sf_dump
adtglog2              wifi_test               wifisrv
ptptun                img_name
```

### TCC (Tiny C Compiler) Strings
```
tcc_new               tcc_delete              tcc_add_file
tcc_add_symbol        tcc_set_options         tcc_get_symbol
tcc_get_section_ptr   tcc_load_offline_section tcc_relocate
libtcc.c              0.9.26 (Fabrice Bellard)
```

---

*For build instructions and current status, see [README.md](README.md).*  
*For the complete project history, see [CHANGELOG.md](CHANGELOG.md).*  
*For module-by-module technical analysis, see [DEEPDIVE.md](DEEPDIVE.md).*
