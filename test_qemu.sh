#!/usr/bin/env bash
#
# test_qemu.sh — Test any camera model in QEMU: build ML, launch, capture boot log
#
# Usage:
#   ./test_qemu.sh 70D               # Quick test 70D (10s)
#   ./test_qemu.sh EOSM              # Quick test EOS M (10s)
#   ./test_qemu.sh 70D --boot        # Boot from ML on SD card
#   ./test_qemu.sh EOSM --full       # Full boot attempt (60s)
#   ./test_qemu.sh 70D --gdb         # With GDB server
#   ./test_qemu.sh EOSM --debugmsg   # With DebugMsg logging
#
# ROM dumps must be in roms/<CAMERA>/ (ROM0.BIN, ROM1.BIN, SFDATA.BIN)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QEMU_DIR="$SCRIPT_DIR/qemu-eos"
ROM_DIR="$SCRIPT_DIR/roms"
LOG_DIR="$SCRIPT_DIR/logs"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

CAMERA=""
TIMEOUT=10
GDB_MODE=0
BOOT_TRACE=0
BOOT_MODE=0
NO_BUILD=0
D_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        70D|EOSM|EOSM2|100D|200D|5D3|6D|700D|650D|600D|60D|80D|7D|7D2)
            CAMERA="$1"; shift ;;
        --gdb) GDB_MODE=1; TIMEOUT=120; shift ;;
        --boot-trace) GDB_MODE=1; BOOT_TRACE=1; TIMEOUT=120; shift ;;
        --full) TIMEOUT=60; shift ;;
        --mpu) D_ARGS+=("mpu"); shift ;;
        --debugmsg) D_ARGS+=("debugmsg"); shift ;;
        --io) D_ARGS+=("io_quick"); shift ;;
        --tasks) D_ARGS+=("tasks"); shift ;;
        --calls) D_ARGS+=("calls"); shift ;;
         --boot) BOOT_MODE=1; TIMEOUT=180; shift ;;
         --no-build) NO_BUILD=1; shift ;;
         --timeout) TIMEOUT="$2"; shift; shift ;;
        *) echo "Usage: $0 <CAMERA> [options]"; echo "Cameras: 70D, EOSM, EOSM2, 100D, 200D, 5D3, 6D, 700D, ..."; exit 1 ;;
    esac
done

if [ -z "$CAMERA" ]; then
    echo "Usage: $0 <CAMERA> [options]"
    echo "Cameras: 70D, EOSM, EOSM2, 100D, 200D, 5D3, 6D, 700D, 650D, 600D, 60D, 80D, 7D, 7D2"
    exit 1
fi

mkdir -p "$LOG_DIR"

CAM_ROM_DIR="$ROM_DIR/$CAMERA"
SERIAL_LOG="$LOG_DIR/serial_${CAMERA}_${TIMESTAMP}.log"
QEMU_BIN="$QEMU_DIR/build/arm-softmmu/qemu-system-arm"

# ── check ROMs ────────────────────────────────────────────────────────────────
if [ ! -f "$CAM_ROM_DIR/ROM1.BIN" ]; then
    echo "WARNING: No ROMs found for $CAMERA at $CAM_ROM_DIR"
    echo "Creating placeholder ROMs for basic model testing..."
    mkdir -p "$CAM_ROM_DIR"
    python3 -c "
import struct
data = bytearray(16 * 1024 * 1024)
data[0:4] = struct.pack('<I', 0xE12FFF1E)  # BX LR
data[0x100:0x110] = b'DRYOS 2.3 r51'
data[0x200:0x210] = b'${CAMERA}_PLACEHOLDER'
with open('${CAM_ROM_DIR}/ROM1.BIN', 'wb') as f: f.write(data)
data0 = bytearray(8 * 1024 * 1024)
data0[0:8] = b'${CAMERA}_ASST'
with open('${CAM_ROM_DIR}/ROM0.BIN', 'wb') as f: f.write(data0)
_sf = bytearray(16 * 1024 * 1024)
_sf[0:10] = b'${CAMERA}_SFD'
with open('${CAM_ROM_DIR}/SFDATA.BIN', 'wb') as f: f.write(_sf)
print('Placeholder ROMs created')
"
fi

# ── build debug args ──────────────────────────────────────────────────────────
DEBUG_OPTS=""
for d in "${D_ARGS[@]}"; do
    [ -n "$DEBUG_OPTS" ] && DEBUG_OPTS="$DEBUG_OPTS,"
    DEBUG_OPTS="${DEBUG_OPTS}${d}"
done
[ -n "$DEBUG_OPTS" ] && DEBUG_OPTS="-d $DEBUG_OPTS"

# ── QEMU command ──────────────────────────────────────────────────────────────
QEMU_CMD=("$QEMU_BIN" "-M" "$CAMERA" "-nographic" "$DEBUG_OPTS"
    "-drive" "if=sd,format=qcow2,file=$QEMU_DIR/sd.qcow2,cache=unsafe")

