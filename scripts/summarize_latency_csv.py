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
    "p99_9_us",
    "max_us",
]


def outlier_label(field):
    value = field.removeprefix("outliers_over_").removesuffix("us")
    try:
        threshold_us = int(value)
    except ValueError:
        return field
    if threshold_us % 1000 == 0:
        return f">{threshold_us // 1000}ms"
    return f">{threshold_us}us"


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: summarize_latency_csv.py <latency.csv>", file=sys.stderr)
        return 2

    groups = defaultdict(lambda: defaultdict(list))
    with open(sys.argv[1], newline="") as f:
        reader = csv.DictReader(f)
        outlier_fields = [
            field for field in (reader.fieldnames or []) if field.startswith("outliers_over_") and field.endswith("us")
        ]
        fields = FIELDS + outlier_fields
        for row in reader:
            key = (row["transport"], int(row["payload_size"]))
            for field in fields:
                groups[key][field].append(float(row[field]))

    outlier_headers = [outlier_label(field) for field in outlier_fields]
    headers = [
        "transport",
        "payload",
        "runs",
        "msg/s median",
        "MiB/s median",
        "p50 us",
        "p95 us",
        "p99 us",
        "p99.9 us",
        "max us",
        *outlier_headers,
    ]
    aligns = ["---", "---:", "---:", "---:", "---:", "---:", "---:", "---:", "---:", "---:", *(["---:"] * len(outlier_headers))]
    print("| " + " | ".join(headers) + " |")
    print("|" + "|".join(aligns) + "|")
    for (transport, payload), values in sorted(groups.items()):
        runs = len(values["messages_sec"])
        med = {field: statistics.median(values[field]) for field in fields}
        row = [
            transport,
            str(payload),
            str(runs),
            f"{med['messages_sec']:.0f}",
            f"{med['mib_sec']:.2f}",
            f"{med['p50_us']:.0f}",
            f"{med['p95_us']:.0f}",
            f"{med['p99_us']:.0f}",
            f"{med['p99_9_us']:.0f}",
            f"{med['max_us']:.0f}",
            *[f"{med[field]:.0f}" for field in outlier_fields],
        ]
        print("| " + " | ".join(row) + " |")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
