#!/usr/bin/env python3
"""
DRYOS API Reference for Canon 70D (DIGIC V)

Extracted from platform/70D.112/stubs.S and QEMU model_list.c.
DryOS is a real-time operating system by DryOS Co. Ltd, used by
Canon in DIGIC camera firmware.

Kernel services are loaded as RAM modules at boot (0x000xxxxx offsets).
The ROM1-resident firmware (0xFFxxxxxx) calls these offsets through
a dynamically-loaded kernel module space.
"""

import re

def generate_dryos_api():
    output = []
    
    output.append("DRYOS API Reference — Canon 70D (FW 1.1.2, DIGIC V)")
    output.append("=" * 60)
    output.append("")
    output.append("Kernel functions loaded at 0x000xxxxx offsets (DryOS module space)")
    output.append("ROM1-resident wrappers use absolute 0xFFxxxxxx addresses")
    output.append("")
    
    with open("platform/70D.112/stubs.S") as f:
        stubs_content = f.read()
    
    # Parse all NSTUBs
    stubs = {}
    for line in stubs_content.split("\n"):
        line_s = line.strip()
        if not line_s.startswith("NSTUB("):
            continue
        m = re.match(r"NSTUB\(\s*(0x[0-9a-fA-F]+),\s*(\S+)\)", line_s)
        if m:
            addr = int(m.group(1), 16)
            name = m.group(2)
            if name.startswith("_"):
                name = name[1:]
            stubs[name] = addr

    # Categories
    categories = {
        "Task Management": ["task_create", "task_trampoline", "task_dispatch_hook",
                           "current_task", "task_max", "get_task_info_by_id",
                           "current_interrupt", "pre_isr_hook", "post_isr_hook"],
        "Memory Allocation": ["AllocateMemory", "FreeMemory", "GetMemoryInformation",
                             "GetSizeOfMaxRegion", "CreateMemorySuite", "DeleteMemorySuite",
                             "CreateMemoryChunk", "AddMemoryChunk", "GetFirstChunkFromSuite",
                             "GetNextMemoryChunk", "GetMemoryAddressOfMemoryChunk",
                             "alloc_dma_memory", "free_dma_memory"],
        "Semaphore/IPC": ["create_named_semaphore", "take_semaphore", "give_semaphore",
                         "take_semaphore_now"],
        "Message Queues": ["msg_queue_create", "msg_queue_receive", "msg_queue_post",
                          "msg_queue_count"],
        "Recursive Locks": ["CreateRecursiveLock", "AcquireRecursiveLock", "ReleaseRecursiveLock"],
        "Timers": ["SetTimerAfter", "CancelTimer", "SetHPTimerAfterNow", "SetHPTimerNextTick"],
        "Events": ["TryPostEvent", "TryPostStageEvent", "TryPostEvent_end", "TryPostStageEvent_end"],
        "EDMAC/DMA": ["StartEDmac", "StopEDmac", "SetEDmac", "AbortEDmac",
                      "ConnectReadEDmac", "ConnectWriteEDmac",
                      "RegisterEDmacCompleteCBR", "UnregisterEDmacCompleteCBR",
                      "RegisterEDmacAbortCBR", "UnregisterEDmacAbortCBR",
                      "RegisterEDmacPopCBR", "UnregisterEDmacPopCBR",
                      "CreateResLockEntry", "LockEngineResources", "UnLockEngineResources",
                      "dma_memcpy"],
        "File I/O": ["FIO_OpenFile", "FIO_CloseFile", "FIO_ReadFile", "FIO_WriteFile",
                     "FIO_CreateFile", "FIO_CreateDirectory", "FIO_RemoveFile",
                     "FIO_RenameFile", "FIO_SeekSkipFile", "FIO_GetFileSize",
                     "FIO_FindFirstEx", "FIO_FindNextEx", "FIO_FindClose"],
        "Properties": ["prop_register_slave", "prop_request_change", "prop_deliver",
                       "prop_cleanup", "PROPAD_GetPropertyData"],
        "Debug": ["DryosDebugMsg", "dm_set_store_level", "dm_names", "vsnprintf", "bzero32"],
        "GUI/Display": ["gui_main_task", "GUI_Control", "GUI_ChangeMode", "gui_init_end",
                        "SetGUIRequestMode", "MirrorDisplay", "NormalDisplay", "ReverseDisplay"],
        "Lens/AF": ["EngDrvOut", "engio_write", "shamem_read", "call",
                    "GetCFnData", "SetCFnData"],
        "Audio/ASIF": ["StartASIFDMAADC", "StopASIFDMAADC", "StartASIFDMADAC", "StopASIFDMADAC",
                       "SetNextASIFADCBuffer", "SetNextASIFDACBuffer",
                       "SetAudioVolumeIn", "SetAudioVolumeOut",
                       "SoundDevActiveIn", "SoundDevShutDownIn",
                       "PowerMicAmp", "PowerAudioOutput", "SetSamplingRate",
                       "audio_ic_read", "audio_ic_write"],
        "MVR/H.264": ["mvrFixQScale", "mvrSetDefQScale", "mvrSetFullHDOptSize",
                     "mvrSetGopOptSizeFULLHD"],
        "Tasks (named from ROM)": ["create_init_task", "init_task", "CreateTask",
                                  "msleep", "CreateRecursiveLock", "task_create"],
    }
    
    for cat_name, func_list in categories.items():
        output.append(f"\n## {cat_name}")
        output.append("")
        for name in func_list:
            if name in stubs:
                addr = stubs[name]
                if addr >= 0xFF000000:
                    region = f"ROM1:0x{addr - 0xF8000000:X}"
                    addr_str = f"0x{addr:08X}"
                elif addr >= 0x00050000:
                    region = "RAM-module"
                    addr_str = f"0x{addr:08X}"
                elif addr >= 0x00030000:
                    region = "DryOS-kernel"
                    addr_str = f"0x{addr:08X}"
                else:
                    region = "low-RAM"
                    addr_str = f"0x{addr:08X}"
                output.append(f"  {name:40s} {addr_str}  [{region}]")
            else:
                output.append(f"  {name:40s} (not in stubs.S)")
        output.append("")
    
    # Also add QEMU model parameters
    output.append("\n## QEMU Model Parameters (from eos.c/model_list.c)")
    output.append("")
    output.append("Used by QEMU-EOS for 70D emulation:")
    params = [
        ("digic_version", "5"),
        ("rom0_size", "0x800000 (8MB)"),
        ("rom1_size", "0x1000000 (16MB)"),
        ("ram_size", "0x20000000 (512MB)"),
        ("current_task_addr", "0x7AAC0"),
        ("mpu_request_register", "0xC02200BC"),
        ("mpu_status_register", "0xC02200BC"),
        ("card_led_address", "0xC022C06C"),
        ("serial_flash_size", "0x800000"),
        ("serial_flash_cs_register", "0xC022002C"),
        ("serial_flash_cs_bitmask", "0x00000002"),
        ("rtc_cs_register", "0xC02201D4"),
        ("rtc_time_correct", "0xA0"),
        ("dedicated_movie_mode", "0 (false)"),
        ("imgpowdet_register", "0xC02201C4"),
    ]
    for name, val in params:
        output.append(f"  {name:40s} {val}")
    
    return "\n".join(output)


if __name__ == "__main__":
    import os
    os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    api_doc = generate_dryos_api()
    with open("tools/ghidra/DRYOS_API.md", "w") as f:
        f.write(api_doc)
    print(f"Generated: tools/ghidra/DRYOS_API.md ({len(api_doc.split(chr(10)))} lines)")
