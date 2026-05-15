#!/usr/bin/env python3
import csv
import statistics
import sys
from collections import defaultdict


FIELDS = [
    "messages_sec",
    "mib_sec",
    "p50_us",
    "p95_us",
    "p99_us",
    "max_us",
]


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: summarize_latency_csv.py <latency.csv>", file=sys.stderr)
        return 2

    groups = defaultdict(lambda: defaultdict(list))
    with open(sys.argv[1], newline="") as f:
        for row in csv.DictReader(f):
            key = (row["transport"], int(row["payload_size"]))
            for field in FIELDS:
                groups[key][field].append(float(row[field]))

    print("| transport | payload | runs | msg/s median | MiB/s median | p50 us | p95 us | p99 us | max us |")
    print("|-----------|---------|------|--------------|--------------|--------|--------|--------|--------|")
    for (transport, payload), values in sorted(groups.items()):
        runs = len(values["messages_sec"])
        med = {field: statistics.median(values[field]) for field in FIELDS}
        print(
            f"| {transport} | {payload} | {runs} | {med['messages_sec']:.0f} | "
            f"{med['mib_sec']:.2f} | {med['p50_us']:.0f} | {med['p95_us']:.0f} | "
            f"{med['p99_us']:.0f} | {med['max_us']:.0f} |"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
