#!/usr/bin/env bash
# Extract hardware test logs from QEMU SD card image
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/platform/70D.112/build"
OUTPUT_DIR="$SCRIPT_DIR/logs/hw_test"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

mkdir -p "$OUTPUT_DIR"

echo "=== Extracting HW Test Logs ==="
echo "SD Image: $SD_IMG"
echo "Output: $OUTPUT_DIR"
echo ""

if [[ ! -f "$SD_IMG" ]]; then
    echo "ERROR: SD image not found at $SD_IMG"
    echo "Run: ./test_70d_qemu.sh --boot --no-build --timeout 40"
    exit 1
fi

# Convert to raw for mtools
RAW_IMG="$OUTPUT_DIR/sd_raw_$TIMESTAMP.img"
echo "Converting to raw format..."
qemu-img convert -f qcow2 -O raw "$SD_IMG" "$RAW_IMG"

# Find partition offset (typically sector 99)
OFFSET=$((99 * 512))

# Extract log files
echo "Extracting log files..."
mcopy -i "$RAW_IMG@+$OFFSET" ML/LOGS/hw_test_full.txt "$OUTPUT_DIR/hw_test_$TIMESTAMP.txt" 2>/dev/null && \
    echo "✓ Extracted: hw_test_$TIMESTAMP.txt" || \
    echo "✗ No hw_test_full.txt found (test may not have run)"

# Extract BMP screenshot if it exists
mcopy -i "$RAW_IMG@+$OFFSET" ML/LOGS/hw_test.bmp "$OUTPUT_DIR/hw_test_$TIMESTAMP.bmp" 2>/dev/null && \
    echo "✓ Extracted: hw_test_$TIMESTAMP.bmp" || \
    echo "ℹ No screenshot (BMP output not available in QEMU)"

# Show results
echo ""
echo "=== Results ==="
if [[ -f "$OUTPUT_DIR/hw_test_$TIMESTAMP.txt" ]]; then
    echo "Log file: $OUTPUT_DIR/hw_test_$TIMESTAMP.txt"
    echo ""
    echo "--- Content ---"
    cat "$OUTPUT_DIR/hw_test_$TIMESTAMP.txt"
    echo ""
    echo "--- End ---"
else
    echo "No log file found. Test may not have run yet."
    echo "Run: ./test_70d_qemu.sh --boot --no-build --timeout 40"
fi

# Cleanup raw image
rm -f "$RAW_IMG"

echo ""
echo "Output directory: $OUTPUT_DIR/"
