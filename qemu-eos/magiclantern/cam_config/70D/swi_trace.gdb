# 70D DRYOS SWI (Supervisor Interrupt) Dispatch Trace
# Traces the dynamic SWI handler installed by DRYOS at boot.
#
# The ARM vector table is at 0xFFFF0000 (high vectors).
# The SWI vector at 0xFFFF0008 is written by DRYOS during initialization
# with the address of the SWI handler. This handler is NOT in ROM1,
# it's installed dynamically in RAM at boot.
#
# Usage:
#   ./run_swi_trace.sh
#
# Or manually:
#   qemu-system-arm -M 70D ... -s -S &
#   arm-none-eabi-gdb -x 70D/swi_trace.gdb
#
# This script:
#   1. Sets a hardware watchpoint on 0xFFFF0008 (SWI vector)
#   2. When DRYOS writes the handler address, reads it and sets a breakpoint
#   3. On each SWI entry, extracts the syscall number from the SWI instruction
#      at LR-4 (ARM mode SWI encoding: 0xEFnnnnnn where nnnnnn = 24-bit immediate)
#   4. Prints syscall info, return address, R12, and backtrace
#   5. Limits output to first 100 SWI calls

source -v debug-logging.gdb

# 70D-specific task/interrupt detection
macro define CURRENT_TASK 0x7AAC0
macro define CURRENT_ISR  (MEM(0x648) ? MEM(0x64C) >> 2 : 0)

set pagination off
set print frame-arguments none

# SWI trace state variables
set $swi_count = 0
set $swi_max = 100

################################################################################
# Phase 1: Watch for SWI handler installation at 0xFFFF0008
################################################################################

printf "\n"
KGRN
printf "=== 70D SWI Trace ===\n"
KRESET
printf "Watching 0xFFFF0008 for DRYOS SWI handler installation...\n\n"

# Hardware watchpoint on the SWI vector (4 bytes, write)
watch *(unsigned int*)0xFFFF0008
set $swi_watch_num = $bpnum

