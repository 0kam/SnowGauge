#!/usr/bin/env python3
"""Summarise a picowatt capture of a SnowGauge logging cycle.

    python3 tools/analyze_power.py capture.csv [--threshold-ma 1.0] [--interval-min 5]

Reads the CSV written by `picowatt-cli --cal` (column current_cal_A) and splits
it into the sleep baseline and the measurement bursts, then extrapolates the
battery life for the field schedule. Prints a Markdown-ready summary.
"""
import argparse
import csv
import statistics


def load(path):
    ts, cur, vbus = [], [], []
    with open(path) as f:
        for row in csv.DictReader(f):
            col = "current_cal_A" if "current_cal_A" in row else "current_A"
            ts.append(float(row["t_s"]))
            cur.append(float(row[col]))
            vbus.append(float(row["vbus_V"]))
    return ts, cur, vbus


def bursts(ts, cur, threshold_a, gap_s=1.0, min_s=0.2, win=12):
    """Runs above the threshold, on a smoothed signal so sample noise (sd ~0.15 mA
    at the quiet preset) cannot fake a burst. Dips shorter than gap_s are merged."""
    smooth, acc = [], 0.0
    for k, i in enumerate(cur):
        acc += i
        if k >= win:
            acc -= cur[k - win]
        smooth.append(acc / min(k + 1, win))
    out, start, last = [], None, None
    for t, i in zip(ts, smooth):
        if i > threshold_a:
            if start is None:
                start = t
            last = t
        elif start is not None and t - last > gap_s:
            out.append((start, last))
            start = None
    if start is not None:
        out.append((start, last))
    return [(a, b) for a, b in out if b - a >= min_s]


def charge_mah(ts, cur, t0, t1):
    """Trapezoidal integral over [t0, t1] in mAh."""
    total = 0.0
    for (ta, ia), (tb, ib) in zip(zip(ts, cur), zip(ts[1:], cur[1:])):
        if tb <= t0 or ta >= t1:
            continue
        total += (ia + ib) / 2 * (tb - ta)
    return total / 3.6  # A*s -> mAh


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--threshold-ma", type=float, default=1.0,
                    help="current above which the device counts as awake")
    ap.add_argument("--interval-min", type=float, default=5.0,
                    help="logging interval of the capture")
    ap.add_argument("--field-per-day", type=float, default=9.0,
                    help="measurements per day in the field (17:00-05:00 / 90 min)")
    ap.add_argument("--capacity-mah", type=float, default=3000.0,
                    help="usable cell capacity (L91 at low current)")
    args = ap.parse_args()

    ts, cur, vbus = load(args.csv)
    span = ts[-1] - ts[0]
    thr = args.threshold_ma / 1000
    bs = bursts(ts, cur, thr)
    awake = sum(b - a for a, b in bs)

    sleep_samples = [i for t, i in zip(ts, cur)
                     if all(not (a - 1 <= t <= b + 1) for a, b in bs)]
    sleep_ua = statistics.fmean(sleep_samples) * 1e6
    sleep_sd = statistics.stdev(sleep_samples) * 1e6 / len(sleep_samples) ** 0.5
    total_mah = charge_mah(ts, cur, ts[0], ts[-1])
    mean_ua = total_mah / (span / 3600) * 1000

    print(f"capture      : {span / 60:.1f} min, {len(ts)} samples, "
          f"vbus {statistics.fmean(vbus):.3f} V")
    print(f"sleep+adv    : {sleep_ua:.1f} uA (sem {sleep_sd:.1f}, "
          f"{len(sleep_samples)} samples)")
    print(f"bursts       : {len(bs)} in {span / 60:.1f} min "
          f"(expected {span / 60 / args.interval_min:.1f} at {args.interval_min:g} min)")
    if bs:
        durs = [b - a for a, b in bs]
        chgs = [charge_mah(ts, cur, a - 0.5, b + 0.5) for a, b in bs]
        peaks = [max(i for t, i in zip(ts, cur) if a <= t <= b) for a, b in bs]
        print(f"  duration   : {statistics.median(durs):.2f} s median "
              f"({min(durs):.2f}-{max(durs):.2f})")
        print(f"  charge     : {statistics.median(chgs) * 1000:.2f} uAh median "
              f"({min(chgs) * 1000:.2f}-{max(chgs) * 1000:.2f})")
        print(f"  peak       : {statistics.median(peaks) * 1000:.1f} mA median")
        print(f"  duty       : {awake / span * 100:.3f} %")
    print(f"mean current : {mean_ua:.1f} uA over the capture "
          f"({total_mah * 1000:.1f} uAh in {span / 60:.1f} min)")

    if bs:
        per_burst = statistics.median([charge_mah(ts, cur, a - 0.5, b + 0.5) for a, b in bs])
        field_mah_day = sleep_ua * 24 / 1000 + per_burst * args.field_per_day
        print(f"field ({args.field_per_day:g}/day): {field_mah_day:.3f} mAh/day "
              f"= {field_mah_day / 24 * 1000:.1f} uA average, "
              f"{args.capacity_mah / field_mah_day / 365:.1f} years on "
              f"{args.capacity_mah:.0f} mAh")


if __name__ == "__main__":
    main()
