#!/usr/bin/env bash
#
# test_70d_qemu.sh — One-command QEMU 70D test: build ML, launch QEMU, capture boot log
#
# Usage:
#   ./test_70d_qemu.sh              # Quick test (10s timeout, no GDB)
# ./test_70d_qemu.sh --gdb # Launch with GDB server on port 1234
# ./test_70d_qemu.sh --gdb --boot-trace # Launch with GDB + boot-trace script
#   ./test_70d_qemu.sh --full       # Full boot attempt (60s timeout)
#   ./test_70d_qemu.sh --mpu        # MPU debug logging
#   ./test_70d_qemu.sh --debugmsg   # Enable DebugMsg logging
#   ./test_70d_qemu.sh --boot       # Boot from ML autoexec.bin on SD card
#   ./test_70d_qemu.sh --no-build   # Skip ML build step
#
# ROM dumps must be in /app/70d/roms/70D/ (ROM0.BIN, ROM1.BIN, SFDATA.BIN)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ML_DIR="$SCRIPT_DIR"
QEMU_DIR="$SCRIPT_DIR/qemu-eos"
ROM_DIR="$SCRIPT_DIR/roms"
LOG_DIR="$SCRIPT_DIR/logs"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Defaults
TIMEOUT=10
GDB_MODE=0
BOOT_TRACE=0
BUILD=1
BOOT_MODE=0
D_ARGS=()
SERIAL_FILE=""
DISPLAY_OPT="-display none"

while [[ $# -gt 0 ]]; do
    case "$1" in
--gdb) GDB_MODE=1; TIMEOUT=120; shift ;;
    --boot-trace) GDB_MODE=1; BOOT_TRACE=1; TIMEOUT=120; shift ;;
        --full)     TIMEOUT=60; shift ;;
        --mpu)      D_ARGS+=("mpu"); shift ;;
        --debugmsg) D_ARGS+=("debugmsg"); shift ;;
        --io)       D_ARGS+=("io_quick"); shift ;;
        --tasks)    D_ARGS+=("tasks"); shift ;;
        --calls)    D_ARGS+=("calls"); shift ;;
        --boot)     BOOT_MODE=1; shift ;;
        --no-build) BUILD=0; shift ;;
        --display)  DISPLAY_OPT=""; shift ;;
        --timeout)  TIMEOUT="$2"; shift; shift ;;
        *)          echo "Unknown option: $1"; exit 1 ;;
    esac
done

mkdir -p "$LOG_DIR"

# ── Step 1: Build ML ──────────────────────────────────────────────────────────
if [[ $BUILD -eq 1 ]]; then
    echo "═══ Building Magic Lantern for 70D... ═══"
    cd "$ML_DIR"
    make 70D_config 2>/dev/null || true
    make -j"$(nproc)" 2>&1 | tail -5

    AUTOEXEC="$ML_DIR/platform/70D.112/build/autoexec.bin"
    if [[ -f "$AUTOEXEC" ]]; then
        SIZE=$(stat --format="%s" "$AUTOEXEC" 2>/dev/null || stat -f "%z" "$AUTOEXEC" 2>/dev/null || echo "?")
        echo "  autoexec.bin: $(( SIZE / 1024 ))KB ($SIZE bytes)"
        if [[ $SIZE -gt 614400 ]]; then
            echo "  ⚠ WARNING: autoexec.bin exceeds 600KB!"
        fi
    else
        echo "  ⚠ Build failed — no autoexec.bin"
        exit 1
    fi
    echo ""
fi

# ── Step 2: Check ROM dumps ──────────────────────────────────────────────────
echo "═══ Checking ROM dumps... ═══"
ROM_OK=1
for f in ROM0.BIN ROM1.BIN SFDATA.BIN; do
    FPATH="$ROM_DIR/70D/$f"
    if [[ -f "$FPATH" ]]; then
        SIZE=$(stat --format="%s" "$FPATH" 2>/dev/null || stat -f "%z" "$FPATH" 2>/dev/null || echo "0")
        # Check if placeholder (all zeros)
        FIRST_BYTE=$(od -A n -t x1 -N 16 "$FPATH" | tr -d ' ')
        if [[ "$FIRST_BYTE" == "00000000000000000000000000000000" ]]; then
            echo "  $f: ${SIZE} bytes — ⚠ PLACEHOLDER (all zeros)"
            ROM_OK=0
        else
            echo "  $f: ${SIZE} bytes — ✓ Real ROM dump"
        fi
    else
        echo "  $f: MISSING"
        ROM_OK=0
    fi
