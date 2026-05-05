# DRYOS Kernel Dispatch Architecture — Canon 70D (DIGIC V)

## Overview

The 70D runs DRYOS version 2.3, release #0051 on an ARM946E-S CPU (ARMv5TE).
Unlike typical ARM systems that use SWI/SVC for system calls, DIGIC V uses a
hybrid dispatch mechanism with three complementary techniques:

1. **RAM-loaded modules** — kernel functions at 0x000xxxxx
2. **Jump table in ROM1** — LDR PC trampolines at ROM1+0xBA4BB0  
3. **Wrap-around BL** — firmware code at 0xFF0Cxxxx+ calls RAM via 32-bit wrap

---

## 1. RAM-Loaded Modules

Kernel services (CreateTask, msleep, semaphore, socket API, etc.) are loaded
into RAM at 0x000xxxxx addresses during boot. These are NOT stored in ROM1
at those offsets — the binary data at ROM1[0x0000-0x0BFFFF] is 0xFFFFFFFF
(uninitialized).

The boot code (cstart) copies module code from another source (likely from
within the firmware image) to these addresses.

### Known RAM Module Addresses

| Address | Function | Verified |
|---------|----------|----------|
| 0x00002359 | CreateTask (?) | Jump table entry |
| 0x00001E45 | CreateTask (?) | Jump table entry |
| 0x00001F23 | task. related | Jump table entry |
| 0x00002379 | task. related | Jump table entry |
| 0x00001F93 | task. related | Jump table entry |
| 0x00002381 | task. related | Jump table entry |
| 0x0000234B | (kernel) | Jump table entry |
| 0x00006904 | WiFi power/clock | cstart BL target, known from WiFi RE |
| 0x00059AF8 | socket_create | hw_test v15 validated (PUSH prologue) |
| 0x00059E94 | socket_bind | hw_test v15 validated |
| 0x00059DDC | socket_connect | hw_test v15 validated |
| 0x0005A9D0 | socket_listen | hw_test v15 validated |
| 0x00059CE8 | socket_recv | hw_test v15 validated |
| 0x0005A09C | socket_send | hw_test v15 validated |
| 0x0005A810 | socket_setsockopt | hw_test v15 validated |

### cstart BL calls to RAM (36 unique targets)

During boot, cstart calls these RAM functions to initialize kernel subsystems:

```
0x00001900 0x00001944 0x0000259C 0x00003168 0x0000343C 0x00003928
0x0000603C 0x0000605C 0x000066A8 0x00006904 0x00006C70 0x00007308
0x0000758C 0x00007A90 0x000082AC 0x00008664 0x000091B8 0x00009504
0x000096CC 0x00009864 0x000098CC 0x000099C4 0x00009A54 0x00009A9C
0x00009E84 0x00009EB0 0x00009EFC 0x00009F50 0x0000A028 0x0000B120
0x0000D27C 0x0000D6DC 0x0000E520 0x000710F0 0x00071618 0x00077ECC
```

---

## 2. Jump Table at ROM1+0xBA4BB0

A position-independent jump table exists at ROM1+0xBA4BB0 (runtime 0xFFBA4BB0
via ROM1 mirror). Each entry is 8 bytes:

```
LDR PC, [PC, #4]    ; (ARM: 0xE51FF004) — load next word into PC
<target_address>     ; 4-byte RAM module address
```

This is in ROM1 (read-only) and contains hardcoded dispatch entries for the
RAM module functions. The firmware code at 0xFF0xxxxx jumps here, which then
redirects to the actual RAM module function.

### Table Layout

| Entry | ROM1 Offset | RAM Target | Function (inferred) |
|-------|-------------|------------|---------------------|
| 0 | 0xBA4BB0 | 0x0000234B | Kernel init |
| 1 | 0xBA4BB8 | 0x00002359 | Kernel init |
| 2 | 0xBA4BC0 | 0x00001E45 | Task mgmt |
| 3 | 0xBA4BC8 | 0x00001F23 | Task mgmt |
| ... | ... | ... | ... |
| 35 | 0xBA4EA0 | 0x00003FAB | |
| 36 | 0xBA4EA8 | 0x00005EA1 | Socket range |
| 37 | 0xBA4EB0 | 0x00005CD1 | Socket range |
| 38 | 0xBA4EB8 | 0x00005E03 | Socket range |
| 39 | 0xBA4EC0 | 0x00000991 | |
| 40 | 0xBA4EC8 | 0x00005D2D | Socket range |
| 41 | 0xBA4ED0 | 0x00000B79 | |
| 42 | 0xBA4ED8 | 0x00005DE5 | Socket range |
| 43 | 0xBA4EE0 | 0x00005E79 | Socket range |
| 44 | 0xBA4EE8 | 0x00000BE3 | |
| 45 | 0xBA4EF0 | 0x0000426F | |
| 46 | 0xBA4EFC | 0x00005F39 | |
| 47 | 0xBA4F04 | 0x000035C3 | |
| 48 | 0xBA4F0C | 0x000036E5 | |
| 49 | 0xBA4F14 | 0x00003613 | |
| 50 | 0xBA4F1C | 0x00005FF1 | |
| 51 | 0xBA4F24 | 0x00004045 | |
| 52 | 0xBA4F2C | 0x000062B9 | |
| +53+ | scattered | various | Smaller tables nearby |

