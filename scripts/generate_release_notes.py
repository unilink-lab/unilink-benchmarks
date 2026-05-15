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


def value_or_unknown(value):
    return value if value not in (None, "", "None") else "unknown"


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
