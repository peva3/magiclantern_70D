#!/usr/bin/env bash
#
# run_swi_trace.sh — Trace DRYOS SWI dispatch on Canon 70D in QEMU
#
# Launches QEMU with GDB server, then runs swi_trace.gdb to:
#   1. Watch 0xFFFF0008 (SWI vector) for DRYOS handler installation
#   2. Set breakpoint on the installed handler
#   3. Decode and log all SWI calls (syscall numbers, backtraces)
#
# Usage:
#   ./run_swi_trace.sh                   # Default: 30s timeout
#   ./run_swi_trace.sh --timeout 60      # Custom timeout
#   ./run_swi_trace.sh --display         # Show QEMU display (GUI)
#   ./run_swi_trace.sh --boot            # Boot from ML autoexec.bin on SD
#
# Requirements:
#   - QEMU built at qemu-eos/build/
#   - ROM dumps at roms/70D/ (ROM0.BIN, ROM1.BIN, SFDATA.BIN)
#   - arm-none-eabi-gdb in PATH

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
QEMU_DIR="$ROOT_DIR/qemu-eos"
ROM_DIR="$ROOT_DIR/roms"
LOG_DIR="$ROOT_DIR/logs"
GDB_SCRIPT="$QEMU_DIR/magiclantern/cam_config/70D/swi_trace.gdb"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Defaults
TIMEOUT=30
DISPLAY_OPT="-display none"
BOOT_FLAG=0
BUILD_ML=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --timeout) TIMEOUT="$2"; shift; shift ;;
        --display) DISPLAY_OPT=""; shift ;;
        --boot) BOOT_FLAG=1; shift ;;
        --build) BUILD_ML=1; shift ;;
        *) echo "Usage: $0 [--timeout N] [--display] [--boot] [--build]"; exit 1 ;;
    esac
done

mkdir -p "$LOG_DIR"

# ── Check prerequisites ────────────────────────────────────────────────────

QEMU_BIN="$QEMU_DIR/build/arm-softmmu/qemu-system-arm"
if [[ ! -x "$QEMU_BIN" ]]; then
    echo "ERROR: QEMU binary not found at $QEMU_BIN"
    echo "  Build it: cd $QEMU_DIR && ./configure --target-list=arm-softmmu --disable-werror && make -j\$(nproc)"
    exit 1
fi

GDB=""
for g in arm-none-eabi-gdb gdb-multiarch; do
    if command -v "$g" &>/dev/null; then
        GDB="$g"
        break
    fi
done
if [[ -z "$GDB" ]]; then
    echo "ERROR: No ARM-capable GDB found in PATH"
    echo "  Install gdb-multiarch or arm-none-eabi-gdb."
    exit 1
fi

if [[ ! -f "$GDB_SCRIPT" ]]; then
    echo "ERROR: GDB script not found at $GDB_SCRIPT"
    exit 1
fi

# Check ROM dumps
ROM_OK=1
for f in ROM0.BIN ROM1.BIN SFDATA.BIN; do
    FPATH="$ROM_DIR/70D/$f"
    if [[ -f "$FPATH" ]]; then
        SIZE=$(stat --format="%s" "$FPATH" 2>/dev/null || stat -f "%z" "$FPATH" 2>/dev/null || echo "0")
        FIRST_BYTE=$(od -A n -t x1 -N 16 "$FPATH" | tr -d ' ')
        if [[ "$FIRST_BYTE" == "00000000000000000000000000000000" ]]; then
            echo "  $f: ${SIZE} bytes — ⚠ PLACEHOLDER (all zeros)"
            ROM_OK=0
        else
            echo "  $f: ${SIZE} bytes — ✓ Real ROM dump"
        fi
    else
        echo "  $f: MISSING at $FPATH"
        ROM_OK=0
    fi
done

if [[ $ROM_OK -eq 0 ]]; then
    echo ""
    echo "  ⚠ Real ROM dumps required for firmware boot."
    echo "  QEMU will launch but firmware won't execute correctly."
    echo ""
fi

# ── Build ML (optional) ────────────────────────────────────────────────────

if [[ $BUILD_ML -eq 1 ]]; then
    echo "═══ Building Magic Lantern (CONFIG_QEMU=y) ... ═══"
    make -C "$ROOT_DIR/platform/70D.112" -j"$(nproc)" CONFIG_QEMU=y 2>&1 | tail -5
    echo ""
fi

# ── Build QEMU command ─────────────────────────────────────────────────────

# SD card image
SD_IMG="$QEMU_DIR/magiclantern/disk_images/sd.qcow2"
if [[ ! -f "$SD_IMG" ]]; then
    if [[ -f "$SD_IMG.xz" ]]; then
        xz -d "$SD_IMG.xz" 2>/dev/null || true
    else
        qemu-img create -f qcow2 "$SD_IMG" 1G 2>/dev/null
    fi
