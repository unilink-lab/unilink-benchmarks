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
    "client_dropped_messages",
    "client_dropped_bytes",
    "client_backpressure_events",
    "client_max_queued_bytes",
]


def parse_float(value):
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def median_or_none(values):
    numeric = [value for value in values if value is not None]
    if not numeric:
        return None
    return statistics.median(numeric)


def runtime_stats_supported(values):
    return any(value and value > 0 for value in values)


def format_stat(value, suffix=""):
    if value is None:
        return "n/a"
    if value == int(value):
        return f"{value:.0f}{suffix}"
    return f"{value:.2f}{suffix}"


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: summarize_strategy_csv.py <strategy.csv>", file=sys.stderr)
        return 2

    groups = defaultdict(lambda: defaultdict(list))
    with open(sys.argv[1], newline="") as f:
        for row in csv.DictReader(f):
            key = (row["transport"], row["strategy"], int(row["payload_size"]))
            for field in FIELDS:
                groups[key][field].append(parse_float(row.get(field)))
            groups[key]["runtime_stats_supported"].append(parse_float(row.get("runtime_stats_supported")))

    print(
        "| transport | strategy | payload | runs | accepted MiB/s | received MiB/s | delivery % | failed sends | "
        "runtime stats | dropped msg | dropped MiB | max queue KiB | bp events |"
    )
    print(
        "|-----------|----------|---------|------|----------------|----------------|------------|--------------|"
        "---------------|-------------|-------------|---------------|-----------|"
    )
    for (transport, strategy, payload), values in sorted(groups.items()):
        runs = len(values["accepted_mib_sec"])
        med = {field: median_or_none(values[field]) for field in FIELDS}
        stats_supported = runtime_stats_supported(values["runtime_stats_supported"])
        dropped_mib = None
        max_queue_kib = None
        if stats_supported:
            if med["client_dropped_bytes"] is not None:
                dropped_mib = med["client_dropped_bytes"] / (1024.0 * 1024.0)
            if med["client_max_queued_bytes"] is not None:
                max_queue_kib = med["client_max_queued_bytes"] / 1024.0
        print(
            f"| {transport} | {strategy} | {payload} | {runs} | {med['accepted_mib_sec']:.2f} | "
            f"{med['received_mib_sec']:.2f} | {med['delivery_rate']:.2f} | {med['failed_sends']:.0f} | "
            f"{'yes' if stats_supported else 'n/a'} | "
            f"{format_stat(med['client_dropped_messages'] if stats_supported else None)} | "
            f"{format_stat(dropped_mib)} | {format_stat(max_queue_kib)} | "
            f"{format_stat(med['client_backpressure_events'] if stats_supported else None)} |"
        )
    print()
    print(
        "For BestEffort, accepted throughput alone can be misleading. Use received throughput, delivery rate, "
        "dropped messages/bytes, and max queued bytes together."
    )
    print()
    print(
        "For Reliable, dropped message counters should normally remain zero. Failed sends indicate payloads "
        "rejected before local acceptance."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
