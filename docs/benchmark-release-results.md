# Benchmark Release Results

This document describes how to record benchmark results for released `unilink` versions.

The preferred storage model is:

- GitHub Release assets for official `unilink` version baselines.
- GitHub Actions artifacts for commit-level experiments.
- Repository commits only for documentation and tooling, not routine raw benchmark output.

## Why Release Assets

Benchmark result files are generated data. Keeping every run in Git history makes the repository noisier without making
the benchmark code easier to maintain.

Release assets are a better fit for official version baselines because they are:

- attached to a clear version-oriented release record;
- easy to replace if a benchmark run was invalid;
- separate from source code history;
- downloadable as a complete result package;
- suitable for larger result files if future benchmarks store more raw data.

## Release Tag Naming

Use a benchmark-specific release tag so it is clear that the release belongs to this benchmark repository, not the core
`unilink` library package.

For `unilink` 0.7.2:

```text
benchmark-unilink-v0.7.2
```

For a Jetson Orin Nano Super reference result:

```text
benchmark-unilink-v0.7.2-jetson-orin-nano-super
```

Recommended release title:

```text
Benchmark results for unilink v0.7.2
```

## Self-Hosted Runner

Use a stable self-hosted runner for baseline results. Public GitHub-hosted runners are useful for build checks, but they
are not stable enough for benchmark baselines because hardware and host load are outside our control.

Recommended runner labels:

```json
["self-hosted","Linux","X64"]
```

For Jetson Orin Nano Super, register the runner with ARM64 and a board-specific label:

```json
["self-hosted","Linux","ARM64","jetson-orin-nano-super"]
```

The runner should have:

- CMake;
- a C++17-capable compiler;
- Git;
- Python 3;
- Bash;
- GitHub CLI (`gh`) if release uploads should happen inside the workflow.

If `gh` is unavailable, the workflow artifact still contains the complete benchmark result package.

## Running the Workflow

Open:

```text
Actions -> Benchmark Release -> Run workflow
```

For the `unilink` 0.7.2 baseline, use:

```text
unilink_ref: v0.7.2
runner_labels: ["self-hosted","Linux","X64"]
publish_release: true
payload_sizes: 64 256 1024 4096 16384 65536
repeats: 3
iterations: 10000
warmup_iterations: 1000
udp_max_payload_size: 1024
strategy_duration: 3
```

For a Jetson Orin Nano Super reference baseline, use:

```text
unilink_ref: v0.7.2
runner_labels: ["self-hosted","Linux","ARM64","jetson-orin-nano-super"]
platform_suffix: linux-arm64-jetson-orin-nano-super
release_suffix: jetson-orin-nano-super
reference_platform: Jetson Orin Nano Super
publish_release: true
payload_sizes: 64 256 1024 4096 16384 65536
repeats: 3
iterations: 10000
warmup_iterations: 1000
udp_max_payload_size: 1024
strategy_duration: 3
```

The workflow will:

1. check out `unilink-benchmarks`;
2. configure the benchmark build with `UNILINK_BENCH_UNILINK_GIT_TAG=<unilink_ref>`;
3. build in Release mode;
4. collect runner environment and hardware metadata;
5. run the latency matrix;
6. run the strategy sweep;
7. generate human-readable release notes from the summary files and runner metadata;
8. package the result files into a tarball;
9. upload the tarball as a workflow artifact;
10. upload the tarball to a GitHub Release when `publish_release=true` and `unilink_ref` starts with `v`;
11. create or update the GitHub Release body with the generated summary.

The default UDP latency payload cap is 1024 bytes. This keeps the `unilink` 0.7.2 baseline runnable because UDP echo
payloads larger than 1024 bytes do not reliably return in this benchmark model. Set `udp_max_payload_size` to `0` only
when intentionally validating a version or environment where larger UDP datagrams are expected to work.

For Jetson baseline runs, keep power mode, clock state, cooling, and background system load controlled. The environment
collector records Jetson-specific information when available, including device-tree model, L4T release, `nvpmodel -q`,
`jetson_clocks --show`, and thermal zone readings.

## Commit SHA Runs

Commit SHA runs are useful when validating a candidate fix or comparing an unreleased change.

For those runs, set:

```text
unilink_ref: <commit-sha>
publish_release: false
```

Keep these as workflow artifacts unless the result becomes part of a release baseline decision.

## Release Asset Contents

The packaged asset name uses this pattern:

```text
unilink-<ref>-linux-x64-self-hosted.tar.gz
unilink-<ref>-linux-x64-self-hosted.tar.gz.sha256
```

For example:

```text
unilink-v0.7.2-linux-x64-self-hosted.tar.gz
unilink-v0.7.2-linux-x64-self-hosted.tar.gz.sha256
```

For Jetson Orin Nano Super:

```text
unilink-v0.7.2-linux-arm64-jetson-orin-nano-super.tar.gz
unilink-v0.7.2-linux-arm64-jetson-orin-nano-super.tar.gz.sha256
```

The tarball contains:

```text
latency_matrix.csv
latency_matrix_summary.md
latency_matrix.csv.meta
strategy_sweep.csv
strategy_sweep_summary.md
strategy_sweep.csv.meta
udp_payload_smoke.csv
udp_payload_smoke_summary.md
environment.txt
hardware.json
manifest.json
release_notes.md
```

The GitHub Release body is generated from `release_notes.md`. It includes the benchmark target, resolved commit,
self-hosted runner hardware summary, latency summary table, strategy summary table, UDP payload smoke summary, and run
notes.

## Metadata Files

`manifest.json` is the top-level machine-readable record. It includes:

- requested `unilink` ref;
- resolved `unilink` commit when available;
- benchmark repository commit;
- GitHub run id;
- platform suffix;
- reference platform;
- SHA-256 checksums for packaged result files.

`environment.txt` is a human-readable summary of the runner.

`hardware.json` is the machine-readable runner environment record. It includes:

- hostname;
- OS and kernel;
- architecture;
- CPU model;
- logical CPU count;
- estimated physical core count;
- threads per core;
- socket count;
- NUMA node count when available;
- CPU cache information when available;
- total memory;
- compiler version;
- CMake version;
- Git version;
- GitHub runner name, OS, architecture, run id, and commit SHA.
- Jetson model, L4T release, power mode, clock status, and thermal zones when available.

Hardware metadata is required because benchmark results are only directly comparable when the runner hardware and software
environment are the same or intentionally controlled.

## Result Interpretation

Use release results as baseline measurements, not absolute guarantees.

For latency benchmarks, compare:

- `p50` for the normal case;
- `p95` and `p99` for tail latency;
- `max` only as a noisy outlier signal;
- `messages/sec` and `MiB/sec` for throughput under the echo model.

For strategy benchmarks, compare:

- accepted throughput;
- received throughput;
- delivery rate;
- failed sends.

`Reliable` and `BestEffort` are not expected to produce the same shape of result. `Reliable` prioritizes delivery under
pressure, while `BestEffort` prioritizes non-blocking sends and may drop data.

## When to Re-Run

Re-run a release baseline when:

- the original run used the wrong `unilink_ref`;
- the self-hosted runner was under unusual load;
- the benchmark code changed in a way that affects measurement;
- runner hardware, OS, compiler, or kernel changed;
- the result package is missing metadata.

If a release asset is replaced, keep the same release tag and use `gh release upload --clobber` through the workflow.

## Repository Policy

Do not routinely commit generated benchmark CSV files to the repository.

Use this rule of thumb:

- official version result: GitHub Release asset;
- commit-level experiment: GitHub Actions artifact;
- documentation/index of available baselines: repository docs;
- benchmark code or scripts: repository source.