# ── QEMU_EOS_WORKDIR for ROM path resolution ────────────────────────────────────
export QEMU_EOS_WORKDIR="$ROM_DIR"

# ── QEMU_EOS_DEBUGMSG for DebugMsg logging ─────────────────────────────────────
if [[ " ${D_ARGS[*]} " =~ " debugmsg " ]]; then
    case "$CAMERA" in
        70D)  export QEMU_EOS_DEBUGMSG="0xFFA50C3C" ;;
        EOSM) export QEMU_EOS_DEBUGMSG="0xFF1BE4AC" ;;
        *)    echo "DebugMsg address not known for $CAMERA" ;;
    esac
fi

if [ "$BOOT_MODE" -eq 1 ]; then
    case "$CAMERA" in
        70D)  FW_VER="112" ;;
        EOSM) FW_VER="202" ;;
        100D) FW_VER="101" ;;
        200D) FW_VER="101" ;;
        5D3)  FW_VER="123" ;;
        6D)   FW_VER="116" ;;
        700D) FW_VER="115" ;;
        650D) FW_VER="104" ;;
        600D) FW_VER="102" ;;
        *)    FW_VER="202" ;;
    esac
    ML_BUILD_DIR="$SCRIPT_DIR/platform/${CAMERA}.${FW_VER}/build"
    if [ -f "$ML_BUILD_DIR/autoexec.bin" ]; then
        echo "Booting $CAMERA from ML build..."
        SD_IMG="$QEMU_DIR/sd_${CAMERA}.qcow2"
        CF_IMG="$QEMU_DIR/cf_${CAMERA}.raw"
        if [ ! -f "$SD_IMG" ]; then
            # Use raw format for speed (no COW overhead)
            qemu-img create -f raw "$SD_IMG" 256M 2>/dev/null
            python3 "$SCRIPT_DIR/tools/format_sd_fat32.py" "$SD_IMG" 2>/dev/null
        fi
        if [ ! -f "$CF_IMG" ] && grep -q "cf_driver_interrupt" "$QEMU_DIR/hw/eos/model_list.c" 2>/dev/null; then
            qemu-img create -f raw "$CF_IMG" 64M 2>/dev/null
        fi
        QEMU_CMD=("$QEMU_BIN" "-M" "$CAMERA" "-nographic"
            "-drive" "if=sd,format=qcow2,file=$SD_IMG,cache=unsafe"
            "-serial" "file:$SERIAL_LOG"
            "$DEBUG_OPTS"
            "-display" "none")
    else
        echo "No ML build for $CAMERA. Run without --boot to test model only."
        exit 1
    fi
fi

echo "============================================"
echo " QEMU $CAMERA Test | Timeout: ${TIMEOUT}s"
echo " Log: $SERIAL_LOG"
echo "============================================"

# ── GDB mode ─────────────────────────────────────────────────────────────────
if [ "$GDB_MODE" -eq 1 ]; then
    CMD_GDB="$QEMU_DIR/magiclantern/cam_config/$CAMERA"
    if [ "$BOOT_TRACE" -eq 1 ] && [ -f "$CMD_GDB/boot.gdb" ]; then
        GDB_SCRIPT="$CMD_GDB/boot.gdb"
    elif [ -f "$CMD_GDB/debugmsg.gdb" ]; then
        GDB_SCRIPT="$CMD_GDB/debugmsg.gdb"
    elif [ -f "$CMD_GDB/patches.gdb" ]; then
        GDB_SCRIPT="$CMD_GDB/patches.gdb"
    else
        echo "No GDB scripts for $CAMERA at $CMD_GDB"
        exit 1
    fi
    echo "GDB script: $GDB_SCRIPT"
    "${QEMU_CMD[@]}" -s -S &
    QEMU_PID=$!
    sleep 1
    arm-none-eabi-gdb -batch -x "$GDB_SCRIPT" \
        -ex "target remote :1234" \
        -ex "echo --- done ---\n" 2>&1 | tee "$SERIAL_LOG" || true
    kill $QEMU_PID 2>/dev/null || true
    wait $QEMU_PID 2>/dev/null || true
else
    timeout "$TIMEOUT" "${QEMU_CMD[@]}" 2>&1 | tee "$SERIAL_LOG" || true
fi

echo ""
echo "=== Analysis ==="
if grep -q "startupInitializeComplete\|K325 READY\|ML started\|ml_started" "$SERIAL_LOG" 2>/dev/null; then
    echo "  Firmware booted successfully"
elif grep -q "PLACEHOLDER\|E12FFF1E" "$SERIAL_LOG" 2>/dev/null; then
    echo "  Placeholder ROMs (no real firmware)"
elif [ -s "$SERIAL_LOG" ]; then
    echo "  Model loaded (check log for details)"
else
    echo "  No output captured"
fi
