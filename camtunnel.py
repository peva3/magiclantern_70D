#!/usr/bin/env python3
"""
camtunnel - USB PTP Tunnel for Canon 70D Magic Lantern

Provides remote access to Magic Lantern over USB PTP.
Wraps PTP_CHDK (opcode 0x9999) and PTP_TUNNEL (opcode 0xA1E9) protocols.

Usage:
  ./camtunnel.py                      Interactive shell
  ./camtunnel.py --exec script.cam    Run script
  ./camtunnel.py call func_name       Call firmware function by name
  ./camtunnel.py propget <prop_id>    Get property
  ./camtunnel.py propset <prop_id> <val>  Set property
  ./camtunnel.py screenshot           Capture LCD
  ./camtunnel.py lua "print('hi')"    Execute Lua script
  ./camtunnel.py engio_read <addr>    Read ENGIO register
  ./camtunnel.py engio_write <addr> <val>  Write ENGIO register
  ./camtunnel.py shamem_read <addr>   Read shamem register
  ./camtunnel.py memread <addr> <len> Read camera memory
  ./camtunnel.py memwrite <addr> <data> Write camera memory (hex)
  ./camtunnel.py console              Capture DryOS debug log
  ./camtunnel.py upload <local> <remote>  Upload file
  ./camtunnel.py download <remote> [local]  Download file
"""

import struct
import sys
import os
import time
import argparse
import code
try:
    import usb.core
    import usb.util
    HAVE_PYUSB = True
except ImportError:
    HAVE_PYUSB = False
    print("Warning: pyusb not installed. Install with: pip install pyusb", file=sys.stderr)

CANON_VID = 0x04A9
PTP_OC_CHDK = 0x9999
PTP_TUNNEL_CODE = 0xA1E9
PTP_RC_OK = 0x2001

PTP_CHDK_Version = 0
PTP_CHDK_GetMemory = 1
PTP_CHDK_SetMemory = 2
PTP_CHDK_CallFunction = 3
PTP_CHDK_TempData = 4
PTP_CHDK_UploadFile = 5
PTP_CHDK_DownloadFile = 6
PTP_CHDK_ExecuteScript = 7

PTP_TUNNEL_CallByName = 0
PTP_TUNNEL_ConsoleCapture = 1
PTP_TUNNEL_Screenshot = 2
PTP_TUNNEL_ExecuteLua = 3
PTP_TUNNEL_EngioRead = 4
PTP_TUNNEL_EngioWrite = 5
PTP_TUNNEL_ShamemRead = 6
PTP_TUNNEL_SetPropertyRaw = 7

BUF_SIZE = 128 * 1024

class PTPError(Exception):
    pass

