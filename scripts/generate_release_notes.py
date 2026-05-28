#!/usr/bin/env python3
import argparse
import csv
import json
from pathlib import Path


def read_key_value_file(path):
    values = {}
    if not path.exists():
        return values

    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        values[key.strip()] = value.strip()
    return values


def read_metadata(path):
    values = {}
    if not path.exists():
        return values

    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.reader(handle):
            if len(row) >= 2:
                values[row[0]] = row[1]
    return values


def read_json(path):
    if not path.exists():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def read_section(path, fallback):
    if not path.exists():
        return fallback
    content = path.read_text(encoding="utf-8").strip()
    return content if content else fallback


def strip_markdown_heading(content, heading):
    lines = content.splitlines()
    if lines and lines[0].strip() == heading:
        return "\n".join(lines[1:]).lstrip()
    return content


def value_or_unknown(value):
    return value if value not in (None, "", "None") else "unknown"


def parse_int_list(value):
    if not value:
        return []
    values = []
    for item in value.split():
        try:
            values.append(int(item))
        except ValueError:
            continue
    return values


def parse_transport_list(value):
    return [item for item in (value or "").split() if item]


def expected_latency_counts(metadata):
    payloads = parse_int_list(metadata.get("payload_sizes"))
    transports = parse_transport_list(metadata.get("transports"))
    try:
        repeats = int(metadata.get("repeats") or 0)
    except ValueError:
        repeats = 0
    try:
        udp_max_payload_size = int(metadata.get("udp_max_payload_size") or 0)
    except ValueError:
        udp_max_payload_size = 0

    expected = {}
    for payload in payloads:
        for transport in transports:
            if transport == "udp" and udp_max_payload_size != 0 and payload > udp_max_payload_size:
                continue
            expected[(transport, payload)] = repeats
    return expected


def read_latency_rows(path):
    if not path.exists():
        return []
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def numeric(row, field):
    try:
        return float(row.get(field, ""))
    except ValueError:
        return None


def latency_run_notes(result_dir, metadata):
    rows = read_latency_rows(result_dir / "latency_matrix.csv")
    if not rows:
        return []

    expected = expected_latency_counts(metadata)
    actual = {}
    for row in rows:
        try:
            key = (row["transport"], int(row["payload_size"]))
        except (KeyError, ValueError):
            continue
        actual[key] = actual.get(key, 0) + 1

    notes = []
    missing = []
    for key, expected_count in sorted(expected.items()):
        actual_count = actual.get(key, 0)
        if actual_count < expected_count:
            transport, payload = key
            missing.append(f"{transport}/{payload}: {actual_count}/{expected_count}")
    if missing:
        notes.append("- Latency matrix appears partial; completed runs: " + ", ".join(missing) + ".")

    anomaly_rows = []
    outlier_fields = [
        field for field in (rows[0].keys() if rows else []) if field.startswith("outliers_over_") and field.endswith("us")
    ]
    for row in rows:
        p99_9 = numeric(row, "p99_9_us")
        outlier_count = max((numeric(row, field) or 0 for field in outlier_fields), default=0)
        if (p99_9 is not None and p99_9 >= 5000) or outlier_count > 0:
            anomaly_rows.append(row)

    if anomaly_rows:
        notes.extend(
            [
                "- Latency anomaly threshold: rows with p99.9 >= 5000 us or any configured outlier count > 0.",
                "",
                "| transport | payload | p50 us | p99.9 us | max us | >5ms | >10ms | >50ms |",
                "|---|---:|---:|---:|---:|---:|---:|---:|",
            ]
        )
        for row in anomaly_rows[:20]:
            notes.append(
                "| "
                + " | ".join(
                    [
                        row.get("transport", ""),
                        row.get("payload_size", ""),
                        row.get("p50_us", ""),
                        row.get("p99_9_us", ""),
                        row.get("max_us", ""),
                        row.get("outliers_over_5000us", "0"),
                        row.get("outliers_over_10000us", "0"),
                        row.get("outliers_over_50000us", "0"),
                    ]
                )
                + " |"
            )
        if len(anomaly_rows) > 20:
            notes.append(f"- Additional anomalous latency rows omitted from notes: {len(anomaly_rows) - 20}.")

    return notes


def jetson_collection_notes(environment, hardware):
    notes = []
    jetson = hardware.get("hardware", {}).get("jetson", {})
    if not jetson.get("detected"):
        return notes

    clock_status = jetson.get("jetson_clocks_status") or environment.get("jetson_clocks_status")
    clock_error = jetson.get("jetson_clocks_error") or environment.get("jetson_clocks_error")
    if clock_status and clock_status != "ok":
        message = f"- Jetson clocks state was not confirmed (`{clock_status}`"
        if clock_error:
            message += f": {clock_error}"
        message += "); interpret latency and tail values with clock/throttling caution."
        notes.append(message)

    nvpmodel_status = jetson.get("nvpmodel_status") or environment.get("jetson_nvpmodel_status")
    nvpmodel_error = jetson.get("nvpmodel_error") or environment.get("jetson_nvpmodel_error")
    if nvpmodel_status and nvpmodel_status != "ok":
        message = f"- Jetson nvpmodel state was not confirmed (`{nvpmodel_status}`"
        if nvpmodel_error:
            message += f": {nvpmodel_error}"
        message += ")."
        notes.append(message)

    collection = hardware.get("collection_status", {})
    if collection.get("status") == "partial":
        notes.append("- Environment collection completed with non-fatal unavailable/error fields; see `hardware.json`.")

    return notes


