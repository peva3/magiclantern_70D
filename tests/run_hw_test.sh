#!/usr/bin/env bash
# Run hw_test module in QEMU and extract logs automatically
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$SCRIPT_DIR"

echo "=== Running HW Test in QEMU ==="
echo "Timeout: 120 seconds (waiting for full ML module init)"
echo ""

# Run QEMU with extended timeout (120s for full ML init + module loading)
./test_70d_qemu.sh --boot --no-build --timeout 120 2>&1 | tee logs/qemu_boot_$$.log &
QEMU_PID=$!

# Wait for QEMU to finish (or timeout)
echo "Waiting for QEMU to complete (120s)..."
for i in {1..24}; do
    sleep 5
    echo "  [$((i*5))s] Still waiting..."
done

# Kill QEMU if still running
kill $QEMU_PID 2>/dev/null || true
wait $QEMU_PID 2>/dev/null || true

echo ""
echo "=== QEMU Finished ==="
echo ""

# Extract logs
./extract_hw_logs.sh

echo ""
echo "=== Done ==="
