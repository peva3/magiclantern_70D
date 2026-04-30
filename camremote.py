#!/usr/bin/env python3
"""
camremote.py — Remote control companion for Canon 70D wifisrv module.

Connects to the camera's WiFi TCP server (default port 5555) and sends
single-byte commands. The camera must be on the same network with WiFi
enabled from Canon's menu.

Usage:
  python3 camremote.py <command> [options]

Commands:
  status            Get camera status (battery, shutter, temp, LV, remote flag)
  ping              Ping the camera (returns PONG)
  shutter           Trigger remote shutter release (call("FA_RemoteRelease"))
  lv                Toggle LiveView on/off
  bootdisk          Enable bootdisk mode
  screenshot        Save screenshot to ML/LOGS/WIFI_SCRN.BMP
  shell             Interactive shell mode (persistent connection, multiple cmds)

Options:
  --ip IP           Camera IP address (default: 192.168.1.20)
  --port PORT       Camera port (default: 5555)
  --repeat N        Repeat command N times with 5s delay

Examples:
  python3 camremote.py status
  python3 camremote.py shutter
  python3 camremote.py shell
"""

import socket
import struct
import sys
import time

DEFAULT_IP = "192.168.1.20"
DEFAULT_PORT = 5555

CMD_MAP = {
    "ping": "P",
    "status": "S",
    "shutter": "R",
    "lv": "L",
    "bootdisk": "B",
    "screenshot": "C",
    "quit": "Q",
}


def recv_resp(sock):
    """Read response from socket: 2-byte length prefix + payload."""
    raw_len = sock.recv(2)
    if len(raw_len) < 2:
        return None
    resp_len = struct.unpack("!H", raw_len)[0]
    resp = b""
    while len(resp) < resp_len:
        chunk = sock.recv(resp_len - len(resp))
        if not chunk:
            break
        resp += chunk
    return resp.decode("utf-8", errors="replace")


def send_one(ip, port, cmd_byte):
    """Open connection, send command, read response, close."""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect((ip, port))
        sock.send(bytes([ord(cmd_byte)]))
        resp = recv_resp(sock)
        sock.close()
        return resp
    except Exception as e:
        return f"ERROR: {e}"


def print_status(resp):
    if not resp:
        print("No response from camera")
        return
    if resp.startswith("STAT"):
        parts = {}
        for token in resp.split():
            if "=" in token:
                k, v = token.split("=", 1)
                parts[k] = v
        print(f"  Battery:      {parts.get('b', '?')}%")
        print(f"  Shutter cnt:  {parts.get('s', '?')}")
        print(f"  Temperature:  {parts.get('t', '?')}")
        print(f"  LiveView:     {'ON' if parts.get('lv') == '1' else 'OFF'}")
        print(f"  Remote shot:  {parts.get('rmt', '?')}")
        print(f"  Camera:       {parts.get('cam', '?')} fw {parts.get('fw', '?')}")
    else:
        print(f"  {resp}")


def interactive_shell(ip, port):
    """Interactive shell with persistent connection (one TCP session)."""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(30)
        sock.connect((ip, port))
    except Exception as e:
        print(f"Connection failed: {e}")
        return

    print(f"camremote shell — connected to {ip}:{port}")
    print("Commands: status, shutter, lv, ping, bootdisk, screenshot, quit")
    try:
        while True:
            cmd = input("> ").strip().lower()
            if cmd in ("quit", "exit", "q"):
                sock.send(b"Q")
                break
            if cmd in CMD_MAP:
                sock.send(bytes([ord(CMD_MAP[cmd])]))
                resp = recv_resp(sock)
                if cmd == "status":
                    print_status(resp)
                else:
                    print(f"  {resp}")
            elif cmd:
                print(f"  Unknown. Try: {'/'.join(CMD_MAP.keys())}")
    except (EOFError, KeyboardInterrupt):
        print()
    finally:
        sock.close()


def main():
    ip = DEFAULT_IP
    port = DEFAULT_PORT
    repeat = 1
    args = sys.argv[1:]

    cmds = []
    i = 0
    while i < len(args):
        if args[i] == "--ip" and i + 1 < len(args):
            ip = args[i + 1]
            i += 2
        elif args[i] == "--port" and i + 1 < len(args):
            port = int(args[i + 1])
            i += 2
        elif args[i] == "--repeat" and i + 1 < len(args):
            repeat = int(args[i + 1])
            i += 2
        else:
            cmds.append(args[i])
            i += 1

    if not cmds:
        print(__doc__)
        sys.exit(1)

    for cmd in cmds:
        if cmd == "shell":
            interactive_shell(ip, port)
        elif cmd in CMD_MAP:
            for r in range(repeat):
                if repeat > 1:
                    print(f"[{r+1}/{repeat}] ", end="")
                resp = send_one(ip, port, CMD_MAP[cmd])
                if cmd == "status":
                    print_status(resp)
                else:
                    print(resp)
                if r + 1 < repeat:
                    time.sleep(5)
        else:
            print(f"Unknown command: {cmd}")
            sys.exit(1)


if __name__ == "__main__":
    main()