fi

CF_IMG="$QEMU_DIR/magiclantern/disk_images/cf.qcow2"
if [[ ! -f "$CF_IMG" ]]; then
    qemu-img create -f qcow2 "$CF_IMG" 128M 2>/dev/null || true
fi

# Use ML boot SD if --boot
if [[ $BOOT_FLAG -eq 1 ]]; then
    ML_SD="$ROOT_DIR/platform/70D.112/build/sd.qcow2"
    if [[ -f "$ML_SD" ]]; then
        SD_IMG="$ML_SD"
        echo " Using ML boot SD image: $SD_IMG"
    else
        echo " ⚠ No ML SD image found at $ML_SD; using blank SD"
    fi
fi

CMD=("$QEMU_BIN")
CMD+=(-drive "if=sd,file=$SD_IMG")
CMD+=(-drive "if=ide,file=$CF_IMG")
CMD+=(-M "70D,firmware=boot=$BOOT_FLAG")
CMD+=(-name "70D")
CMD+=($DISPLAY_OPT)

SERIAL_LOG="$LOG_DIR/swi_trace_serial_${TIMESTAMP}.log"
CMD+=(-serial "file:$SERIAL_LOG")

# GDB server: halt at start (-S), listen on port 1234 (-gdb)
CMD+=(-S -gdb tcp::1234)

export QEMU_EOS_WORKDIR="$ROM_DIR"

# ── Launch QEMU in background ──────────────────────────────────────────────

echo "═══ Launching QEMU for 70D SWI trace ═══"
echo "  QEMU:      $QEMU_BIN"
echo "  ROM:       $ROM_DIR/70D/"
echo "  SD:        $SD_IMG"
echo "  GDB port:  1234"
echo "  GDB script: $GDB_SCRIPT"
echo "  Serial log: $SERIAL_LOG"
echo "  Timeout:   ${TIMEOUT}s"
echo "  Boot mode: $([ $BOOT_FLAG -eq 1 ] && echo 'ML autoexec' || echo 'Canon firmware only')"
echo ""

# Launch QEMU
"${CMD[@]}" 2>&1 &
QEMU_PID=$!

# ── Launch GDB ─────────────────────────────────────────────────────────────

echo "═══ Starting GDB SWI trace (${TIMEOUT}s timeout) ═══"
echo ""

# Run GDB with the SWI trace script from the cam_config directory
# so debug-logging.gdb (sourced via relative path) can be found
cd "$ROOT_DIR/qemu-eos/magiclantern/cam_config"
$GDB -batch-silent \
    -x "$GDB_SCRIPT" \
    -ex "set confirm off" \
    -ex "quit" \
    2>&1 | head -"$(( TIMEOUT * 10 ))" || true

# ── Cleanup ────────────────────────────────────────────────────────────────

if kill -0 "$QEMU_PID" 2>/dev/null; then
    echo ""
    echo "═══ Stopping QEMU (PID $QEMU_PID) ═══"
    kill "$QEMU_PID" 2>/dev/null || true
    wait "$QEMU_PID" 2>/dev/null || true
fi

echo ""
echo "═══ SWI Trace Complete ═══"
echo "  Serial log: $SERIAL_LOG"
if [[ -f "$SERIAL_LOG" ]]; then
    LINES=$(wc -l < "$SERIAL_LOG")
    echo "  Lines:     $LINES"
fi
echo ""

# Quick analysis: grep for key events in the serial log
if [[ -f "$SERIAL_LOG" ]] && [[ $LINES -gt 0 ]]; then
    echo "═══ Quick Analysis ═══"
    SWI_COUNT=$(grep -c '\[SWI' "$SERIAL_LOG" 2>/dev/null || echo "0")
    echo "  SWI calls logged: $SWI_COUNT"
    if grep -q 'SWI HANDLER INSTALLED' "$SERIAL_LOG" 2>/dev/null; then
        echo "  ✓ SWI handler detected and traced"
    else
        echo "  ⚠ No SWI handler installation detected"
    fi
    if grep -q 'init_task\|Startup\|CreateTask' "$SERIAL_LOG" 2>/dev/null; then
        echo "  ✓ Firmware boot detected"
    fi
    if grep -q 'ASSERT' "$SERIAL_LOG" 2>/dev/null; then
        echo "  ⚠ Assert failures detected"
        grep 'ASSERT' "$SERIAL_LOG" | head -3 | sed 's/^/    /'
    fi
    echo ""
fi

echo "Done."
