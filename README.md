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

Override with environment variables:

```bash
PAYLOAD_SIZES="64 1024 65536" REPEATS=5 ITERATIONS=50000 WARMUP_ITERATIONS=1000 \
  ./scripts/run_latency_matrix.sh
```

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

Use the strategy benchmark to compare accepted throughput, received throughput, delivery rate, and failed sends.
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

## Saving Results

Generated files under `build/` are local artifacts and are ignored by Git.

To keep a benchmark snapshot in the repository, write results under `results/`:

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

## Script Configuration

Latency matrix environment variables:

| Variable | Default | Meaning |
|----------|---------|---------|
| `PAYLOAD_SIZES` | `64 256 1024 4096 16384 65536` | Payload sizes to test |
| `TRANSPORTS` | `tcp udp uds` | Transports to test |
| `REPEATS` | `3` | Number of repeated runs per payload/transport |
| `ITERATIONS` | `10000` | Measured latency iterations per run |
| `WARMUP_ITERATIONS` | `1000` | Unmeasured warmup iterations per run |
| `OUTPUT` | `build/latency_matrix.csv` | Raw CSV output path |
| `SUMMARY` | `build/latency_matrix_summary.md` | Median summary output path |

Strategy sweep environment variables:

| Variable | Default | Meaning |
|----------|---------|---------|
| `PAYLOAD_SIZES` | `64 256 1024 4096 16384 65536` | Payload sizes to test |
| `REPEATS` | `3` | Number of repeated runs per payload |
| `DURATION` | `3` | Seconds per strategy run |
| `OUTPUT` | `build/strategy_sweep.csv` | Raw CSV output path |
| `SUMMARY` | `build/strategy_sweep_summary.md` | Median summary output path |

## Metrics

Each benchmark reports:

- payload size
- iterations
- elapsed time
- messages/sec
- MiB/sec
- latency min / avg / p50 / p95 / p99 / max

The strategy benchmark reports:

- accepted MiB/sec
- received MiB/sec
- delivery rate
- failed sends

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
