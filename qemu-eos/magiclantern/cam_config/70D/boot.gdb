# 70D Boot Trace GDB Script
# Enhanced boot-phase debugging for Canon 70D 1.1.2
#
# Usage:
#   ./run_canon_fw.sh 70D -s -S & arm-none-eabi-gdb -x 70D/boot.gdb
#   ./run_canon_fw.sh 70D,firmware="boot=1" -s -S & arm-none-eabi-gdb -x 70D/boot.gdb
#
# This script traces the complete boot sequence:
#   Phase 1: Firmware entry -> cstart -> init_task
#   Phase 2: CreateTaskMain -> AllocateMemory pool init
#   Phase 3: Task creation cascade (DryOS kernel startup)
#   Phase 4: ML initialization (if boot=1)
#
# For lighter tracing, use debugmsg.gdb instead.

source -v debug-logging.gdb

# Firmware patches (applied inline to avoid double target remote from patches.gdb)
# patch sio_send_retry - prevents infinite "send retrying..." loop
set *(int*)0xFF33A570 = 0xe3a00000
set *(int*)0xFF33A574 = 0xe12fff1e

macro define CURRENT_TASK 0x7AAC0
macro define CURRENT_ISR (MEM(0x648) ? MEM(0x64C) >> 2 : 0)

# symbol-file ../magic-lantern/platform/70D.112/magiclantern

################################################################################
# Phase 1: Firmware Entry & Early Boot
################################################################################

# cstart - first C code executed after ROM boot
b *0xFF0C1BA8
commands
silent
print_current_location
KGRN
printf "=== PHASE 1: cstart (0xFF0C1BA8) ===\n"
KRESET
printf "  sp=%08X lr=%08X\n", $sp, $lr
c
end

# init_task - DryOS first task
b *0xFF0C54CC
commands
silent
print_current_location
KGRN
printf "=== PHASE 2: init_task (0xFF0C54CC) ===\n"
KRESET
printf "  r0=%08X r1=%08X r2=%08X r3=%08X\n", $r0, $r1, $r2, $r3
c
end

# CreateTaskMain - sets up AllocateMemory pool, creates system tasks
b *0xFF0C314C
commands
silent
print_current_location
KGRN
printf "=== PHASE 2: CreateTaskMain (0xFF0C314C) ===\n"
KRESET
printf "  AllocMem pool: R0(start)=%08X R1(end)=%08X\n", $r0, $r1
c
end

# AllocateMemory init pool call (ROM_ALLOCMEM_INIT = 0xFF0C3178)
b *0xFF0C3178
commands
silent
print_current_location
KYEL
printf "  AllocateMemory_init_pool called: start=%08X end=%08X\n", $r0, $r1
KRESET
c
end

# Rename one of the two Startup tasks (prevent name collision)
b *0xff0c3314
commands
silent
set $r0 = 0xFF0C2C80
c
end

################################################################################
# Phase 3: Task Creation & Kernel Services
################################################################################

# task_create - logs all DryOS tasks as they're created
b *0x98CC
task_create_log

# register_interrupt - logs all interrupt handlers
b *0x9174
register_interrupt_log

# register_func - logs registered DryOS functions
b *0xFF1442C0
register_func_log

# CreateStateObject - logs state machine creation
b *0x3D970
CreateStateObject_log

# asserts - critical for catching firmware errors early
b *0x1900
assert_log

################################################################################
# Phase 4: ML Boot (if boot=1)
################################################################################

# ML boot key addresses from consts.h:
#   DRYOS_ASSERT_HANDLER = 0x7AAA0
#   RESTARTSTART = 0x44C100 (ML load address)
#   Pool: 0x4E0000 - 0xD3C000 (592KB reserved)

# Watch DRYOS_ASSERT_HANDLER installation
rwatch *0x7AAA0
commands
silent
KGRN
printf "=== DRYOS_ASSERT_HANDLER written: %08X ===\n", *(int*)0x7AAA0
KRESET
c
end

# Watch ML load address area for code arrival (first 4 bytes at RESTARTSTART)
rwatch *0x44C100
commands
silent
KGRN
printf "=== ML code area accessed at 0x44C100 ===\n"
KRESET
c
end

################################################################################
# Optional: MPU Communication (disabled by default - very verbose)
################################################################################

if 0
b *0x396bc
mpu_send_log

b *0x5ed0
mpu_recv_log
end

################################################################################
# Optional: Property System (disabled by default - very verbose)
################################################################################

if 0
b *0xFF12AB14
prop_request_change_log

b *0xFF31E250
mpu_analyze_recv_data_log

b *0xFF31B408
prop_lookup_maybe_log

b *0xFF3247B8
mpu_prop_lookup_log
end

################################################################################
# Optional: Semaphores (disabled by default)
################################################################################

if 0
b *0x8420
create_semaphore_log

b *0x8580
take_semaphore_log

b *0x866C
give_semaphore_log
end

################################################################################
# Optional: Timers (disabled by default)
################################################################################

if 0
b *0xE8AC
SetTimerAfter_log

b *0x7F94
SetHPTimerAfterNow_log

b *0x8094
SetHPTimerNextTick_log

b *0xEAAC
CancelTimer_log
end

################################################################################
# Boot Phase Summary Helper
################################################################################

define boot_phase_summary
printf "\n"
KGRN
printf "=== 70D Boot Phase Summary ===\n"
KRESET
printf "CURRENT_TASK:     %08X\n", *(int*)0x7AAC0
printf "DRYOS_ASSERT:     %08X\n", *(int*)0x7AAA0
printf "AllocMem pool:    0x4E0000 - 0xD3C000\n"
printf "ML load addr:     0x44C100 (RESTARTSTART)\n"
printf "init_task:        0xFF0C54CC\n"
printf "cstart:           0xFF0C1BA8\n"
printf "CreateTaskMain:   0xFF0C314C\n"
end
document boot_phase_summary
Print a summary of 70D boot phase key addresses and current state.
end

define boot_enable_mpu
enable 6
enable 7
printf "MPU send/recv logging enabled\n"
end
document boot_enable_mpu
Enable MPU communication logging (very verbose).
end

define boot_enable_props
enable 8
enable 9
enable 10
enable 11
printf "Property system logging enabled\n"
end
document boot_enable_props
Enable property system logging (very verbose).
end

define boot_enable_semaphores
enable 12
enable 13
enable 14
printf "Semaphore logging enabled\n"
end
document boot_enable_semaphores
Enable semaphore logging (verbose).
end

define boot_enable_timers
enable 15
enable 16
enable 17
enable 18
printf "Timer logging enabled\n"
end
document boot_enable_timers
Enable timer logging.
end

printf "\n70D Boot Trace GDB script loaded.\n"
printf "Phase 1-4 breakpoints active. MPU/props/semaphores/timers disabled.\n"
printf "Helper commands:\n"
printf "  boot_phase_summary  - print key address state\n"
printf "  boot_enable_mpu     - enable MPU logging\n"
printf "  boot_enable_props   - enable property logging\n"
printf "  boot_enable_semaphores - enable semaphore logging\n"
printf "  boot_enable_timers  - enable timer logging\n\n"