~82 total jump entries in the ROM1+0xBA4Bxx region.

---

## 3. Wrap-Around BL Mechanism

The firmware code at 0xFF0Cxxxx+ calls RAM functions at 0x000xxxxx using
standard ARM BL instructions. Since the firmware is at high addresses and
RAM is at low addresses, the BL offset wraps around the 32-bit address space:

```
firmware_addr = 0xFF0Cxxxx
target        = 0x0000xxxx

BL_offset = (target - (firmware_addr + 8)) / 4
         = (0x0000xxxx - 0xFF0Cyyyy) / 4
         = large positive value (wraps at 32 bits)
```

Example: BL from 0xFF0C2994 to 0x00006904
```
target = (0xFF0C2994 + 8 + offset*4) & 0xFFFFFFFF
offset = 0x3D0FDC  (encodes as imm24 in BL instruction)
```

This is the ONLY dispatch mechanism used internally by Canon firmware.
There are NO SWI/SVC system call instructions anywhere in the firmware.

---

## 4. NSTUB Dispatch (for ML usage)

Magic Lantern's NSTUB table defines kernel function entry points at 0xFF0Axxxx.
These addresses are NOT executable code in ROM1 — the data at ROM1 offsets
0x0Axxxx is a descriptor/initialization table used by the boot process.

On real hardware, the 0xFF0Axxxx region is WRITEABLE (likely mapped to RAM
through the memory controller or caching configuration). During boot, cstart:

1. Reads the descriptor table at 0xFF0Axxxx (in ROM1 via mirror)
2. For each entry, calls a RAM function that writes an LDR PC trampoline
   at the appropriate address

The trampoline format written to 0xFF0Axxxx:
```asm
LDR PC, [PC, #0]    ; @ 0xFF0Axxxx: 0xE51FF004
<ram_module_addr>    ; @ 0xFF0Axxxx+4
```

The QEMU model maps 0xFF000000 as a read-only mirror of ROM1 (0xF8000000).
This is INCORRECT for the 0xFF0xxxxx range on real hardware — real hardware
has writeable RAM there, populated by the boot code.

### NSTUB → RAM Module Mapping

The exact mapping of NSTUB addresses to RAM module addresses is determined
by the boot-time descriptor table at 0xFF0Axxxx. This table has not been
fully decoded, but the format appears to be:

```
<nstub_addr> <ram_module_addr>   ; in the descriptor table
```

Note: Not all NSTUBs use this mechanism. Some (like call() at 0xFF14439C)
are actual ROM1 code since their addresses are at 0xFF0Cxxxx+ (within the
firmware copy region).

---

## 5. FIO Dispatch

File I/O functions (FIO_OpenFile, etc.) use a different mechanism.
They dispatch through a filesystem driver struct containing a function
pointer table. The NSTUB addresses for FIO functions are:

- FIO_OpenFile: 0xFF0E0B48 — in the firmware code region (ROM1+0x0E0B48)
  - Contains "MVP_SetSpeedParams\0" string — NOT executable code
- This is actually a RAM-loaded module accessed via a function pointer table

The struct layout (from RAM dump analysis):
```c
struct fio_driver {
    int (*open)(...);     // offset 0x00
    int (*close)(...);    // offset 0x04 
    int (*read)(...);     // offset 0x08
    int (*write)(...);    // offset 0x0C
    int (*create)(...);   // offset 0x10
    int (*remove)(...);   // offset 0x14
    // ...
};
```

FIO functions dispatch through these struct pointers rather than through
the jump table.

---

## Summary

| Dispatch Mechanism | Used For | Address Range |
|--------------------|----------|---------------|
| RAM-loaded modules | All kernel functions | 0x000xxxxx |
| Jump table (ROM1) | System call dispatch | 0xF8BA4BB0+ |
| Wrap-around BL | Firmware → RAM calls | 0xFF0Cxxxx → 0x0000xxxx |
| Descriptor table | Boot-time fixup | 0xFF0Axxxx (in ROM1) |
| Function pointer table | FIO filesystem | RAM at 0x000xxxxx |

## Key Architectural Difference from Other ARM Systems

- **No SWI/SVC instructions** — DIGIC V does NOT use ARM software interrupts
- **RAM module loader** — kernel code is loaded into low RAM at boot
- **ROM has both data tables and jump tables** for dispatch
- **QEMU model limitation** — 0xFF000000 is incorrectly modeled as ROM1 mirror;
  real hardware has writeable RAM here populated by cstart
