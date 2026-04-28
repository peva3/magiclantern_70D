#!/usr/bin/env bash
# Quick test: Can QEMU boot and load modules?
set -euo pipefail

cd /app/70d

echo "=== Module Load Test ==="
echo "Starting QEMU (90 second timeout)..."
echo ""

# Start QEMU
./test_70d_qemu.sh --boot --no-build --timeout 90 2>&1 | tee /tmp/qemu_test.log &
QEMU_PID=$!

# Monitor boot progress
echo "Monitoring boot..."
for i in {1..36}; do
    sleep 2.5
    
    # Check if ML started
    if grep -q "created gui_sem in menu_init" /tmp/qemu_test.log 2>/dev/null; then
        echo "✓ ML GUI initialized"
        
        # Wait a bit more for module loading
        echo "Waiting for module loading (30 more seconds)..."
        sleep 30
        
        # Check for module loading
        if grep -qi "module\|LOAD" /tmp/qemu_test.log 2>/dev/null; then
            echo "✓ Module activity detected"
            grep -i "module\|LOAD" /tmp/qemu_test.log | tail -10
        else
            echo "ℹ No module activity in logs (may still be loading)"
        fi
        
        break
    fi
done

# Kill QEMU
kill $QEMU_PID 2>/dev/null || true
wait $QEMU_PID 2>/dev/null || true

echo ""
echo "=== Test Complete ==="
