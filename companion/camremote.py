#!/usr/bin/env python3
"""
camremote.py — Companion for 70D wifi_enable module

Connects to Canon 70D running wifi_enable.mo TCP server.
Default port: 5555. Camera IP must be known (check router DHCP).

Commands sent as single bytes:
  P -> PONG response
  S -> Camera status (battery, shutter count, temp, LV state)
  Q -> Close connection

Usage:
  ./camremote.py <camera_ip> [port]

Requires: Python 3.6+
"""

import sys
import socket
import struct

PORT = 5555
TIMEOUT = 5

def recv_resp(sock):
    """Read 2-byte length prefix + payload."""
    header = sock.recv(2)
    if len(header) < 2:
        return b''
    plen = struct.unpack('>H', header)[0]
    if plen == 0:
        return b''
    data = b''
    while len(data) < plen:
        chunk = sock.recv(plen - len(data))
        if not chunk:
            break
        data += chunk
    return data

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <camera_ip> [port]")
        sys.exit(1)

    ip = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else PORT

    print(f"[camremote] Connecting to {ip}:{port}...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(TIMEOUT)

    try:
        sock.connect((ip, port))
    except (socket.timeout, ConnectionRefusedError) as e:
        print(f"[camremote] FAILED: {e}")
        print("Check: 1) Camera WiFi is on and connected")
        print("       2) wifi_enable.mo is loaded (ML -> Modules)")
        print(f"       3) Camera IP is correct (check router)")
        sys.exit(1)

    print("[camremote] Connected!")

    try:
        while True:
            cmd = input("cmd> ").strip().upper()
            if not cmd:
                continue
            sock.send(cmd[0:1].encode())

            if cmd[0] in ('P', 'S', 'L', 'R', 'B'):
                resp = recv_resp(sock)
                if resp:
                    print(f"  Response: {resp.decode(errors='replace')}")
                else:
                    print("  (no response)")

            elif cmd[0] == 'Q':
                print("  Closing...")
                break

            else:
                print("  Commands: P=ping, S=status, L=LV toggle, R=remote release, Q=quit")
    except KeyboardInterrupt:
        print()
    finally:
        sock.close()
        print("[camremote] Disconnected")

if __name__ == '__main__':
    main()
