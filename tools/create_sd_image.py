#!/usr/bin/env python3
"""Create a bootable SD raw image for ML 70D QEMU testing."""
import os, struct, sys, subprocess, tempfile, shutil

CAMERA = sys.argv[1] if len(sys.argv) > 1 else "70D"
FW_VER = "112" if CAMERA == "70D" else "202"
BUILD_DIR = f"/app/70d/platform/{CAMERA}.{FW_VER}/build"
OUTPUT = f"/app/70d/qemu-eos/sd_{CAMERA}.raw"
SIZE_MB = 256
TMP_RAW = f"/tmp/sd_{CAMERA}_raw.img"

# Use pre-built SD image as template (firmware checks BPB fields like OEM='CANONEOS')
PREBUILT = f"/app/70d/qemu-eos/magiclantern/disk_images/sd.qcow2"
if not os.path.exists(PREBUILT):
    print(f"ERROR: pre-built SD image not found at {PREBUILT}")
    sys.exit(1)

# Extract the raw template from qcow2
subprocess.run(['qemu-img', 'convert', '-f', 'qcow2', '-O', 'raw', PREBUILT, TMP_RAW],
               check=True, capture_output=True)

# Mount via loopback
import subprocess
import time

# Use mcopy from mtools to copy files directly to the FAT image
env = os.environ.copy()
env['MTOOLS_SKIP_CHECK'] = '1'
env = {**env, 'MTOOLS_FAT_COMPAT': '1'}

def mcopy(src, dst):
    subprocess.run(['mcopy', '-i', TMP_RAW, src, f'::{dst}'], check=True, env=env)

def mmd(path):
    subprocess.run(['mmd', '-i', TMP_RAW, f'::{path}'], check=False, env=env)

# Copy autoexec.bin
mcopy(f"{BUILD_DIR}/autoexec.bin", "autoexec.bin")

# Create ML directory structure
for d in ["ML", "ML/MODULES", "ML/SETTINGS", "ML/LOGS", "ML/SCRIPTS", "ML/FONTS"]:
    mmd(d)

# Copy modules
import glob
for mo in glob.glob(f"{BUILD_DIR}/modules/*.mo"):
    mcopy(mo, f"ML/MODULES/{os.path.basename(mo)}")

# Copy fonts and other ML data
for item in glob.glob(f"{BUILD_DIR}/zip/ML/*"):
    name = os.path.basename(item)
    if os.path.isfile(item):
        mcopy(item, f"ML/{name}")

print(f"SD image created with {os.path.getsize(TMP_RAW)} bytes")

# Copy to raw output (no compression overhead)
shutil.copy2(TMP_RAW, OUTPUT)
os.remove(TMP_RAW)
print(f"Raw image: {OUTPUT} ({os.path.getsize(OUTPUT)} bytes)")
