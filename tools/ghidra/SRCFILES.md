# Canon 70D Firmware Source Tree (reconstructed from ROM1 strings)

**DRYOS version 2.3, release #0051**
**ICU Firmware 1.1.2, K325 platform**

## Core Kernel (DryOS)
| Path | Purpose |
|------|---------|
| `./KernelDry/KerTask.c` | Task management (create, dispatch, priority, context switch) |
| `./KernelDry/KerSem.c` | Semaphore operations (binary, counting) |
| `./KernelDry/KerSys.c` | System services (boot, panic) |
| `./KernelDry/KerFlag.c` | Event flag groups |
| `./KernelDry/KerQueue.c` | Message queues |
| `./KernelDry/KerRLock.c` | Recursive locks |
| `./Module/Dryos/kernel/pvmalloc.c` | Virtual memory allocator |
| `./Memory/Memory.c` | Memory pool manager |
| `./PackMemory/PackHeap.c` | Packed heap allocator |
| `./PackMemory/PackMem.c` | Packed memory utilities |
| `./RingHeap/RingHeap.c` | Ring buffer allocator |
| `./System/Memory/Memory.c` | System memory utilities |

## Eventproc (callable by name)
| Path | Purpose |
|------|---------|
| `./BootInfo/BootInfo.c` | Boot-related eventprocs (EnableBootDisk, etc.) |
| `./PtpMgr/PtpApp/ptpUtility.c` | PTP utility eventprocs |
| `./EvntShel/Console.c` | Console/debug eventprocs (dumpf, etc.) |
| `./FACTORY/zFctUtilFuncAndDatabase.c` | Factory test eventprocs |

## Sensor & Image Pipeline
| Path | Purpose |
|------|---------|
| `./SensorDrive/SensorDrive.c` | Image sensor power/init |
| `./Device/TG/TGdriver.c` | Timing Generator driver |
| `./Device/TG/LvTgDriver.c` | LV Timing Generator driver |
| `./EekoApp/Color/ClrAlgorWrap.c` | Color algorithm wrapper |
| `./EekoApp/HDR/EekoHdrDrv.c` | HDR processing |
| `./PathDrv/EekoAddRawPathCore.c` | RAW frame addition |
| `./PathDrv/EekoDistortPathCore.c` | Distortion correction |
| `./PathDrv/EekoFilteringResizePathCore.c` | Filter + resize |

## Audio
| Path | Purpose |
|------|---------|
| `./Audio/AudioIC.c` | Audio codec IC driver |
| `./ASIF/ASIF.c` | Audio Serial Interface DMA |
| `./ASIF/AsifState.c` | ASIF state machine |
| `./AudioCtrl/AudioCtrl.c` | Audio control |
| `./SoundDevice/AudioLevel.c` | Audio level metering |
| `./SoundDevice/SoundDevice_CODEC.c` | Codec-specific sound device |
| `./Voice/Sound.c` | System sounds (beeps) |

## WiFi / Networking
| Path | Purpose |
|------|---------|
| `./WlanMgr/WlanMgr.c` | WiFi manager |
| `./WlanSdcom/WlanSDIODriver.c` | SDIO WiFi driver |
| `./WlanSdcom/WlanSdcomDrv.c` | SDIO communication driver |
| `./ifwrapper/sdio/diana/esdio_dcp.c` | SDIO wrapper (chip: Diana) |
| `./nell/nell_ie.c` | Nell IO engine |
| `./nell/nell_ioctl.c` | Nell IO control |
| `./nell/nell_nlcmd.c` | Nell network command |
| `./nell/nell_task.c` | Nell task handler |
| `./nell/nell_uap.c` | Nell upper-layer handler |
| `./nell/nl/nl_cmd.c` | Network layer commands |
| `./nell/nl/nl_network.c` | Network layer |
| `./nell/nl/nl_security.c` | Network security (802.11i) |
| `./nus/nus_task.c` | Network utility service |
| `./nus/nusd.c` | Network utility daemon |
| `./platform/diana/dry_nell_task.c` | Diana platform Nell task |
| `./platform/diana/sd_wif11n.c` | SD WiFi 802.11n driver |
| `./NWMgr/NetworkSetting.c` | Network settings |
| `./NWCommu/NWCommuTransOrder.c` | Network communication transport |

