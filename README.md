# unilink-benchmarks

Standalone performance benchmarks for unilink.

This repository measures latency, throughput, and concurrency behavior across supported unilink transports.

## What is unilink?

unilink is a modern C++ communication library that provides a unified interface for serial, network, and local IPC transports.

Core library:
https://github.com/jwsung91/unilink

## Scope

This repository is separate from the core library to keep the main `unilink` release and package flow lightweight.

The first benchmark suite focuses on local echo latency tests for:

- TCP
- UDP
- Unix Domain Socket

Serial benchmarks may be added later as optional hardware-dependent benchmarks.

TCP and UDS latency benchmarks use a small length header internally so each round trip measures one complete payload
even though those transports are byte streams.

## Build

### Quick start with a local unilink checkout

```bash
cmake -S . -B build \
  -DUNILINK_BENCH_USE_FETCHCONTENT=ON \
  -DUNILINK_BENCH_UNILINK_SOURCE_DIR=$HOME/workspace/jwsung91/unilink \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Then run:

```bash
./scripts/run_latency_matrix.sh
./scripts/run_strategy_sweep.sh
```

### Option 1: Use installed unilink package

```bash
cmake -S . -B build -DUNILINK_BENCH_USE_FETCHCONTENT=OFF
cmake --build build
```

If `unilink` is installed in a non-standard prefix, pass either `CMAKE_PREFIX_PATH` or `unilink_DIR`:

```bash
cmake -S . -B build \
  -DUNILINK_BENCH_USE_FETCHCONTENT=OFF \
  -Dunilink_DIR=/path/to/install/lib/cmake/unilink \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Option 2: Fetch unilink from source

```bash
cmake -S . -B build -DUNILINK_BENCH_USE_FETCHCONTENT=ON
cmake --build build
```

Specify a released version tag or commit id for reproducible benchmark runs:

```bash
cmake -S . -B build \
  -DUNILINK_BENCH_USE_FETCHCONTENT=ON \
  -DUNILINK_BENCH_UNILINK_GIT_TAG=v0.7.2
cmake --build build
```

```bash
cmake -S . -B build \
  -DUNILINK_BENCH_USE_FETCHCONTENT=ON \
  -DUNILINK_BENCH_UNILINK_GIT_TAG=<commit-id>
cmake --build build
```

For local development against a checked-out `unilink` source tree:

```bash
cmake -S . -B build -DUNILINK_BENCH_USE_FETCHCONTENT=ON \
  -DUNILINK_BENCH_UNILINK_SOURCE_DIR=/path/to/unilink
cmake --build build
```

Benchmark sweep metadata records the requested unilink ref and the resolved commit when available.

## Run

Binaries are generated under:

```text
build/bin
```

Example:

```bash
./build/bin/bench_tcp_echo_server --port 9000
./build/bin/bench_tcp_latency_client --host 127.0.0.1 --port 9000 --payload-size 1024 --iterations 100000
```

Or use scripts:

```bash
./scripts/run_tcp_latency.sh
./scripts/run_udp_latency.sh
./scripts/run_uds_latency.sh
./scripts/run_all_local.sh
```

Latency clients also support warmup iterations and CSV output:

```bash
./build/bin/bench_tcp_latency_client \
  --host 127.0.0.1 \
  --port 9000 \
  --payload-size 1024 \
  --iterations 100000 \
  --warmup-iterations 1000 \
  --csv-output results.csv
```

For repeated payload-size sweeps, use:

```bash
./scripts/run_latency_matrix.sh
```

Defaults:

- payload sizes: `64 256 1024 4096 16384 65536`
- repeats: `3`
- measured iterations per run: `10000`
- warmup iterations per run: `1000`
- UDP latency payload cap: `1024`
- outlier thresholds: `5000 10000 50000` us

Override with environment variables:

```bash
PAYLOAD_SIZES="64 1024 65536" REPEATS=5 ITERATIONS=50000 WARMUP_ITERATIONS=1000 \
  ./scripts/run_latency_matrix.sh
```

Configure tail-latency outlier thresholds in microseconds:

```bash
OUTLIER_THRESHOLDS_US="5000 10000 50000" ./scripts/run_latency_matrix.sh
```

UDP latency defaults to payloads up to 1024 bytes because `unilink` 0.7.2 UDP echo does not reliably return larger
payloads in this benchmark model. Set `UDP_MAX_PAYLOAD_SIZE=0` to disable the cap for versions or environments where
larger UDP datagrams are expected to work.

