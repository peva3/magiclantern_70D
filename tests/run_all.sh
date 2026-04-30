#!/bin/bash
# run_all.sh — Pre-deployment test suite for Magic Lantern 70D
# Run this before copying to SD card to catch issues early.
#
# Usage: ./tests/run_all.sh
#   (run from repo root, or from tests/ directory)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$REPO_DIR"

echo "============================================"
echo " Magic Lantern 70D — Pre-Deployment Tests"
echo "============================================"
echo ""

all_ok=1

# --- Test 1: Module symbol check ---
echo "--- [1/3] Module symbol check ---"
make -C "$SCRIPT_DIR" check-symbols 2>/dev/null || all_ok=0
echo ""

# --- Test 2: Python script syntax check ---
echo "--- [2/3] Companion script syntax check ---"
for script in camremote.py camtunnel.py; do
    if [ -f "$REPO_DIR/$script" ]; then
        python3 -m py_compile "$REPO_DIR/$script" 2>/dev/null
        if [ $? -eq 0 ]; then
            echo "  OK:   $script"
        else
            echo "  FAIL: $script (syntax error)"
            python3 -m py_compile "$REPO_DIR/$script"
            all_ok=0
        fi
    fi
done
echo ""

# --- Test 3: Build size check ---
echo "--- [3/3] Build size check ---"
if [ -f "$REPO_DIR/platform/70D.112/build/autoexec.bin" ]; then
    SIZE=$(stat -c%s "$REPO_DIR/platform/70D.112/build/autoexec.bin")
    SIZE_KB=$((SIZE / 1024))
    LIMIT=$((656 * 1024))
    echo "  autoexec.bin: ${SIZE_KB}KB (limit: 656KB)"
    if [ $SIZE -le $LIMIT ]; then
        echo "  PASS: Within limit (safety margin: $(((LIMIT - SIZE) / 1024))KB)"
    else
        echo "  FAIL: Exceeds 656KB limit!"
        all_ok=0
    fi
else
    echo "  SKIP: autoexec.bin not found (build first)"
fi
echo ""

# --- Summary ---
echo "============================================"
if [ "$all_ok" = "1" ]; then
    echo " All tests PASSED — ready for deployment"
    exit 0
else
    echo " Some tests FAILED — fix issues before deploying"
    exit 1
fi
echo "============================================"
