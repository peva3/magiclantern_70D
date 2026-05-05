DRYOS API Reference — Canon 70D (FW 1.1.2, DIGIC V)
============================================================

ARCHITECTURE: DIGIC V DryOS does NOT use ARM SWI/SVC for syscalls.
Instead, kernel services are compiled as RAM-loaded modules dispatched
via BL to RAM addresses (0x000xxxxx range). ROM1-resident NSTUB wrappers
load function pointers from RAM module space and call through them.
This is a C-level function table dispatch — no SWI vector table exists.

Kernel functions loaded at 0x000xxxxx offsets (DryOS module space)
ROM1-resident wrappers use absolute 0xFFxxxxxx addresses


## Task Management

  task_create                              0x000098CC  [low-RAM]
  task_trampoline                          0x0000EF60  [low-RAM]
  task_dispatch_hook                       0x0007AAD4  [RAM-module]
  current_task                             0x0007AAC0  [RAM-module]
  task_max                                 0x0007BEF4  [RAM-module]
  get_task_info_by_id                      0x00051358  [RAM-module]
  current_interrupt                        0x0000064C  [low-RAM]
  pre_isr_hook                             0x0007A9B8  [RAM-module]
  post_isr_hook                            0x0007A9BC  [RAM-module]


## Memory Allocation

  AllocateMemory                           0x0000A75C  [low-RAM]
  FreeMemory                               0x0000AAF8  [low-RAM]
  GetMemoryInformation                     0x0000A4FC  [low-RAM]
  GetSizeOfMaxRegion                       0x0000A4A8  [low-RAM]
  CreateMemorySuite                        0x0000C420  [low-RAM]
  DeleteMemorySuite                        0x0000C070  [low-RAM]
  CreateMemoryChunk                        0x0000B45C  [low-RAM]
  AddMemoryChunk                           0x0000BC70  [low-RAM]
  GetFirstChunkFromSuite                   0x0000BD8C  [low-RAM]
  GetNextMemoryChunk                       0x0000CA90  [low-RAM]
  GetMemoryAddressOfMemoryChunk            0x0000BA08  [low-RAM]
  alloc_dma_memory                         0x0003DF6C  [DryOS-kernel]
  free_dma_memory                          0x0003DFA0  [DryOS-kernel]


## Semaphore/IPC

  create_named_semaphore                   0x000091DC  [low-RAM]
  take_semaphore                           0x0000933C  [low-RAM]
  give_semaphore                           0x00009428  [low-RAM]
  take_semaphore_now                       0x000092E4  [low-RAM]


## Message Queues

  msg_queue_create                         0x0003A4E8  [DryOS-kernel]
  msg_queue_receive                        0x0003A5F8  [DryOS-kernel]
  msg_queue_post                           0x0003A7E4  [DryOS-kernel]
  msg_queue_count                          0x0003A824  [DryOS-kernel]


## Recursive Locks

  CreateRecursiveLock                      0x0000D4D0  [low-RAM]
  AcquireRecursiveLock                     0x0003A930  [DryOS-kernel]
  ReleaseRecursiveLock                     0x0003AA44  [DryOS-kernel]


## Timers

  SetTimerAfter                            0x0000E8AC  [low-RAM]
  CancelTimer                              0x0000EAAC  [low-RAM]
  SetHPTimerAfterNow                       0x00007F94  [low-RAM]
  SetHPTimerNextTick                       0x00008094  [low-RAM]


## Events

  TryPostEvent                             0x0003DD24  [DryOS-kernel]
  TryPostStageEvent                        0x0003D644  [DryOS-kernel]
  TryPostEvent_end                         0x0003DD80  [DryOS-kernel]
  TryPostStageEvent_end                    0x0003D6A0  [DryOS-kernel]


## EDMAC/DMA

  StartEDmac                               0x0003817C  [DryOS-kernel]
  StopEDmac                                (not in stubs.S)
  SetEDmac                                 0x00037DD0  [DryOS-kernel]
  AbortEDmac                               0x000384A0  [DryOS-kernel]
  ConnectReadEDmac                         0x00037FF4  [DryOS-kernel]
  ConnectWriteEDmac                        0x00037F30  [DryOS-kernel]
  RegisterEDmacCompleteCBR                 0x00038540  [DryOS-kernel]
  UnregisterEDmacCompleteCBR               0x0003857C  [DryOS-kernel]
  RegisterEDmacAbortCBR                    0x000385D4  [DryOS-kernel]
  UnregisterEDmacAbortCBR                  0x00038610  [DryOS-kernel]
  RegisterEDmacPopCBR                      0x00038668  [DryOS-kernel]
  UnregisterEDmacPopCBR                    0x000386A4  [DryOS-kernel]
  CreateResLockEntry                       0xFF2C049C  [ROM1:0x72C049C]
  LockEngineResources                      0xFF2C095C  [ROM1:0x72C095C]
  UnLockEngineResources                    0xFF2C0B00  [ROM1:0x72C0B00]
  dma_memcpy                               0x0000D834  [low-RAM]