## DLNA / UPnP
| Path | Purpose |
|------|---------|
| `./DLNAApp/DlnaMgr.c` | DLNA manager |
| `./DLNAApp/DlnaUtility.c` | DLNA utilities |
| `./DLNAApp/DmsCtrl.c` | Digital Media Server controller |
| `./UPeNdCore/src/httpd/cUPeNdHttpClient.c` | HTTP client (web UI) |

## WiFi Security (WPS/WPA2)
| Path | Purpose |
|------|---------|
| `./api/wpsa/wpsa.c` | WPS application |
| `./api/wpsa/wpsa_controller.c` | WPS controller |
| `./api/wpsa/wpsa_sm.c` | WPS state machine |
| `./api/wpsa/wpsa_80211i_if.c` | 802.11i interface |
| `./api/rsa/impl/rsa.c` | RSA crypto |
| `./api/random/impl/random.c` | Random number gen |
| `./api_dh/impl/dh.c` | Diffie-Hellman key exchange |
| `./api_x509/impl/x509s.c` | X.509 certificate handling |
| `./sha2/impl/sha2.c` | SHA-256 hash |
| `./aes/impl/aes_key_init.c` | AES encryption |

## PTP / USB
| Path | Purpose |
|------|---------|
| `./PtpMgr/ComFrame/PTPRspnd/PtpFrame/DsPtpFrame.c` | PTP data set frame |
| `./PtpMgr/ComFrame/PTPRspnd/PtpFrame/FramCtrl.c` | PTP frame control |
| `./PtpMgr/ComFrame/PTPRspnd/PtpFrame/PropMgr.c` | PTP property manager |
| `./PtpMgr/ComFrame/PTPRspnd/TrnsCtrl.c` | Transport control |
| `./PtpMgr/ComFrame/PTPRspnd/SdioPTPTrnsp2/` | SDIO PTP transport |
| `./PtpMgr/PtpApp/PtpProperty.c` | PTP properties |
| `./PtpMgr/PtpApp/MtpOperation.c` | MTP operations |
| `./FactoryConnect/PTPtoFAPI/PTPtoFAPI.c` | PTP to Factory API bridge |

## GPS
| Path | Purpose |
|------|---------|
| `./Gps/Gps.c` | GPS driver |

## Touch Screen
| Path | Purpose |
|------|---------|
| `./Touch/TouchState.c` | Touch screen state machine |

## LV / EVF (LiveView / Electronic Viewfinder)
| Path | Purpose |
|------|---------|
| `./Evf/Evf.c` | EVF main |
| `./Evf/EvfState.c` | EVF state machine |
| `./LvCommon/LvGainController.c` | LV gain control |
| `./LvCommon/LvFaceYuvController.c` | LV face detection |
| `./LvCommon/LvEncodeController.c` | LV encode controller |
| `./LvCommon/ImageCenter.c` | LV image center |
| `./LvCommon/LvDefectController.c` | LV defect pixel mapping |
| `./LvCommon/LvIRCorrectController.c` | LV IR correction |
| `./LvCommon/LvJob.c` | LV job scheduler |
| `./HeadControl/LvHeadControl.c` | LV sensor head control |
| `./Path/Lv_x1/Lv_x1.c` | LV 1x path |
| `./Path/Lv_x5/Lv_x5.c` | LV 5x path |
| `./Path/Lv_x10/Lv_x10.c` | LV 10x path |
| `./Path/Lv_x1_60fps/Lv_x1_60fps.c` | LV 1x 60fps |
| `./Path/Lv_x1_Ta10/Lv_x1_Ta10.c` | LV TA10 path |
| `./Path/Lv_x1_CreativeFilter/Lv_x1_CreativeFilter.c` | LV creative filter |

