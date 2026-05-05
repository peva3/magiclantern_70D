# EOS M 2.0.2 — 4-Phase Boot Trace
# Usage:
#   ./test_qemu.sh EOSM --boot-trace
# Or manually:
#   cd qemu-eos/build && ../arm-softmmu/qemu-system-arm -M EOSM -nographic -s -S &
#   arm-none-eabi-gdb -x ../magiclantern/cam_config/EOSM/boot.gdb

source -v debug-logging.gdb
source -v EOSM/patches.gdb

# ── Phase 1: Firmware Entry ────────────────────────────────────────────────────
printf "\n=== Phase 1: Firmware Entry ===\n"

# MAIN_FIRMWARE_ADDR = firmware_entry
b *0xFF0C0000
commands
  printf "=== FIRMWARE ENTRY at 0xFF0C0000 ===\n"
  printf "R0=0x%08X R1=0x%08X SP=0x%08X LR=0x%08X\n" $r0 $r1 $sp $lr
  x/4i 0xFF0C0000
  boot_phase_summary
end

# cstart at 0xFF0C1BA8
b *0xFF0C1BA8
commands
  printf "=== CSTART at 0xFF0C1BA8 ===\n"
  printf "R0=0x%08X SP=0x%08X LR=0x%08X\n" $r0 $sp $lr
end

# ── Phase 2: init_task → CreateTaskMain ────────────────────────────────────────
printf "\n=== Phase 2: ==="

# init_task at 0xFF0C54CC
b *0xFF0C54CC
commands
  printf "=== INIT_TASK at 0xFF0C54CC ===\n"
  printf "R0=0x%08X SP=0x%08X\n" $r0 $sp
end

# CreateTaskMain
b *0xFF0C314C
commands
  printf "=== CreateTaskMain ===\n"
  printf "SP=0x%08X\n" $sp
  boot_phase_summary
end

# ── Phase 3: Task Creation Cascade ─────────────────────────────────────────────
printf "\n=== Phase 3: Task Creation ===\n"

b *0x7048
commands
  silent
  if $_tcall < 20
    printf "TASK_CREATE: "
    x/1s $r1
    set $_tcall = $_tcall + 1
  end
  cont
end

set $_tcall = 0

b *0x68F0
commands
  silent
  if $_icall < 10
    printf "REG_INTERRUPT: %d\n" $r0
    set $_icall = $_icall + 1
  end
  cont
end

set $_icall = 0

b *0xFF137B40
commands
  silent
  if $_rcall < 10
    printf "REG_FUNC: "
    x/1s $r0
    set $_rcall = $_rcall + 1
  end
  cont
end

set $_rcall = 0

b *0x21F78
commands
  silent
  if $_scall < 10
    printf "CREATE_STATE_OBJECT: type=%d\n" *(int*)$r0
    set $_scall = $_scall + 1
  end
  cont
end

set $_scall = 0

# assert handler
b *0x1900
commands
  silent
  printf "=== ASSERT: %s (condition %s) at %s:%d ===\n" $r0 $r1 $r2 $r3
  bt
  cont
end

# ── Phase 4: ML Boot Monitoring ────────────────────────────────────────────────
printf "\n=== Phase 4: ML Boot (RESTARTSTART monitoring) ===\n"

# Watch RESTARTSTART area for ML loading
# RESTARTSTART = 0x9E1E0 (from Makefile)
set $ml_load = 0x9E1E0

printf "ML load area: 0x%08X\n" $ml_load

printf "\n=== EOSM 4-Phase Boot Trace Active ===\n"
printf "Target: firmware entry → cstart → init_task → CreateTaskMain → task creation → ML boot\n\n"

# ── Helper commands ────────────────────────────────────────────────────────────
define boot_phase_summary
  printf "PC=0x%08X  SP=0x%08X  LR=0x%08X\n" $pc $sp $lr
  printf "R0=0x%08X R1=0x%08X R2=0x%08X R3=0x%08X\n" $r0 $r1 $r2 $r3
  printf "CURRENT_TASK=0x%08X\n" *(int*)0x3DE78
end
document boot_phase_summary
  Print EOS M boot state summary
end

# ── Optional: MPU Communication Trace ─────────────────────────────────────────
# Uncomment to enable MPU send/recv tracing
# printf "\nTo enable MPU tracing, uncomment in debugmsg.gdb\n"

continue
