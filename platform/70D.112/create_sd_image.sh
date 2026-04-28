#!/usr/bin/env bash
#
# create_sd_image.sh — Create QEMU SD card image with Magic Lantern for 70D
#
# Usage: ./create_sd_image.sh [--no-clean]
#
# Creates sd.qcow2 and cf.qcow2 in the platform build directory,
# suitable for QEMU emulation with ML autoexec.bin and modules.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
ML_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
QEMU_DIR="$ML_DIR/qemu-eos"
BASE_SD="$QEMU_DIR/magiclantern/disk_images/sd.qcow2"
BASE_SD_XZ="$QEMU_DIR/magiclantern/disk_images/sd.qcow2.xz"
PARTITION_OFFSET=50688

NO_CLEAN=0
if [[ "${1:-}" == "--no-clean" ]]; then
    NO_CLEAN=1
fi

if [[ ! -f "$BUILD_DIR/autoexec.bin" ]]; then
    echo "ERROR: No autoexec.bin found. Run 'make' first."
    exit 1
fi

echo "=== Creating QEMU SD card image for 70D ==="

QEMU_IMG="$QEMU_DIR/build/qemu-img"
if [[ ! -x "$QEMU_IMG" ]]; then
    echo "ERROR: qemu-img not found at $QEMU_IMG"
    echo "Build QEMU first: cd $QEMU_DIR && ./configure && make"
    exit 1
fi

WORK_DIR=$(mktemp -d)
trap "rm -rf $WORK_DIR" EXIT

if [[ ! -f "$BASE_SD" ]]; then
    if [[ -f "$BASE_SD_XZ" ]]; then
        echo "Decompressing base SD image..."
        xz -dk "$BASE_SD_XZ" -c > "$BASE_SD"
    else
        echo "ERROR: No base SD image found at $BASE_SD or $BASE_SD_XZ"
        exit 1
    fi
fi

echo "Converting base image to raw..."
$QEMU_IMG convert -f qcow2 -O raw "$BASE_SD" "$WORK_DIR/sd.raw"

echo "Removing old ML files from image..."
mdel -i "$WORK_DIR/sd.raw@@$PARTITION_OFFSET" ::AUTOEXEC.BIN 2>/dev/null || true
mdel -i "$WORK_DIR/sd.raw@@$PARTITION_OFFSET" ::ML/README 2>/dev/null || true
mmd -i "$WORK_DIR/sd.raw@@$PARTITION_OFFSET" ::ML 2>/dev/null || true

echo "Installing ML build to SD image..."
mcopy -i "$WORK_DIR/sd.raw@@$PARTITION_OFFSET" "$BUILD_DIR/autoexec.bin" ::AUTOEXEC.BIN

# Copy from build/zip/ML (standard location) or build/ML (legacy)
ML_SRC=""
if [[ -d "$BUILD_DIR/zip/ML" ]]; then
    ML_SRC="$BUILD_DIR/zip/ML"
elif [[ -d "$BUILD_DIR/ML" ]]; then
    ML_SRC="$BUILD_DIR/ML"
fi

if [[ -n "$ML_SRC" ]]; then
    # FAT filesystems are case-insensitive, but QEMU's emulation may not be
    # Create a temporary copy with uppercase MODULES directory
    ML_TMP="$WORK_DIR/ML"
    cp -r "$ML_SRC" "$ML_TMP"
    if [[ -d "$ML_TMP/modules" ]]; then
        mv "$ML_TMP/modules" "$ML_TMP/MODULES"
    fi
    
    echo "Copying ML directory from $ML_TMP..."
    # Copy root ML files first
    mcopy -v -i "$WORK_DIR/sd.raw@@$PARTITION_OFFSET" "$ML_TMP"/* ::ML/
# Then copy MODULES directory explicitly
if [[ -d "$ML_TMP/MODULES" ]]; then
    echo "Copying MODULES directory..."
    mcopy -s -v -i "$WORK_DIR/sd.raw@@$PARTITION_OFFSET" "$ML_TMP/MODULES" ::ML/MODULES
fi

# Copy SETTINGS directory explicitly (for module .en files)
if [[ -d "$ML_TMP/SETTINGS" ]]; then
    echo "Copying SETTINGS directory..."
    mcopy -s -v -i "$WORK_DIR/sd.raw@@$PARTITION_OFFSET" "$ML_TMP/SETTINGS" ::ML/SETTINGS
fi
else
    echo "WARNING: No ML directory found, modules won't be available"
fi

if [[ -f "$BUILD_DIR/MAGIC.CFG" ]]; then
    mcopy -i "$WORK_DIR/sd.raw@@$PARTITION_OFFSET" "$BUILD_DIR/MAGIC.CFG" ::ML/SETTINGS/MAGIC.CFG 2>/dev/null || true
fi

echo "Verifying installed files..."
mdir -i "$WORK_DIR/sd.raw@@$PARTITION_OFFSET" ::

echo "Converting to qcow2..."
$QEMU_IMG convert -f raw -O qcow2 "$WORK_DIR/sd.raw" "$BUILD_DIR/sd.qcow2"
cp "$BUILD_DIR/sd.qcow2" "$BUILD_DIR/cf.qcow2"

SD_SIZE=$(stat --format="%s" "$BUILD_DIR/sd.qcow2" 2>/dev/null || stat -f "%z" "$BUILD_DIR/sd.qcow2")
AUTOEXEC_SIZE=$(stat --format="%s" "$BUILD_DIR/autoexec.bin" 2>/dev/null || stat -f "%z" "$BUILD_DIR/autoexec.bin")

echo ""
echo "=== Done ==="
echo "  sd.qcow2: $(( SD_SIZE / 1024 ))KB ($SD_SIZE bytes)"
echo "  cf.qcow2: $(( SD_SIZE / 1024 ))KB"
echo "  autoexec.bin: $(( AUTOEXEC_SIZE / 1024 ))KB"
echo ""
echo "  Run QEMU with: ./test_70d_qemu.sh --boot --no-build"