class PTPSession:
    def __init__(self, dev=None):
        self.dev = dev
        self.ep_in = None
        self.ep_out = None
        self.intf = None
        self.transaction = 1
        self.timeout = 5000

    def find_camera(self):
        if not HAVE_PYUSB:
            raise PTPError("pyusb not installed")
        dev = usb.core.find(idVendor=CANON_VID)
        if dev is None:
            raise PTPError("No Canon camera found (VID 0x04A9)")
        self.dev = dev

    def connect(self):
        if self.dev is None:
            self.find_camera()
        if self.dev.is_kernel_driver_active(0):
            try:
                self.dev.detach_kernel_driver(0)
            except usb.core.USBError:
                pass
        cfg = self.dev.get_active_configuration()
        for intf in cfg:
            if intf.bInterfaceClass == 6:
                self.intf = intf
                break
        if self.intf is None:
            for intf in cfg:
                self.intf = intf
                break
        usb.util.claim_interface(self.dev, self.intf.bInterfaceNumber)
        for ep in self.intf:
            if ep.bEndpointAddress & 0x80:
                self.ep_in = ep
            else:
                self.ep_out = ep

    def send_operation(self, opcode, *params):
        tid = self.transaction
        self.transaction += 1
        n = len(params)
        data = struct.pack('<HHII', 12 + n * 4, 1, opcode, tid)
        for p in params:
            data += struct.pack('<I', p)
        data += struct.pack('<I', 0) * (5 - n)
        self.dev.write(self.ep_out, data, self.timeout)

    def send_data(self, data_block):
        self.dev.write(self.ep_out, data_block, self.timeout)

    def recv_data(self, size=None):
        if size is None:
            header = self.dev.read(self.ep_in, 512, self.timeout)
            plen = struct.unpack_from('<I', header, 0)[0]
            if len(header) < plen:
                rest = self.dev.read(self.ep_in, plen - len(header), self.timeout)
                header = bytes(header) + bytes(rest)
            return header
        data = b''
        while len(data) < size:
            chunk = self.dev.read(self.ep_in, min(BUF_SIZE, size - len(data)), self.timeout)
            data += bytes(chunk)
        return data

    def recv_response(self):
        resp = self.dev.read(self.ep_in, 512, self.timeout)
        plen, rtype, cmd, tid, rc = struct.unpack_from('<HHI<I', bytes(resp), 0)
        nparams = (plen - 12) // 4
        params = []
        off = 12
        for i in range(nparams):
            params.append(struct.unpack_from('<I', bytes(resp), off)[0])
            off += 4
        return rc, params

    def wait_for_interrupt(self):
        try:
            return bytes(self.dev.read(self.ep_in, 64, self.timeout))
        except usb.core.USBError:
            return b''

    def chdk_call(self, func_addr, *args):
        data = struct.pack('<I', func_addr)
        for a in args:
            data += struct.pack('<I', a)
        return self.chdk_command(PTP_CHDK_CallFunction, data=data)

    def chdk_command(self, subcmd, param2=0, param3=0, data=None):
        self.send_operation(PTP_OC_CHDK, subcmd, param2, param3)
        if data is not None:
            self.send_data(data)
        time.sleep(0.1)
        rc, params = self.recv_response()
        if rc != PTP_RC_OK:
            raise PTPError(f"CHDK command {subcmd} failed: rc=0x{rc:04X}")
        return params

    def chdk_get_memory(self, addr, size):
        self.send_operation(PTP_OC_CHDK, PTP_CHDK_GetMemory, addr, size)
        data = self.recv_data()
        time.sleep(0.1)
        rc, params = self.recv_response()
        if rc != PTP_RC_OK:
            raise PTPError(f"GetMemory failed: rc=0x{rc:04X}")
        return data

    def chdk_set_memory(self, addr, data_block):
        self.send_operation(PTP_OC_CHDK, PTP_CHDK_SetMemory, addr, len(data_block))
        self.send_data(data_block)
        time.sleep(0.1)
        rc, params = self.recv_response()
        if rc != PTP_RC_OK:
            raise PTPError(f"SetMemory failed: rc=0x{rc:04X}")

    def chdk_upload_file(self, local_path, remote_name):
        with open(local_path, 'rb') as f:
            file_data = f.read()
        fn_bytes = remote_name.encode('utf-8')
        data = struct.pack('<I', len(fn_bytes)) + fn_bytes + file_data
        self.send_operation(PTP_OC_CHDK, PTP_CHDK_UploadFile)
        self.send_data(data)
        time.sleep(0.1)
        rc, params = self.recv_response()
        if rc != PTP_RC_OK:
            raise PTPError(f"UploadFile failed: rc=0x{rc:04X}")
        return params

    def chdk_download_file(self, remote_name):
        fn_bytes = remote_name.encode('utf-8')
        self.send_operation(PTP_OC_CHDK, PTP_CHDK_TempData, 0, len(fn_bytes))
        self.send_data(fn_bytes)
        time.sleep(0.05)
        rc, params = self.recv_response()
        if rc != PTP_RC_OK:
            raise PTPError(f"TempData failed: rc=0x{rc:04X}")
        self.send_operation(PTP_OC_CHDK, PTP_CHDK_DownloadFile)
        data = self.recv_data()
        time.sleep(0.1)
        rc, params = self.recv_response()
        if rc != PTP_RC_OK:
            raise PTPError(f"DownloadFile failed: rc=0x{rc:04X}")

        hdr_len = struct.unpack_from('<I', data, 0)[0]
        file_len = params[0] if params else len(data)
        start = 12
        return data[start:start + file_len]

    def tunnel_command(self, subcmd, param2=0, param3=0, data=None):
        self.send_operation(PTP_TUNNEL_CODE, subcmd, param2, param3)
        if data is not None:
            self.send_data(data)
        time.sleep(0.1)
        rc, params = self.recv_response()
        if rc != PTP_RC_OK:
            raise PTPError(f"Tunnel command {subcmd} failed: rc=0x{rc:04X}")
        if params and params[0] == 0xFFFFFFFF:
            raise PTPError(f"Tunnel command {subcmd} returned error")
        return params

    def tunnel_call_byname(self, func_name, *args):
        data = func_name.encode('utf-8') + b'\x00'
        for a in args:
            data += struct.pack('<I', a)
        params = self.tunnel_command(PTP_TUNNEL_CallByName, param2=len(args) & 0xFF, data=data)
        return params[0] if params else 0

    def tunnel_console_capture(self):
        params = self.tunnel_command(PTP_TUNNEL_ConsoleCapture, param2=0)
        data = self.recv_data()
        return data

    def tunnel_screenshot(self):
        params = self.tunnel_command(PTP_TUNNEL_Screenshot)
        data = self.recv_data()
        return data

    def tunnel_execute_lua(self, script):
        data = script.encode('utf-8')
        params = self.tunnel_command(PTP_TUNNEL_ExecuteLua, data=data)
        return params

    def tunnel_engio_read(self, addr):
        params = self.tunnel_command(PTP_TUNNEL_EngioRead, param2=addr)
        return params[0] if params else 0

    def tunnel_engio_write(self, addr, val):
        params = self.tunnel_command(PTP_TUNNEL_EngioWrite, param2=addr, param3=val)
        return params

    def tunnel_shamem_read(self, addr):
        params = self.tunnel_command(PTP_TUNNEL_ShamemRead, param2=addr)
        return params[0] if params else 0

    def tunnel_set_property(self, prop_id, data_block):
        params = self.tunnel_command(PTP_TUNNEL_SetPropertyRaw, param2=prop_id, data=data_block)
        return params

