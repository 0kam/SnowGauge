#!/usr/bin/env python3
"""Send shell commands to the SnowGauge firmware over USB CDC ACM.

    python3 tools/sgshell.py [-p /dev/cu.usbmodemXXXX] "rec count" "time" ...
    python3 tools/sgshell.py --file rec_202612.bin "rec dump 0"   # not a file transfer:
        just saves the raw shell output; use BLE (mcumgr fs) for the real files.

Needs pyserial. Strips ANSI escape codes and the prompt so the output can be
piped or diffed. A command's output is collected until the prompt reappears
or --timeout seconds pass.
"""
import argparse
import glob
import re
import sys
import time

import serial

ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]|\r")
PROMPT = "sg:~$ "


def find_port():
    ports = [p for p in glob.glob("/dev/cu.usbmodem*")]
    if len(ports) == 1:
        return ports[0]
    raise SystemExit(f"give -p PORT (candidates: {ports})")


def run(ser, cmd, timeout):
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode())
    buf = ""
    end = time.time() + timeout
    while time.time() < end:
        chunk = ser.read(4096).decode(errors="replace")
        if chunk:
            buf += chunk
            if ANSI.sub("", buf).rstrip().endswith(PROMPT.rstrip()) and cmd in buf:
                break
    text = ANSI.sub("", buf)
    lines = [l for l in text.split("\n") if l.strip() and not l.strip().startswith(PROMPT.rstrip())]
    # drop the echoed command
    if lines and lines[0].strip().endswith(cmd):
        lines = lines[1:]
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-p", "--port")
    ap.add_argument("-t", "--timeout", type=float, default=5.0)
    ap.add_argument("--file", help="also write the raw output of the last command here")
    ap.add_argument("commands", nargs="+")
    args = ap.parse_args()

    port = args.port or find_port()
    with serial.Serial(port, 115200, timeout=0.2) as ser:
        time.sleep(0.3)
        ser.write(b"\n")
        time.sleep(0.3)
        out = ""
        for cmd in args.commands:
            out = run(ser, cmd, args.timeout)
            print(f"### {cmd}")
            print(out)
        if args.file:
            with open(args.file, "w") as fh:
                fh.write(out + "\n")


if __name__ == "__main__":
    main()