commands $swi_watch_num
    silent
    set $handler = *(unsigned int*)0xFFFF0008
    if $handler != 0 && $handler != $swi_handler_saved
        set $swi_handler_saved = $handler
        KGRN
        printf "\n=== SWI HANDLER INSTALLED ===\n"
        KRESET
        printf "  Handler address: 0x%08X\n", $handler
        if $handler >= 0xFF000000
            printf "  Location: ROM1 (firmware code)\n"
        else
            printf "  Location: RAM (dynamic DRYOS code)\n"
        end
        print_current_location
        printf "\n"

        # Switch from watchpoint to breakpoint on the handler
        # Verify it looks like valid ARM code (check for PUSH prologue)
        set $prologue = *(unsigned int*)$handler
        if ($prologue & 0xFFF00000) == 0xE9200000 || ($prologue & 0xFFFF0000) == 0xE92D0000
            printf "  Prologue: PUSH {registers} (valid ARM code)\n"
        else
            if ($prologue & 0xFFFF0000) == 0xE24FE000
                printf "  Prologue: SUB LR, PC, #imm (common SWI handler pattern)\n"
            else
                printf "  Prologue: 0x%08X (unknown, but setting breakpoint anyway)\n", $prologue
            end
        end
        printf "\n"

        # Set breakpoint on the SWI handler
        b *$handler
        set $swi_bp_num = $bpnum

        commands $swi_bp_num
            silent
            set $swi_count = $swi_count + 1
            if $swi_count <= $swi_max
                # Extract syscall number from the SWI instruction at LR-4
                # ARM SWI encoding: cond[31:28]=1110, 1111[27:24], imm24[23:0]
                # Full encoding: 0xEFnnnnnn where nnnnnn = 24-bit immediate
                set $swi_addr = $lr - 4
                set $swi_insn = *(unsigned int*)$swi_addr

                # Check if it's really an ARM-mode SWI (0xEF prefix)
                if ($swi_insn & 0xFF000000) == 0xEF000000
                    set $syscall_num = $swi_insn & 0x00FFFFFF

                    # Color-code: green for known-range syscalls, yellow for unusual
                    if $syscall_num < 0x100
                        KGRN
                    else
                        if $syscall_num < 0x1000
                            KYLW
                        else
                            KRED
                        end
                    end
                    printf "[SWI #%3d] 0x%06X (%5d)", $swi_count, $syscall_num, $syscall_num
                    KRESET
                else
                    # THUMB mode SVC? 0xDF prefix
                    if ($swi_insn & 0xFF00) == 0xDF00
                        set $syscall_num = $swi_insn & 0xFF
                        KYEL
                        printf "[SWI #%3d] THUMB SVC %d", $swi_count, $syscall_num
                        KRESET
                    else
                        # Not an SWI instruction - maybe syscall number in R12
                        KRED
                        printf "[SWI #%3d] NOT SWI at LR-4! insn=0x%08X", $swi_count, $swi_insn
                        KRESET
                        set $syscall_num = -1
                    end
                end

                # Print return context
                printf "  LR=0x%08X  R0=0x%08X  R12=0x%08X  SP=0x%08X\n", \
                    $lr, $r0, $r12, $sp

                # Print ret_addr location hint (ROM1, ROM0, or RAM)
                if $lr >= 0xFF000000 && $lr < 0xFFFFFFFF
                    printf "  Return location: ROM1\n"
                else
                    if $lr >= 0xF0000000 && $lr < 0xF8000000
                        printf "  Return location: ROM0 (assets)\n"
                    else
                        if $lr >= 0x40000000 && $lr < 0x60000000
                            printf "  Return location: RAM\n"
                        else
                            printf "  Return location: 0x%08X\n", $lr
                        end
                    end
                end

                # Print backtrace for first 20 SWI calls
                if $swi_count <= 20
                    printf "  Backtrace:\n"
                    bt 1
                end

                # For the first call, print full register context
                if $swi_count == 1
                    printf "  Full register context (first SWI call):\n"
                    info registers
                end
            end

            if $swi_count == $swi_max
                printf "\n"
                KGRN
                printf "=== SWI trace limit reached (%d calls) ===\n", $swi_max
                printf "Disabling breakpoint. Enable with: enable %d\n", $swi_bp_num
                KRESET
                printf "Summary: %d SWI calls traced.\n", $swi_count
                disable $swi_bp_num
            end
            c
        end

        # Remove the watchpoint now that we have the handler
        delete $swi_watch_num

        KGRN
        printf "Watchpoint removed. Breakpoint active at 0x%08X.\n", $handler
        printf "Tracing up to %d SWI calls...\n\n", $swi_max
        KRESET
    end
    c
end

################################################################################
# Phase 2: Boot patches and auxiliary breakpoints
################################################################################

# Patch sio_send_retry to prevent firmware boot loop
set *(int*)0xFF33A570 = 0xe3a00000
set *(int*)0xFF33A574 = 0xe12fff1e

# Log task creation for context (which tasks make SWI calls)
b *0x98CC
commands
    silent
    print_current_location
    KBLU
    printf "task_create(%s, prio=%x, stack=%x, entry=%x, arg=%x)\n", STR($r0), $r1, $r2, $r3, MEM($sp)
    KRESET
    c
end

# Log interrupt registration
b *0x9174
commands
    silent
    print_current_location
    if $r0 && ((char*)$r0)[0]
        printf "register_interrupt(%s, 0x%x, 0x%x, 0x%x)\n", $r0, $r1, $r2, $r3
    end
    c
end

# Log asserts
b *0x1900
commands
    silent
    print_current_location
    KRED
    printf "ASSERT: %s at %s:%d, %x\n", STR($r0), STR($r1), $r2, $lr
    KRESET
    c
end

################################################################################
# Summary
################################################################################

printf "\n"
KGRN
printf "=== 70D SWI Trace Active ===\n"
KRESET
printf "  Watchpoint:  *(int*)0xFFFF0008 (SWI vector)\n"
printf "  Max SWI calls to trace: %d\n", $swi_max
printf "  Task creation logging:  ON\n"
printf "  Interrupt registration: ON\n"
printf "  Assert logging:         ON\n"
printf "\n"

c