done

if [[ $ROM_OK -eq 0 ]]; then
    echo ""
    echo "  ⚠ Real ROM dumps required for firmware boot."
    echo "  QEMU will launch but firmware won't execute (placeholder ROMs = NOPs)."
    echo "  See ROM-DUMP.md for instructions on dumping from a physical 70D."
    echo ""
fi

# ── Step 3: Prepare QEMU disk images ─────────────────────────────────────────
echo "═══ Preparing QEMU disk images... ═══"
DISK_DIR="$QEMU_DIR/magiclantern/disk_images"
SD_IMG="$DISK_DIR/sd.qcow2"
CF_IMG="$DISK_DIR/cf.qcow2"

if [[ ! -f "$SD_IMG" ]]; then
    XZ_IMG="$DISK_DIR/sd.qcow2.xz"
    if [[ -f "$XZ_IMG" ]]; then
        echo "  Extracting sd.qcow2.xz..."
        xz -d "$XZ_IMG" 2>/dev/null || true
    else
        echo "  Creating minimal sd.qcow2..."
        qemu-img create -f qcow2 "$SD_IMG" 1G 2>/dev/null
    fi
fi

if [[ ! -f "$CF_IMG" ]]; then
    echo "  Creating minimal cf.qcow2..."
    qemu-img create -f qcow2 "$CF_IMG" 128M 2>/dev/null || \
        dd if=/dev/zero of="$CF_IMG" bs=1 count=0 seek=128M 2>/dev/null
fi

# If --boot mode, copy ML build to SD image
if [[ $BOOT_MODE -eq 1 ]]; then
    echo "  Installing ML on SD card image..."
    BUILD_DIR="$ML_DIR/platform/70D.112/build"
    # This requires the SD image to have an existing partition
    # For now, just note that --boot needs 'make disk_image' first
    if [[ ! -f "$BUILD_DIR/sd.qcow2" ]]; then
        echo "  ⚠ No boot SD image found at $BUILD_DIR/sd.qcow2"
        echo "  Run 'cd $ML_DIR && make -C platform/70D.112 disk_image' first"
        BOOT_MODE=0
    else
        SD_IMG="$BUILD_DIR/sd.qcow2"
        CF_IMG="$BUILD_DIR/cf.qcow2"
    fi
fi

# ── Step 4: Build QEMU command ───────────────────────────────────────────────
echo "═══ Launching QEMU 70D... ═══"

QEMU_BIN="$QEMU_DIR/build/arm-softmmu/qemu-system-arm"
if [[ ! -x "$QEMU_BIN" ]]; then
    echo "  ⚠ QEMU binary not found at $QEMU_BIN"
    echo "  Run 'cd $QEMU_DIR && ./configure && make -j\$(nproc)' first"
    exit 1
fi

BOOT_FLAG=0
if [[ $BOOT_MODE -eq 1 ]]; then
    BOOT_FLAG=1
fi

CMD=("$QEMU_BIN")
CMD+=(-drive if=sd,file="$SD_IMG")
CMD+=(-drive if=ide,file="$CF_IMG")
CMD+=(-M "70D,firmware=boot=$BOOT_FLAG")
CMD+=(-name "70D")

if [[ -n "$DISPLAY_OPT" ]]; then
    CMD+=($DISPLAY_OPT)
fi

# Serial output to file + console via tee
SERIAL_LOG="$LOG_DIR/serial_70D_${TIMESTAMP}.log"
CMD+=(-serial "file:$SERIAL_LOG")

# DebugMsg output captured from stderr
DEBUGMSG_LOG="$LOG_DIR/debugmsg_70D_${TIMESTAMP}.log"