The matrix script writes CSV, a median summary table, and a small metadata file under `build/`.

Default output files:

```text
build/latency_matrix.csv
build/latency_matrix_summary.md
build/latency_matrix.csv.meta
```

## Backpressure Strategy

unilink supports two backpressure strategies:

- `Reliable`: waits for queue pressure to clear and prioritizes delivery.
- `BestEffort`: avoids blocking and may drop data under pressure.

Use the strategy benchmark to compare accepted throughput, received throughput, delivery rate, failed sends, and
RuntimeStats-derived queue/drop metrics when the selected `unilink` ref supports them.
This is a one-way streaming benchmark, not an echo latency benchmark:

```bash
./scripts/run_strategy_matrix.sh
```

Or run it directly:

```bash
./build/bin/bench_strategy_matrix --payload-size 1024 --duration 3 --csv-output strategy.csv
```

For repeated strategy sweeps across payload sizes:

```bash
./scripts/run_strategy_sweep.sh
```

Override with:

```bash
PAYLOAD_SIZES="64 1024 65536" REPEATS=5 DURATION=5 ./scripts/run_strategy_sweep.sh
```

Default output files:

```text
build/strategy_sweep.csv
build/strategy_sweep_summary.md
build/strategy_sweep.csv.meta
```

## UDP Payload Smoke

Use the UDP payload smoke benchmark to distinguish large-datagram echo behavior from strategy pressure benchmark
behavior:

```bash
./scripts/run_udp_payload_smoke.sh
```

Defaults:

- payload sizes: `1024 4096 16384`
- measured iterations: `1000`
- warmup iterations: `100`
- timeout: `1000 ms`
- strategies: `Reliable` and `BestEffort`

Output files:

```text
build/udp_payload_smoke.csv
build/udp_payload_smoke_summary.md
```

If strategy benchmarks report zero delivery for larger UDP payloads, run this smoke benchmark in the same environment
to classify whether single/few datagram echo works before changing the core library.

## Saving Results

Generated files under `build/` are local artifacts and are ignored by Git.

For ad-hoc local snapshots, write results under `results/`:

```bash
RESULT_DIR=results/local/$(date -u +%Y-%m-%d)
mkdir -p "${RESULT_DIR}"

OUTPUT="${RESULT_DIR}/latency_matrix.csv" \
SUMMARY="${RESULT_DIR}/latency_matrix_summary.md" \
./scripts/run_latency_matrix.sh

OUTPUT="${RESULT_DIR}/strategy_sweep.csv" \
SUMMARY="${RESULT_DIR}/strategy_sweep_summary.md" \
./scripts/run_strategy_sweep.sh
```

Then commit the result snapshot:

```bash
git add "${RESULT_DIR}"
git commit -m "docs: add local benchmark results"
```

Each sweep also writes a `.meta` file containing environment and unilink source information, including the requested ref
and resolved commit when available.

For official release-version measurements, prefer GitHub Release assets instead of committing raw result files to the
repository. The `Benchmark Release` workflow is intended for a self-hosted runner:

```text
Actions -> Benchmark Release -> Run workflow
```

Recommended input for the `unilink` 0.7.2 baseline:

```text
unilink_ref: v0.7.2
publish_release: true
```

Recommended input for a Jetson Orin Nano Super reference baseline:

```text
unilink_ref: v0.7.2
runner_labels: ["self-hosted","Linux","ARM64","jetson-orin-nano-super"]
platform_suffix: linux-arm64-jetson-orin-nano-super
release_suffix: jetson-orin-nano-super
reference_platform: Jetson Orin Nano Super
publish_release: true
```

The workflow builds the benchmark suite against the requested `unilink` tag or commit, runs the latency and strategy
sweeps, uploads a workflow artifact, and uploads release assets when the ref looks like a release tag. Release tags use
this naming pattern:

```text
benchmark-unilink-v0.7.2
benchmark-unilink-v0.7.2-jetson-orin-nano-super
```

Each release artifact contains:

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

`environment.txt` is a human-readable runner summary. `hardware.json` records the self-hosted runner hardware and
environment, including CPU model, logical CPUs, estimated physical cores, cache information when available, total memory,
OS/kernel, compiler, CMake, Git, runner name, and GitHub run id.
On Jetson systems it also records device-tree model, L4T release, `nvpmodel -q`, `jetson_clocks --show`, and thermal
zone readings when available.

