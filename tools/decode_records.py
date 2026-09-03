#!/usr/bin/env python3
"""Decode SnowGauge record files (rec_YYYYMM.bin, 40-byte records v1) to CSV.

Usage:
    python3 tools/decode_records.py rec_202612.bin [more.bin ...] > records.csv
    python3 tools/decode_records.py --hex "4753 01 ..."   # one record pasted from the shell

Layout: see firmware/src/record.h. Records are re-synchronised on the magic
word, so a torn record at the end of a file is reported and skipped.
"""
import argparse
import struct
import sys
from datetime import datetime, timezone

RECORD_SIZE = 40
MAGIC = 0x5347
VERSION = 1
FMT = "<HBBIIHHHHHHhhhhhHHH"  # 40 bytes
assert struct.calcsize(FMT) == RECORD_SIZE

FLAGS = {
    0: "time_synced",
    1: "time_estimated",
    2: "lidar_ok",
    3: "tilt_ok",
    4: "manual",
    5: "first_after_boot",
}

HEADER = [
    "seq", "epoch", "time_utc", "flags", "dist_cm", "var_cm2", "strength",
    "n_frames", "n_valid", "n_oor", "tilt_deg", "pitch_deg", "roll_deg",
    "imu_temp_c", "lidar_temp_c", "vbat_start_mv", "vbat_end_mv",
]


def crc16_ccitt(data: bytes, seed: int = 0xFFFF) -> int:
    """Zephyr crc16_ccitt(): reflected CCITT (poly 0x8408 = 0x1021 reversed), init 0xFFFF."""
    crc = seed
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0x8408 if crc & 1 else crc >> 1
    return crc


def decode(buf: bytes):
    """Return a dict for a valid record, or None."""
    if len(buf) != RECORD_SIZE:
        return None
    f = struct.unpack(FMT, buf)
    magic, version, flags, epoch, seq = f[0:5]
    if magic != MAGIC or version != VERSION:
        return None
    if f[-1] != crc16_ccitt(buf[:-2]):
        return None
    (dist, var, strength, n_frames, n_valid, n_oor,
     tilt, pitch, roll, imu_t, lidar_t, vb0, vb1) = f[5:18]

    def i16_or_none(v, scale):
        return None if v == -32768 else v / scale

    return {
        "seq": seq,
        "epoch": epoch,
        "time_utc": datetime.fromtimestamp(epoch, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        if epoch else "",
        "flags": "|".join(n for b, n in FLAGS.items() if flags & (1 << b)),
        "dist_cm": None if dist == 0xFFFF else dist,
        "var_cm2": None if var == 0xFFFF else var,
        "strength": strength,
        "n_frames": n_frames,
        "n_valid": n_valid,
        "n_oor": n_oor,
        "tilt_deg": i16_or_none(tilt, 100),
        "pitch_deg": i16_or_none(pitch, 100),
        "roll_deg": i16_or_none(roll, 100),
        "imu_temp_c": i16_or_none(imu_t, 10),
        "lidar_temp_c": i16_or_none(lidar_t, 10),
        "vbat_start_mv": vb0,
        "vbat_end_mv": vb1,
    }


def iter_records(data: bytes, name: str = "<data>"):
    i = 0
    while i + RECORD_SIZE <= len(data):
        r = decode(data[i:i + RECORD_SIZE])
        if r is not None:
            yield r
            i += RECORD_SIZE
            continue
        # resync on the next magic word
        j = data.find(b"SG", i + 1)
        print(f"{name}: bad record at offset {i}, resyncing", file=sys.stderr)
        if j < 0:
            break
        i = j
    if i < len(data):
        print(f"{name}: {len(data) - i} trailing bytes ignored", file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*", help="record files (rec_*.bin)")
    ap.add_argument("--hex", help="hex string of one or more records")
    args = ap.parse_args()

    sources = []
    if args.hex:
        sources.append(("<hex>", bytes.fromhex(args.hex.replace(" ", ""))))
    for fn in args.files:
        with open(fn, "rb") as fh:
            sources.append((fn, fh.read()))
    if not sources:
        ap.error("give record files or --hex")

    print(",".join(HEADER))
    n = 0
    for name, data in sources:
        for r in iter_records(data, name):
            print(",".join("" if r[k] is None else str(r[k]) for k in HEADER))
            n += 1
    print(f"{n} records", file=sys.stderr)


if __name__ == "__main__":
    main()