# GDB mode
if [[ $GDB_MODE -eq 1 ]]; then
    CMD+=(-S -gdb tcp::1234)
    if [[ $BOOT_TRACE -eq 1 ]]; then
        GDB_SCRIPT="$QEMU_DIR/magiclantern/cam_config/70D/boot.gdb"
        echo " GDB server on port 1234. Connect with:"
        echo " arm-none-eabi-gdb -x $GDB_SCRIPT"
    else
        GDB_SCRIPT="$QEMU_DIR/magiclantern/cam_config/70D/debugmsg.gdb"
        echo " GDB server on port 1234. Connect with:"
        echo " arm-none-eabi-gdb -x $GDB_SCRIPT"
    fi
    echo " Boot trace: $QEMU_DIR/magiclantern/cam_config/70D/boot.gdb"
    echo ""
fi

# -d debug flags
if [[ ${#D_ARGS[@]} -gt 0 ]]; then
    D_STR=$(IFS=,; echo "${D_ARGS[*]}")
    CMD+=(-d "$D_STR")
    echo "  Debug flags: $D_STR"
fi

# QEMU_EOS_WORKDIR for ROM path resolution
export QEMU_EOS_WORKDIR="$ROM_DIR"

echo "  Command: ${CMD[*]}"
echo "  Serial log: $SERIAL_LOG"
echo "  DebugMsg log: $DEBUGMSG_LOG"
echo "  Timeout: ${TIMEOUT}s"
echo ""

# ── Step 5: Run QEMU ─────────────────────────────────────────────────────────
echo "═══ QEMU Output ═══"
echo "──────────────────────────────────────────────────────"

set +e
timeout "$TIMEOUT" "${CMD[@]}" 2>"$DEBUGMSG_LOG"
EXIT_CODE=$?
set -e

echo "──────────────────────────────────────────────────────"
echo ""

# ── Step 6: Report results ───────────────────────────────────────────────────
echo "═══ Results ═══"
echo "  QEMU exit code: $EXIT_CODE"

if [[ -f "$SERIAL_LOG" ]]; then
    SERIAL_LINES=$(wc -l < "$SERIAL_LOG")
    echo "  Serial log: $SERIAL_LOG ($SERIAL_LINES lines)"
    if [[ $SERIAL_LINES -gt 0 ]]; then
        echo "  Last 10 lines:"
        tail -10 "$SERIAL_LOG" | sed 's/^/    /'
    fi
fi

if [[ -f "$DEBUGMSG_LOG" ]]; then
    DEBUGMSG_LINES=$(wc -l < "$DEBUGMSG_LOG")
    echo "  DebugMsg log: $DEBUGMSG_LOG ($DEBUGMSG_LINES lines)"
    if [[ $DEBUGMSG_LINES -gt 0 ]]; then
        echo "  Last 10 lines:"
        tail -10 "$DEBUGMSG_LOG" | sed 's/^/    /'
    fi
fi

# Quick analysis
echo ""
echo "═══ Quick Analysis ═══"
if [[ -f "$DEBUGMSG_LOG" ]]; then
    # Check for key boot events
    if grep -q "init_task\|Startup\|CreateTask" "$DEBUGMSG_LOG" 2>/dev/null; then
        echo "  ✓ Firmware boot detected (init_task/Startup found)"
    fi
    if grep -q "GUI\|menu\|display" "$DEBUGMSG_LOG" 2>/dev/null; then
        echo "  ✓ GUI initialization detected"
    fi
    if grep -q "mpu_send\|mpu_recv\|MPU" "$DEBUGMSG_LOG" 2>/dev/null; then
        echo "  ✓ MPU communication detected"
    fi
    if grep -q "assert\|ASSERT\|crash\|prefetch" "$DEBUGMSG_LOG" 2>/dev/null; then
        echo "  ⚠ Crash/assert detected!"
        grep "assert\|ASSERT\|crash\|prefetch" "$DEBUGMSG_LOG" | head -5 | sed 's/^/    /'
    fi
    if grep -q "placeholder" "$DEBUGMSG_LOG" 2>/dev/null; then
        echo "  ⚠ Placeholder ROM warning (expected without real dumps)"
    fi
fi

echo ""
echo "Logs saved to: $LOG_DIR/"