The GitHub Release body is generated from `release_notes.md` and includes the benchmark target, runner summary, latency
summary table, strategy summary table, UDP payload smoke summary, and run notes.

The release upload step uses the GitHub CLI (`gh`) on the self-hosted runner. If `gh` is not installed, the workflow
artifact is still the right fallback output to keep from the run.

Commit-id benchmark runs are useful for experiments, but they should usually stay as workflow artifacts rather than
release assets.

See [Benchmark Release Results](docs/benchmark-release-results.md) for the full self-hosted runner and release asset
workflow.

## Script Configuration

Latency matrix environment variables:

| Variable | Default | Meaning |
|----------|---------|---------|
| `PAYLOAD_SIZES` | `64 256 1024 4096 16384 65536` | Payload sizes to test |
| `TRANSPORTS` | `tcp udp uds` | Transports to test |
| `REPEATS` | `3` | Number of repeated runs per payload/transport |
| `ITERATIONS` | `10000` | Measured latency iterations per run |
| `WARMUP_ITERATIONS` | `1000` | Unmeasured warmup iterations per run |
| `UDP_MAX_PAYLOAD_SIZE` | `1024` | Skip UDP latency runs above this payload size; `0` disables the cap |
| `OUTLIER_THRESHOLDS_US` | `5000 10000 50000` | Outlier thresholds in microseconds |
| `OUTPUT` | `build/latency_matrix.csv` | Raw CSV output path |
| `SUMMARY` | `build/latency_matrix_summary.md` | Median summary output path |

UDP latency script environment variables:

| Variable | Default | Meaning |
|----------|---------|---------|
| `SERVER_START_DELAY` | `0.2` | Seconds to wait after the UDP server process starts before running the client |

Strategy sweep environment variables:

| Variable | Default | Meaning |
|----------|---------|---------|
| `PAYLOAD_SIZES` | `64 256 1024 4096 16384 65536` | Payload sizes to test |
| `REPEATS` | `3` | Number of repeated runs per payload |
| `DURATION` | `3` | Seconds per strategy run |
| `OUTPUT` | `build/strategy_sweep.csv` | Raw CSV output path |
| `SUMMARY` | `build/strategy_sweep_summary.md` | Median summary output path |

UDP payload smoke environment variables:

| Variable | Default | Meaning |
|----------|---------|---------|
| `HOST` | `127.0.0.1` | UDP loopback host |
| `PORT` | `9401` | Base UDP server port |
| `PAYLOAD_SIZES` | `1024 4096 16384` | Payload sizes to test |
| `ITERATIONS` | `1000` | Measured sends per payload |
| `WARMUP_ITERATIONS` | `100` | Unmeasured warmup sends per payload |
| `TIMEOUT_MS` | `1000` | Echo timeout per send |
| `STRATEGY` | `both` | `reliable`, `besteffort`, or `both` |
| `OUTPUT` | `build/udp_payload_smoke.csv` | Raw CSV output path |
| `SUMMARY` | `build/udp_payload_smoke_summary.md` | Markdown summary output path |

## Metrics

Each benchmark reports:

- payload size
- iterations
- elapsed time
- messages/sec
- MiB/sec
- latency min / avg / p50 / p95 / p99 / p99.9 / max
- outlier counts over configured thresholds

Outlier counts use `latency_us > threshold_us`. Summary tables use the median across repeated runs, matching existing
latency summary behavior. These counts help distinguish one-off max spikes from repeated tail-latency behavior.

The strategy benchmark reports:

- accepted MiB/sec
- received MiB/sec
- delivery rate
- failed sends

The UDP payload smoke benchmark reports:

- send success
- server received count
- client echo received count
- echo match count
- timeout count
- delivery percentage
- match percentage
- dropped messages
- dropped bytes
- backpressure events
- max queued bytes
- final queued bytes

RuntimeStats columns are populated only when the selected `unilink` ref supports the `stats()` API. Older refs are still
supported; stats columns are marked as unavailable in the summary.

## Notes

Benchmark results may vary depending on:

- OS
- CPU
- scheduler behavior
- system load
- socket buffer size
- loopback network behavior
- compiler and build type

Use Release builds for meaningful measurements:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## License

Apache-2.0
