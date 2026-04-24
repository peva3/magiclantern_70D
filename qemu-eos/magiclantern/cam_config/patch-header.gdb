# Shared GDB patch header - connects to QEMU target
# Used by camera-specific patches.gdb scripts
# Usage: ./run_canon_fw.sh <CAM> -s -S & arm-none-eabi-gdb -x <CAM>/patches.gdb -ex quit

set pagination off
set output-radix 16
set architecture arm
set tcp connect-timeout 300

if $_isvoid($TCP_PORT)
target remote localhost:1234
else
eval "target remote localhost:%d", $TCP_PORT
end
