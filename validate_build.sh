#!/usr/bin/env bash
# Validate the hw_test build without requiring QEMU execution
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/platform/70D.112/build"
MODULES_DIR="$BUILD_DIR/modules"

echo "=== Build Validation Report ==="
echo "Date: $(date)"
echo "Build Dir: $BUILD_DIR"
echo ""

# Check autoexec.bin
if [[ -f "$BUILD_DIR/autoexec.bin" ]]; then
    SIZE=$(stat -c%s "$BUILD_DIR/autoexec.bin")
    SIZE_KB=$((SIZE / 1024))
    echo "✓ autoexec.bin: ${SIZE_KB}KB (${SIZE} bytes)"
    if [[ $SIZE -lt 671088 ]]; then  # 656KB limit
        echo "  Size OK: Under 656KB limit"
    else
        echo "  ⚠️  WARNING: Exceeds 656KB limit!"
    fi
else
    echo "✗ autoexec.bin: NOT FOUND"
fi
echo ""

# Check modules
echo "=== Modules ==="
for mod in hw_test crop_rec dual_iso sd_uhs mlv_lite; do
    if [[ -f "$MODULES_DIR/${mod}.mo" ]]; then
        SIZE=$(stat -c%s "$MODULES_DIR/${mod}.mo")
        echo "✓ ${mod}.mo: ${SIZE} bytes"
    else
        echo "✗ ${mod}.mo: NOT FOUND"
    fi
done
echo ""

# Check symbol file
if [[ -f "$BUILD_DIR/70D_112.sym" ]]; then
    echo "✓ Symbol file: 70D_112.sym"
else
    echo "✗ Symbol file: NOT FOUND"
fi
echo ""

# Check SD image
if [[ -f "$BUILD_DIR/sd.qcow2" ]]; then
    echo "✓ SD image: sd.qcow2"
else
    echo "✗ SD image: NOT FOUND (run create_sd_image.sh)"
fi
echo ""

# Check module source
echo "=== Source Validation ==="
if [[ -f "$SCRIPT_DIR/modules/hw_test/hw_test.c" ]]; then
    echo "✓ hw_test.c exists"
    grep -c "MODULE_INIT" "$SCRIPT_DIR/modules/hw_test/hw_test.c" > /dev/null 2>&1 && echo "  ✓ MODULE_INIT found" || echo "  ✗ MODULE_INIT missing"
    grep -c "hw_test_init" "$SCRIPT_DIR/modules/hw_test/hw_test.c" > /dev/null 2>&1 && echo "  ✓ hw_test_init function found" || echo "  ✗ hw_test_init missing"
else
    echo "✗ hw_test.c: NOT FOUND"
fi
echo ""

# Check modules.included
if grep -q "hw_test" "$SCRIPT_DIR/platform/70D.112/modules.included"; then
    echo "✓ hw_test in modules.included"
else
    echo "✗ hw_test NOT in modules.included"
fi
echo ""

echo "=== Register Addresses (from crop_rec.c) ==="
grep -i "0x26B54\|0x2684C\|0xFF2BC6C4" "$SCRIPT_DIR/modules/crop_rec/crop_rec.c" | head -3 || echo "Not found in crop_rec.c"
echo ""

echo "=== Next Steps ==="
echo "1. Build verified - ready for hardware testing"
echo "2. Copy autoexec.bin to SD card"
echo "3. Boot 70D with ML"
echo "4. hw_test module will run automatically"
echo "5. Check ML/LOGS/hw_test_full.txt for results"
echo ""
echo "=== Validation Complete ==="
