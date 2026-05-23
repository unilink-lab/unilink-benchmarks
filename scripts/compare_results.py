#!/usr/bin/env python3
import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path


LATENCY_FIELDS = [
    ("messages_sec", "msg/s"),
    ("mib_sec", "MiB/s"),
    ("p50_us", "p50 us"),
    ("p95_us", "p95 us"),
    ("p99_us", "p99 us"),
    ("max_us", "max us"),
]

STRATEGY_FIELDS = [
    ("accepted_mib_sec", "accepted MiB/s"),
    ("received_mib_sec", "received MiB/s"),
    ("delivery_rate", "delivery %"),
    ("failed_sends", "failed sends"),
    ("dropped_messages", "dropped messages"),
    ("dropped_bytes", "dropped bytes"),
]


def read_csv(path):
    if not path.exists():
        return []
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def parse_float(value):
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def median_or_none(rows, field):
    values = []
    for row in rows:
        value = parse_float(row.get(field))
        if value is not None:
            values.append(value)
    if not values:
        return None
    return statistics.median(values)


def group_rows(rows, key_fields):
    groups = defaultdict(list)
    for row in rows:
        try:
            key = tuple(row[field] for field in key_fields)
        except KeyError:
            continue
        groups[key].append(row)
    return groups


def format_value(value, precision=2):
    if value is None:
        return "n/a"
    if abs(value) >= 1000 or value == int(value):
        return f"{value:.0f}"
    return f"{value:.{precision}f}"


def format_delta(baseline, candidate, lower_is_better):
    if baseline is None or candidate is None:
        return "n/a"
    if baseline == 0:
        return "n/a"
    delta = ((candidate - baseline) / baseline) * 100.0
    marker = ""
    if delta != 0:
        improved = delta < 0 if lower_is_better else delta > 0
        marker = " better" if improved else " worse"
    return f"{delta:+.1f}%{marker}"


def latency_summary(baseline_rows, candidate_rows):
    baseline = group_rows(baseline_rows, ("transport", "payload_size"))
    candidate = group_rows(candidate_rows, ("transport", "payload_size"))
    keys = sorted(set(baseline) | set(candidate), key=lambda key: (key[0], int(key[1])))

    lines = [
        "## Latency Changes",
        "",
        "| transport | payload | metric | baseline | candidate | delta |",
        "|---|---:|---|---:|---:|---:|",
    ]
    for key in keys:
        transport, payload = key
        for field, label in LATENCY_FIELDS:
            base_value = median_or_none(baseline.get(key, []), field)
            cand_value = median_or_none(candidate.get(key, []), field)
            lower_is_better = field.endswith("_us")
            lines.append(
                f"| {transport} | {payload} | {label} | {format_value(base_value)} | "
                f"{format_value(cand_value)} | {format_delta(base_value, cand_value, lower_is_better)} |"
            )
    return lines


def strategy_summary(baseline_rows, candidate_rows):
    baseline = group_rows(baseline_rows, ("transport", "strategy", "payload_size"))
    candidate = group_rows(candidate_rows, ("transport", "strategy", "payload_size"))
    keys = sorted(set(baseline) | set(candidate), key=lambda key: (key[0], key[1], int(key[2])))

    lines = [
        "## Strategy Changes",
        "",
        "| transport | strategy | payload | metric | baseline | candidate | delta |",
        "|---|---|---:|---|---:|---:|---:|",
    ]
    for key in keys:
        transport, strategy, payload = key
        for field, label in STRATEGY_FIELDS:
            base_value = median_or_none(baseline.get(key, []), field)
            cand_value = median_or_none(candidate.get(key, []), field)
            if base_value is None and cand_value is None:
                continue
            lower_is_better = field in {"failed_sends", "dropped_messages", "dropped_bytes"}
            lines.append(
                f"| {transport} | {strategy} | {payload} | {label} | {format_value(base_value)} | "
                f"{format_value(cand_value)} | {format_delta(base_value, cand_value, lower_is_better)} |"
            )
    return lines


def main():
    parser = argparse.ArgumentParser(description="Compare two unilink benchmark result directories.")
    parser.add_argument("--baseline", required=True, help="Baseline result directory")
    parser.add_argument("--candidate", required=True, help="Candidate result directory")
    parser.add_argument("--baseline-label", default="")
    parser.add_argument("--candidate-label", default="")
    parser.add_argument("--output", required=True, help="Markdown output path")
    args = parser.parse_args()

    baseline_dir = Path(args.baseline)
    candidate_dir = Path(args.candidate)
    baseline_label = args.baseline_label or baseline_dir.name
    candidate_label = args.candidate_label or candidate_dir.name

    baseline_latency = read_csv(baseline_dir / "latency_matrix.csv")
    candidate_latency = read_csv(candidate_dir / "latency_matrix.csv")
    baseline_strategy = read_csv(baseline_dir / "strategy_sweep.csv")
    candidate_strategy = read_csv(candidate_dir / "strategy_sweep.csv")

    lines = [
        "# unilink Benchmark Comparison",
        "",
        f"Baseline: `{baseline_label}`  ",
        f"Candidate: `{candidate_label}`",
        "",
        "Compare only runs from the same runner and environment.",
        "",
        "Positive latency delta means slower. Positive throughput delta means faster.",
        "",
    ]
    lines.extend(latency_summary(baseline_latency, candidate_latency))
    lines.append("")
    lines.extend(strategy_summary(baseline_strategy, candidate_strategy))
    lines.extend(
        [
            "",
            "## Notes",
            "",
            "- Missing metrics are shown as `n/a`.",
            "- RuntimeStats-derived columns are compared when present in the input CSV files.",
            "- Delivery and drop metrics need protocol-specific interpretation, especially for BestEffort and UDP.",
        ]
    )

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")
    print(f"summary: {output}")


if __name__ == "__main__":
    main()