## Display / GUI
| Path | Purpose |
|------|---------|
| `./GuiDataStorage/GuiPropertyDataStorage.c` | GUI property storage |
| `./GuiDataStorage/GuiUniqueDataStorage.c` | GUI unique data |
| `./Widget/Button/Button.c` | Button widget |
| `./Widget/TextMenu/TextMenu.c` | Text menu widget |
| `./Widget/PopupMenu/PopupMenu.c` | Popup menu widget |
| `./Widget/Slider/Slider.c` | Slider widget |
| `./Widget/Progress/Progress.c` | Progress bar widget |
| `./Widget/Histogram/Histogram.c` | Histogram widget |

## Movie Recording
| Path | Purpose |
|------|---------|
| `./MovieRecorder/MovieRecorder.c` | Movie recorder main |
| `./MovieRecorder/MovRecState.c` | Movie record state |
| `./MovieRecorder/MovWriter.c` | Movie writer |
| `./MovieRecorder/EncDrvWrapper.c` | Encoder driver wrapper |
| `./MovieRecorder/TimeCodeMaster.c` | Timecode master |
| `./MoviePlayer/MoviePlayer.c` | Movie player |

## Lens / Focus
| Path | Purpose |
|------|---------|
| `./Gmt/Gmt.c` | General mount (lens interface) |
| `./Gmt/GmtState.c` | Lens mount state |
| `./Gmt/GmtAfController.c` | AF control |
| `./Gmt/GmtContinuousAf.c` | Continuous AF |
| `./Gmt/GmtMovieState.c` | Movie mode lens state |
| `./Color/GetLensCorrectionData.c` | Lens correction data (K325 platform) |
| `./Color/GetLensCorrectionData_Fmt2.c` | Lens correction data format 2 |

## AEWB (Auto Exposure / White Balance)
| Path | Purpose |
|------|---------|
| `./AEWB/AEWB.c` | AEWB main |
| `./AEWB/AEWBState.c` | AEWB state |
| `./AEWB/AECalc/AECalc.c` | AE calculation |
| `./AEWB/WBCalc/WBCalc.c` | WB calculation |
| `./AEWB/AEWBCommon/AEWBDataStocker.c` | AEWB data accumulation |
| `./AEWB/AEWBCommon/AEWBPropControl.c` | AEWB property control |
| `./AEWB/AEWBRegister/AEWBRegister.c` | AEWB register interface |

## Factory / Adjustment
| Path | Purpose |
|------|---------|
| `./FACTORY/zFctData.c` | Factory data |
| `./FACTORY/zFctFactoryMenu.c` | Factory menu |
| `./FACTORY/zFctResult.c` | Factory test results |
| `./FACTORY/zFctBackCoverTest.c` | Back cover test |
| `./FACTORY/zFctBackSWTest.c` | Back SW test |
| `./FACTORY/zFctDispDialCK.c` | Display dial check |
| `./FACTORY/zFctErr.c` | Factory error handling |

## Power Management
| Path | Purpose |
|------|---------|
| `./PowerMgr/PowerSaver.c` | Power save controller |
| `./CtrlMan/CtrlMan.c` | Control manager |

## SD Card
| Path | Purpose |
|------|---------|
| `./SdioDrv/SdioDrv.c` | SDIO driver |
| `./SdioTsk/SdioTsk.c` | SDIO task |
| `./Card/STGInitialSetting.c` | Card stage initial settings |

## Debug
| Path | Purpose |
|------|---------|
| `./DbgMgr/DbgMgr.c` | Debug manager |
| `./EvntShel/Console.c` | Debug console |
| `./EvntShel/DCPLog.c` | DCP logging |

## Serial Flash / RTC / EEPROM
| Path | Purpose |
|------|---------|
| `./SerialFlash/serialflash.c` | Serial flash driver |
| `./RTC/RTC2062.c` | RTC driver (RTC2062) |
| `./EEPROM/eeprom.c` | EEPROM driver |
| `./SerialIO/SerialIO.c` | Serial I/O |
