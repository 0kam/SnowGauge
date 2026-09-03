#!/usr/bin/env python3
"""SMP bench client over the USB shell: echo, datetime get/set (UTC).

    python3 tools/smp_datetime.py -p /dev/cu.usbmodemXXXX            # echo + datetime get
    python3 tools/smp_datetime.py -p /dev/cu.usbmodemXXXX --set      # also set to now (UTC)

Needs `pip install smpclient`. Uses the mcumgr shell transport
(CONFIG_MCUMGR_TRANSPORT_SHELL), i.e. the same USB CDC port as the shell.
"""
import argparse
import asyncio
import glob
from datetime import datetime, timezone

from smpclient import SMPClient
from smpclient.transport.serial import SMPSerialTransport
from smpclient.requests.os_management import DateTimeRead, DateTimeWrite, EchoWrite


async def main(port, do_set):
    async with SMPClient(SMPSerialTransport(), port) as c:
        r = await c.request(EchoWrite(d="hi"))
        print("echo:", r.r)
        r = await c.request(DateTimeRead())
        print("datetime before:", getattr(r, "datetime", r))
        if do_set:
            now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S")
            r = await c.request(DateTimeWrite(datetime=now))
            print("datetime set", now, "->", r)
            r = await c.request(DateTimeRead())
            print("datetime after:", getattr(r, "datetime", r))


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("-p", "--port")
    ap.add_argument("--set", action="store_true")
    a = ap.parse_args()
    port = a.port or [p for p in glob.glob("/dev/cu.usbmodem*")][0]
    asyncio.run(main(port, a.set))