def print_hex(data, label=""):
    if label:
        print(f"{label}: {len(data)} bytes")
    for i in range(0, min(len(data), 64), 16):
        hex_str = ' '.join(f'{b:02x}' for b in data[i:i+16])
        ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in data[i:i+16])
        print(f"  {i:04x}: {hex_str:<48} {ascii_str}")

def interactive_shell(sesh):
    banner = """
=== camtunnel - Canon 70D PTP Tunnel ===
Commands: call, memread, memwrite, engio_read, engio_write, shamem_read,
           screenshot, console, lua, propget, propset, upload, download, help

Type any command with args, or 'exit' to quit.

Shortcuts:
  c = sesh.tunnel_call_byname
  m = sesh.chdk_get_memory
  er = sesh.tunnel_engio_read
  sr = sesh.tunnel_shamem_read
  ss = sesh.tunnel_screenshot
"""
    ns = {'sesh': sesh, 'c': sesh.tunnel_call_byname, 'm': sesh.chdk_get_memory,
          'er': sesh.tunnel_engio_read, 'sr': sesh.tunnel_shamem_read,
          'ss': sesh.tunnel_screenshot, 'p': print, 'ph': print_hex, 'PTPError': PTPError}
    code.interact(banner=banner, local=ns)

def main():
    parser = argparse.ArgumentParser(description='camtunnel - USB PTP Tunnel for Canon 70D')
    parser.add_argument('cmd', nargs='*', help='Command and arguments')
    parser.add_argument('--exec', '-e', metavar='FILE', help='Execute script file')
    parser.add_argument('--list', action='store_true', help='List connected cameras')
    args = parser.parse_args()

    if args.list:
        if not HAVE_PYUSB:
            print("pyusb not installed")
            return 1
        devs = usb.core.find(idVendor=CANON_VID, find_all=True)
        found = False
        for dev in devs:
            try:
                desc = f"Canon camera (bus {dev.bus}, addr {dev.address})"
                print(desc)
                found = True
            except usb.core.USBError:
                pass
        if not found:
            print("No Canon cameras found")
        return 0

    sesh = PTPSession()
    try:
        sesh.connect()
    except PTPError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    print(f"Connected: Canon camera (bus {sesh.dev.bus}, addr {sesh.dev.address})")

    if args.exec:
        with open(args.exec) as f:
            script = f.read()
        exec(compile(script, args.exec, 'exec'), {'sesh': sesh, 'c': sesh.tunnel_call_byname,
            'er': sesh.tunnel_engio_read, 'sr': sesh.tunnel_shamem_read,
            'ss': sesh.tunnel_screenshot, 'ph': print_hex})
        return 0

    if not args.cmd:
        interactive_shell(sesh)
        return 0

    cmd = args.cmd[0]
    rest = args.cmd[1:]

    try:
        if cmd == 'call':
            name = rest[0]
            int_args = [int(a, 0) for a in rest[1:]]
            result = sesh.tunnel_call_byname(name, *int_args)
            print(f"call('{name}') = 0x{result:08X} ({result})")

        elif cmd == 'memread':
            addr = int(rest[0], 0)
            size = int(rest[1], 0) if len(rest) > 1 else 4
            data = sesh.chdk_get_memory(addr, size)
            print_hex(data, f"Memory at 0x{addr:08X}")

        elif cmd == 'memwrite':
            addr = int(rest[0], 0)
            val = int(rest[1], 0)
            data = struct.pack('<I', val)
            sesh.chdk_set_memory(addr, data)
            print(f"Wrote 0x{val:08X} to 0x{addr:08X}")

        elif cmd == 'engio_read':
            addr = int(rest[0], 0)
            val = sesh.tunnel_engio_read(addr)
            print(f"engio_read(0x{addr:08X}) = 0x{val:08X}")

        elif cmd == 'engio_write':
            addr = int(rest[0], 0)
            val = int(rest[1], 0)
            sesh.tunnel_engio_write(addr, val)
            print(f"engio_write(0x{addr:08X}, 0x{val:08X})")

        elif cmd == 'shamem_read':
            addr = int(rest[0], 0)
            val = sesh.tunnel_shamem_read(addr)
            print(f"shamem_read(0x{addr:08X}) = 0x{val:08X}")

        elif cmd == 'screenshot':
            data = sesh.tunnel_screenshot()
            fname = rest[0] if rest else f"screenshot_{int(time.time())}.bmp"
            with open(fname, 'wb') as f:
                f.write(data)
            print(f"Screenshot saved: {fname} ({len(data)} bytes)")

        elif cmd == 'console':
            data = sesh.tunnel_console_capture()
            print(data.decode('utf-8', errors='replace'))

        elif cmd == 'lua':
            script = ' '.join(rest)
            if not script and not sys.stdin.isatty():
                script = sys.stdin.read()
            sesh.tunnel_execute_lua(script)
            print(f"Lua script saved to ML/SCRIPTS/PTP_RUN.LUA ({len(script)} bytes)")

        elif cmd == 'propget':
            prop = int(rest[0], 0)
            sesh.tunnel_command(PTP_TUNNEL_ShamemRead, param2=prop)
            print(f"prop_get(0x{prop:08X}) - see console output")

        elif cmd == 'propset':
            prop = int(rest[0], 0)
            val = int(rest[1], 0)
            sesh.tunnel_set_property(prop, struct.pack('<I', val))
            print(f"prop_set(0x{prop:08X}) = 0x{val:08X}")

        elif cmd == 'upload':
            local = rest[0]
            remote = rest[1] if len(rest) > 1 else os.path.basename(local)
            sesh.chdk_upload_file(local, remote)
            print(f"Uploaded: {local} -> {remote}")

        elif cmd == 'download':
            remote = rest[0]
            local = rest[1] if len(rest) > 1 else os.path.basename(remote)
            data = sesh.chdk_download_file(remote)
            with open(local, 'wb') as f:
                f.write(data)
            print(f"Downloaded: {remote} -> {local} ({len(data)} bytes)")

        elif cmd == 'version':
            params = sesh.chdk_command(PTP_CHDK_Version)
            print(f"CHDK PTP version: {params[0]}.{params[1]}")

        else:
            print(f"Unknown command: {cmd}")
            print("Commands: call, memread, memwrite, engio_read, engio_write,")
            print("  shamem_read, screenshot, console, lua, propget, propset,")
            print("  upload, download, version")
            return 1

    except PTPError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    return 0

if __name__ == '__main__':
    sys.exit(main())
