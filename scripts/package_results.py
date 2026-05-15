#!/usr/bin/env python3
import argparse
import csv
import hashlib
import json
import os
import tarfile
from datetime import datetime, timezone
from pathlib import Path


def safe_ref(value):
    return "".join(char if char.isalnum() or char in "._-" else "-" for char in value)


def read_metadata(path):
    if not path.exists():
        return {}

    metadata = {}
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.reader(handle):
            if len(row) >= 2:
                metadata[row[0]] = row[1]
    return metadata


def file_checksums(result_dir):
    checksums = {}
    for path in sorted(result_dir.iterdir()):
        if path.is_file() and path.name != "manifest.json":
            checksums[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    return checksums


def write_manifest(result_dir, unilink_ref, platform_suffix, reference_platform):
    metadata = read_metadata(result_dir / "latency_matrix.csv.meta")
    manifest = {
        "schema_version": 1,
        "created_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "unilink_ref": unilink_ref,
        "unilink_commit": metadata.get("unilink_commit"),
        "unilink_source_kind": metadata.get("unilink_source_kind"),
        "platform_suffix": platform_suffix,
        "reference_platform": reference_platform,
        "benchmark_repo_commit": os.environ.get("GITHUB_SHA"),
        "github_repository": os.environ.get("GITHUB_REPOSITORY"),
        "github_run_id": os.environ.get("GITHUB_RUN_ID"),
        "github_run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT"),
    }

    manifest_path = result_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    manifest["files"] = file_checksums(result_dir)
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return manifest_path


def create_tarball(result_dir, output_dir, asset_base):
    output_dir.mkdir(parents=True, exist_ok=True)
    tarball = output_dir / f"{asset_base}.tar.gz"
    with tarfile.open(tarball, "w:gz") as archive:
        for path in sorted(result_dir.iterdir()):
            if path.is_file():
                archive.add(path, arcname=path.name)
    return tarball


def write_sha256(path):
    checksum_path = path.with_name(path.name + ".sha256")
    checksum = hashlib.sha256(path.read_bytes()).hexdigest()
    checksum_path.write_text(f"{checksum}  {path.name}\n", encoding="utf-8")
    return checksum_path


def append_github_output(path, values):
    if not path:
        return
    with Path(path).open("a", encoding="utf-8") as handle:
        for key, value in values.items():
            handle.write(f"{key}={value}\n")


def main():
    parser = argparse.ArgumentParser(description="Package unilink benchmark results for artifact or release upload.")
    parser.add_argument("--result-dir", default="build/release-results")
    parser.add_argument("--output-dir", default="build/package")
    parser.add_argument("--unilink-ref", required=True)
    parser.add_argument("--platform-suffix", default="linux-x64-self-hosted")
    parser.add_argument("--reference-platform", default="")
    parser.add_argument("--github-output", default=os.environ.get("GITHUB_OUTPUT"))
    args = parser.parse_args()

    result_dir = Path(args.result_dir)
    if not result_dir.is_dir():
        raise SystemExit(f"result directory does not exist: {result_dir}")

    write_manifest(result_dir, args.unilink_ref, args.platform_suffix, args.reference_platform)
    asset_base = f"unilink-{safe_ref(args.unilink_ref)}-{safe_ref(args.platform_suffix)}"
    tarball = create_tarball(result_dir, Path(args.output_dir), asset_base)
    checksum = write_sha256(tarball)

    outputs = {
        "asset_base": asset_base,
        "tarball": str(tarball),
        "checksum": str(checksum),
    }
    append_github_output(args.github_output, outputs)

    for key, value in outputs.items():
        print(f"{key}={value}")


if __name__ == "__main__":
    main()
