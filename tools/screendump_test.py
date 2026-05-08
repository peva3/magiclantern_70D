#!/usr/bin/env python3
"""Capture QEMU 70D display - verify ML menu renders"""
import subprocess, time, os

QEMU = "/app/70d/qemu-eos/build/arm-softmmu/qemu-system-arm"
ROM_DIR = "/app/70d/roms"
SD_IMG = "/app/70d/qemu-eos/sd_70D.qcow2"
OUTPUT = "/app/70d/logs/screendump_test.ppm"
MONITOR = "/tmp/qemu_monitor"

# Kill stale QEMU
os.system("pkill -9 -f qemu-system-arm 2>/dev/null; sleep 2")

env = os.environ.copy()
env["QEMU_EOS_WORKDIR"] = ROM_DIR

proc = subprocess.Popen(
    [QEMU, "-M", "70D", "-nographic",
     "-drive", f"if=sd,format=qcow2,file={SD_IMG}",
     "-serial", "file:/app/70d/logs/serial_screendump.log",
     "-monitor", f"pipe:{MONITOR}"],
    stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    env=env
)

with open(f"{MONITOR.in}", "w") as mon:
    time.sleep(5)  # Wait for boot
    mon.write("screendump /tmp/boot.ppm\n")
    time.sleep(2)
    mon.write("sendkey M\n")       # Press MENU
    time.sleep(2)
    mon.write("screendump /tmp/menu.ppm\n")
    time.sleep(1)
    mon.write("sendkey esc\n")     # Escape
    time.sleep(1)
    mon.write("sendkey L\n")       # Toggle LiveView
    time.sleep(3)
    mon.write("screendump /tmp/lv.ppm\n")
    time.sleep(1)
    mon.write("q\n")

proc.wait(timeout=30)

for label, path in [("boot", "/tmp/boot.ppm"), ("menu", "/tmp/menu.ppm"), ("lv", "/tmp/lv.ppm")]:
    if os.path.exists(path):
        size = os.path.getsize(path)
        print(f"{label}.ppm: {size} bytes")
    else:
        print(f"{label}.ppm: NOT FOUND")
