#!/bin/bash
# Deploy build to 70d-latest folder
# Usage: ./deploy.sh [build_dir]

BUILD_DIR="${1:-platform/70D.112/build}"
DEPLOY_DIR="70d-latest"

echo "=== Deploying to $DEPLOY_DIR ==="

# Copy build artifacts
cp $BUILD_DIR/autoexec.bin $DEPLOY_DIR/
cp $BUILD_DIR/magiclantern.bin $DEPLOY_DIR/
cp $BUILD_DIR/magiclantern.zip $DEPLOY_DIR/
cp $BUILD_DIR/70D_112.sym $DEPLOY_DIR/

# Extract ML package
unzip -o $DEPLOY_DIR/magiclantern.zip -d $DEPLOY_DIR/ > /dev/null 2>&1

echo "✓ Deployed:"
ls -lh $DEPLOY_DIR/autoexec.bin $DEPLOY_DIR/magiclantern.bin $DEPLOY_DIR/magiclantern.zip
echo "✓ Modules: $(ls $DEPLOY_DIR/ML/modules/*.mo | wc -l) modules"
echo "✓ PTP Tunnel module: $(ls -lh $DEPLOY_DIR/ML/modules/ptptun.mo 2>/dev/null | awk '{print $5}' || echo 'N/A')"
