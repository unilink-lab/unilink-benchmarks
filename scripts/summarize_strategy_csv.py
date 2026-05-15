#!/usr/bin/env python3
import csv
import statistics
import sys
from collections import defaultdict


FIELDS = [
    "accepted_mib_sec",
    "received_mib_sec",
    "delivery_rate",
    "failed_sends",
]


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: summarize_strategy_csv.py <strategy.csv>", file=sys.stderr)
        return 2

    groups = defaultdict(lambda: defaultdict(list))
    with open(sys.argv[1], newline="") as f:
        for row in csv.DictReader(f):
            key = (row["transport"], row["strategy"], int(row["payload_size"]))
            for field in FIELDS:
                groups[key][field].append(float(row[field]))

    print("| transport | strategy | payload | runs | accepted MiB/s | received MiB/s | delivery % | failed sends |")
    print("|-----------|----------|---------|------|----------------|----------------|------------|--------------|")
    for (transport, strategy, payload), values in sorted(groups.items()):
        runs = len(values["accepted_mib_sec"])
        med = {field: statistics.median(values[field]) for field in FIELDS}
        print(
            f"| {transport} | {strategy} | {payload} | {runs} | {med['accepted_mib_sec']:.2f} | "
            f"{med['received_mib_sec']:.2f} | {med['delivery_rate']:.2f} | {med['failed_sends']:.0f} |"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