def build_notes(result_dir, unilink_ref, platform_suffix, reference_platform):
    environment = read_key_value_file(result_dir / "environment.txt")
    metadata = read_metadata(result_dir / "latency_matrix.csv.meta")
    hardware = read_json(result_dir / "hardware.json")
    github_actions = hardware.get("github_actions", {})
    jetson = hardware.get("hardware", {}).get("jetson", {})

    unilink_commit = metadata.get("unilink_commit")
    benchmark_commit = environment.get("github_sha") or github_actions.get("github_sha")
    run_id = environment.get("github_run_id") or github_actions.get("github_run_id")
    platform_name = reference_platform or platform_suffix

    lines = [
        f"Self-hosted benchmark results for `unilink` `{unilink_ref}`.",
        "",
        "## Benchmark Target",
        "",
        f"- unilink ref: `{unilink_ref}`",
        f"- unilink commit: `{value_or_unknown(unilink_commit)}`",
        f"- reference platform: `{value_or_unknown(platform_name)}`",
        f"- platform suffix: `{value_or_unknown(platform_suffix)}`",
        f"- benchmark repo commit: `{value_or_unknown(benchmark_commit)}`",
        f"- GitHub run id: `{value_or_unknown(run_id)}`",
        f"- measured at UTC: `{value_or_unknown(environment.get('timestamp_utc'))}`",
        "",
        "## Runner Environment",
        "",
        f"- runner: `{value_or_unknown(environment.get('runner_name'))}`",
        f"- host: `{value_or_unknown(environment.get('hostname'))}`",
        f"- OS: `{value_or_unknown(environment.get('os'))}`",
        f"- kernel: `{value_or_unknown(environment.get('kernel'))}`",
        f"- architecture: `{value_or_unknown(environment.get('architecture'))}`",
        f"- CPU: `{value_or_unknown(environment.get('cpu_model'))}`",
        f"- logical CPUs: `{value_or_unknown(environment.get('logical_cpus'))}`",
        f"- estimated physical cores: `{value_or_unknown(environment.get('physical_cores_estimate'))}`",
        f"- memory KiB: `{value_or_unknown(environment.get('mem_total_kib'))}`",
        f"- compiler: `{value_or_unknown(environment.get('cxx'))}`",
        f"- CMake: `{value_or_unknown(environment.get('cmake'))}`",
    ]

    if jetson.get("detected"):
        lines.extend(
            [
                "",
                "## Jetson Environment",
                "",
                f"- Jetson model: `{value_or_unknown(jetson.get('model'))}`",
                f"- L4T release: `{value_or_unknown(jetson.get('l4t_release'))}`",
                f"- nvpmodel: `{value_or_unknown(environment.get('jetson_nvpmodel'))}`",
                f"- jetson_clocks: `{value_or_unknown(environment.get('jetson_clocks'))}`",
            ]
        )

    latency_notes = latency_run_notes(result_dir, metadata)
    if latency_notes:
        lines.extend(
            [
                "",
                "## Latency Run Notes",
                "",
                *latency_notes,
            ]
        )

    lines.extend(
        [
        "",
        "## Latency Summary",
        "",
        read_section(result_dir / "latency_matrix_summary.md", "_Latency summary was not generated._"),
        "",
        "## Strategy Summary",
        "",
        read_section(result_dir / "strategy_sweep_summary.md", "_Strategy summary was not generated._"),
        "",
        "## UDP Payload Smoke Summary",
        "",
        strip_markdown_heading(
            read_section(result_dir / "udp_payload_smoke_summary.md", "_UDP payload smoke summary was not generated._"),
            "# UDP Payload Smoke Summary",
        ),
        "",
        "## Notes",
        "",
        "- Full raw CSV, metadata, hardware record, manifest, and checksum are attached as release assets.",
        "- Latency values are round-trip local echo measurements in microseconds.",
        "- Strategy measurements are one-way streaming measurements.",
        ]
    )

    udp_cap = metadata.get("udp_max_payload_size")
    if udp_cap and udp_cap != "0":
        lines.append(f"- UDP latency payloads above `{udp_cap}` bytes were skipped for this run.")

    lines.extend(jetson_collection_notes(environment, hardware))

    return "\n".join(lines).rstrip() + "\n"


def main():
    parser = argparse.ArgumentParser(description="Generate GitHub Release notes for benchmark results.")
    parser.add_argument("--result-dir", default="build/release-results")
    parser.add_argument("--unilink-ref", required=True)
    parser.add_argument("--platform-suffix", default="linux-x64-self-hosted")
    parser.add_argument("--reference-platform", default="")
    parser.add_argument("--output", default="build/release-results/release_notes.md")
    args = parser.parse_args()

    result_dir = Path(args.result_dir)
    if not result_dir.is_dir():
        raise SystemExit(f"result directory does not exist: {result_dir}")

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        build_notes(result_dir, args.unilink_ref, args.platform_suffix, args.reference_platform),
        encoding="utf-8",
    )
    print(output)


if __name__ == "__main__":
    main()