## File I/O

  FIO_OpenFile                             0xFF34AEEC  [ROM1:0x734AEEC]
  FIO_CloseFile                            0xFF34B34C  [ROM1:0x734B34C]
  FIO_ReadFile                             0xFF34B0FC  [ROM1:0x734B0FC]
  FIO_WriteFile                            0xFF34B29C  [ROM1:0x734B29C]
  FIO_CreateFile                           0xFF34AFA8  [ROM1:0x734AFA8]
  FIO_CreateDirectory                      0xFF34BBA8  [ROM1:0x734BBA8]
  FIO_RemoveFile                           0xFF34B054  [ROM1:0x734B054]
  FIO_RenameFile                           0xFF34B9E0  [ROM1:0x734B9E0]
  FIO_SeekSkipFile                         0xFF34B1AC  [ROM1:0x734B1AC]
  FIO_GetFileSize                          0xFF34B4C8  [ROM1:0x734B4C8]
  FIO_FindFirstEx                          0xFF34C170  [ROM1:0x734C170]
  FIO_FindNextEx                           0xFF34C264  [ROM1:0x734C264]
  FIO_FindClose                            0xFF34C344  [ROM1:0x734C344]


## Properties

  prop_register_slave                      0xFF12A924  [ROM1:0x712A924]
  prop_request_change                      0xFF12AB14  [ROM1:0x712AB14]
  prop_deliver                             0xFF12B090  [ROM1:0x712B090]
  prop_cleanup                             0xFF12B15C  [ROM1:0x712B15C]
  PROPAD_GetPropertyData                   0xFF12CB14  [ROM1:0x712CB14]


## Debug

  DryosDebugMsg                            0x00006904  [low-RAM]
  dm_set_store_level                       0x00006C70  [low-RAM]
  dm_names                                 0x0007B868  [RAM-module]
  vsnprintf                                0x0003BFC4  [DryOS-kernel]
  bzero32                                  0x00071618  [RAM-module]


## GUI/Display

  gui_main_task                            0xFF0D97AC  [ROM1:0x70D97AC]
  GUI_Control                              0xFF0D9B7C  [ROM1:0x70D9B7C]
  GUI_ChangeMode                           0xFF0D9D38  [ROM1:0x70D9D38]
  gui_init_end                             0xFF0DA220  [ROM1:0x70DA220]
  SetGUIRequestMode                        0xFF196950  [ROM1:0x7196950]
  MirrorDisplay                            0xFF504650  [ROM1:0x7504650]
  NormalDisplay                            0xFF5046B0  [ROM1:0x75046B0]
  ReverseDisplay                           0xFF504680  [ROM1:0x7504680]


## Lens/AF

  EngDrvOut                                0xFF2BC3AC  [ROM1:0x72BC3AC]
  engio_write                              0xFF2BC6C4  [ROM1:0x72BC6C4]
  shamem_read                              0xFF2BC448  [ROM1:0x72BC448]
  call                                     0xFF14439C  [ROM1:0x714439C]
  GetCFnData                               0xFF6C8054  [ROM1:0x76C8054]
  SetCFnData                               0xFF6C813C  [ROM1:0x76C813C]


## Audio/ASIF

  StartASIFDMAADC                          0xFF1172E0  [ROM1:0x71172E0]
  StopASIFDMAADC                           0xFF11758C  [ROM1:0x711758C]
  StartASIFDMADAC                          0xFF1176B4  [ROM1:0x71176B4]
  StopASIFDMADAC                           0xFF117934  [ROM1:0x7117934]
  SetNextASIFADCBuffer                     0xFF117DFC  [ROM1:0x7117DFC]
  SetNextASIFDACBuffer                     0xFF117FE4  [ROM1:0x7117FE4]
  SetAudioVolumeIn                         0xFF11970C  [ROM1:0x711970C]
  SetAudioVolumeOut                        0xFF13F728  [ROM1:0x713F728]
  SoundDevActiveIn                         0xFF11936C  [ROM1:0x711936C]
  SoundDevShutDownIn                       0xFF1195C4  [ROM1:0x71195C4]
  PowerMicAmp                              0xFF13FDE0  [ROM1:0x713FDE0]
  PowerAudioOutput                         0xFF14169C  [ROM1:0x714169C]
  SetSamplingRate                          0xFF13FE14  [ROM1:0x713FE14]
  audio_ic_read                            0xFF13F208  [ROM1:0x713F208]
  audio_ic_write                           (not in stubs.S)


## MVR/H.264

  mvrFixQScale                             0xFF2BB65C  [ROM1:0x72BB65C]
  mvrSetDefQScale                          0xFF2BB154  [ROM1:0x72BB154]
  mvrSetFullHDOptSize                      0xFF2BB18C  [ROM1:0x72BB18C]
  mvrSetGopOptSizeFULLHD                   0xFF2BB370  [ROM1:0x72BB370]


## Tasks (named from ROM)

  create_init_task                         0x00003168  [low-RAM]
  init_task                                0xFF0C54CC  [ROM1:0x70C54CC]
  CreateTask                               (not in stubs.S)
  msleep                                   0x00009818  [low-RAM]
  CreateRecursiveLock                      0x0000D4D0  [low-RAM]
  task_create                              0x000098CC  [low-RAM]


## QEMU Model Parameters (from eos.c/model_list.c)

Used by QEMU-EOS for 70D emulation:
  digic_version                            5
  rom0_size                                0x800000 (8MB)
  rom1_size                                0x1000000 (16MB)
  ram_size                                 0x20000000 (512MB)
  current_task_addr                        0x7AAC0
  mpu_request_register                     0xC02200BC
  mpu_status_register                      0xC02200BC
  card_led_address                         0xC022C06C
  serial_flash_size                        0x800000
  serial_flash_cs_register                 0xC022002C
  serial_flash_cs_bitmask                  0x00000002
  rtc_cs_register                          0xC02201D4
  rtc_time_correct                         0xA0
  dedicated_movie_mode                     0 (false)
  imgpowdet_register                       0xC02201C4