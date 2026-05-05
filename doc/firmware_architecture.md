# Canon 70D Firmware Architecture

## Discovered 2026-05-05 — Deep RE Analysis

## 1. ARM Exception Vector Table

**Location:** Runtime 0xFFFF0000 (ARM high vectors), ROM1+0xFF0000  
**Size:** 8 entries × 4 bytes = 32 bytes  
**Format:** Each entry is `LDR PC, [PC, #+0x18]` — loads handler address from table at 0xFFFF0020

| Vector | Offset | Handler | Description |
|--------|--------|---------|-------------|
| Reset | 0xFFFF0000 | 0xFFFF0044 | Main reset handler (NOT cstart) |
| Undefined Instr | 0xFFFF0004 | 0xFFFF0A48 | Exception dispatch stub #0 |
| SWI | 0xFFFF0008 | 0xFFFF0A60 | Exception dispatch stub #1 |
| Prefetch Abort | 0xFFFF000C | 0xFFFF0A80 | Exception dispatch stub #2 |
| Data Abort | 0xFFFF0010 | 0xFFFF0A98 | Exception dispatch stub #3 |
| Reserved | 0xFFFF0014 | 0x00000000 | Unused |
| IRQ | 0xFFFF0018 | 0xFFFF0AB0 | Exception dispatch stub #4 |
| FIQ | 0xFFFF001C | 0xFFFF0AC8 | Exception dispatch stub #5 |

**Handler address table** at runtime 0xFFFF0020 (ROM1+0xFF0020):
```
0xFFFF0020: 0xFFFF0044  (Reset)
0xFFFF0024: 0xFFFF0A48  (Undefined)
0xFFFF0028: 0xFFFF0A60  (SWI)
0xFFFF002C: 0xFFFF0A80  (PrefetchAbort)
0xFFFF0030: 0xFFFF0A98  (DataAbort)
0xFFFF0034: 0x00000000  (Reserved)
0xFFFF0038: 0xFFFF0AB0  (IRQ)
0xFFFF003C: 0xFFFF0AC8  (FIQ)
```

## 2. Exception Dispatch Stubs

**Location:** ROM1+0xFF0A48-0xFF0AE0 (runtime 0xFFFF0A48-0xFFFF0AE0)  
**Each stub is 8 bytes** with pattern:

```
PUSH {r0-r12, lr}      (save all registers)
MOV r1, lr / MOV r10, lr  (save exception LR, encodes exception type)
ADD r0, PC, #offset    (load descriptor for this exception)
BL -> 0xFFFF4CD0       (call main dispatcher)
POP {r0-r12, lr}       (restore all)
SUBS PC, LR, #N        (return from exception, N varies by type)
```

**Return behavior by exception type:**
- Undefined, SWI: `MOV PC, LR` (normal return)
- PrefetchAbort: `SUBS PC, LR, #4` (retry faulting instruction)
- DataAbort, IRQ, FIQ: `SUBS PC, LR, #8` (skip faulting instruction)
- FIQ: `SUBS PC, LR, #4`

The main dispatcher at **0xFFFF4CD0** (ROM1+0xFF4CD0) receives:
- r0: descriptor pointer (loaded by ADD r0, PC, #offset in stub)
- r1: the saved LR value (encodes the exception type)
- It looks up the handler function from the descriptor and dispatches

## 3. Reset Handler (0xFFFF0044)

**Location:** ROM1+0xFF0044, runtime 0xFFFF0044

This is the true entry point when the CPU comes out of reset. It:
1. Sets up the stack pointer for each CPU mode (FIQ, IRQ, SVC, ABT, UND, SYS)
2. Configures the MMU/CP15 for high vectors
3. Jumps to cstart (0xFF0C1BA8) for C runtime initialization

## 4. cstart — C Runtime Initialization

**Location:** ROM1+0x0C1BA8, runtime 0xFF0C1BA8  
**Size:** 116 bytes (29 instructions)

cstart is a **parameter packager** — it does NOT do the actual initialization itself.
Instead it:

1. Allocates 0x74 bytes on stack
2. Populates a `boot_params` struct on the stack:
   ```
   +0x00: 0x0016AC00  (heap start / memory region)
   +0x04: 0x00120000  (IRQ/FIQ stack top)
   +0x08: 0x000FAE30  (.bss start)
   +0x0C: 0x00063E44  (.bss size = end - start = 0x15EC74 - 0xFAE30)
   +0x10: 0x0015EC74  (.bss end)
   +0x14: 0x0016AC00
   +0x18: 0x22         (IRQ stack size)
   +0x1C: 0x84
   +0x20: 0x1B0
   +0x24: 0x284
   +0x28: 0x98
   +0x2C: 0x85
   +0x30: 0x40
   +0x34: 0x04
   +0x5C: 0x10
   +0x60: 0x800
   +0x64: 0xA0 (160)
   +0x68: 0x280 (640)
   +0x6C: init_task_ptr = 0xFF0C54CC
   ```
3. Calls the boot init function via BL -> wraps to **0x00071618** (RAM module at runtime)

The BL to the init function uses the **wrap-around mechanism**: the PC-relative offset
is calculated so that in ROM1 it targets 0xF9071618, but at runtime (after firmware
copy to RAM at 0xFFxxxxxx) the same instruction wraps to 0x00071618.

The values in the boot_params struct are memory region descriptors and stack sizes —
they are offsets within the firmware's internal RAM region (not ROM addresses).

## 5. DryOS Dispatch Mechanisms

### 5.1 Wrap-around BL (Core Mechanism)

The fundamental dispatch mechanism for calling kernel services from firmware:
- Firmware code at 0xFF0xxxxx uses BL/instructions with large positive offsets
- The addition wraps around 32-bit address space to target 0x000xxxxx (RAM module space)
- Example: BL at 0xFF0C1BB8 has offset24=0x3EBE96 → target wraps to 0x00071618
- This works because the unsigned 32-bit arithmetic overflows: 0xFF0xxxxx + N → 0x000xxxxx

### 5.2 Jump Table (ROM1+0xBA4BB0)

An array of 82+ jump thunks at ROM1+0xBA4BB0. Each entry is:

```
LDR PC, [PC, #offset]   ; load target address from literal pool
```

Each entry targets a RAM module function. The jump table provides
a stable ABI for calling kernel services without knowing the exact
RAM module load address.

### 5.3 NSTUB Descriptor Table (ROM1+0xFF0A48+)

8-byte thunks that save registers, load a descriptor from a literal pool,
and BL to the main dispatcher. Used by exception handlers and the ML NSTUB system.

## 6. Key Observations

1. **No SWI/SVC syscalls**: DIGIC V DryOS does NOT use ARM SWI/SVC for kernel
   entry. Instead, all kernel function calls go through BL to RAM module addresses.

2. **ROM is larger than 16MB**: The BL targets at 0xF9071618 and 0xF9003168
   are ~16.4MB into the ROM, suggesting ROM1 is 32MB minimum. Our 16MB dump
   is incomplete.

3. **cstart is trivial**: Only 29 instructions. It just builds a struct and
   delegates to a RAM module for actual initialization. The REAL boot logic
   (firmware copy, BSS zero, hardware init) is in the RAM-loaded boot module.

4. **Exception dispatch is centralized**: All exceptions go through a single
   dispatcher at 0xFFFF4CD0. The exception type is encoded in the saved LR.

5. **Wrap-around BL is intentional**: The firmware is compiled knowing that
   BL targets will wrap around at 0xFFFFFFFF to reach 0x0000xxxx.
